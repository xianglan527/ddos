#include "proc.h"

#include "assert.h"
#include "config.h"
#include "elf.h"
#include "error.h"
#include "hash.h"
#include "printf.h"
#include "riscv.h"
#include "slab.h"
#include "spinlock.h"
#include "stdio.h"
#include "string.h"
#include "syscall.h"
#include "trap.h"
#include "virtio-blk.h"
#include "bitmap.h"

Cpu cpus[NCPU];
Proc *initproc;
List_entry proc_list;

#define HASH_SHIFT 10
#define HASH_LIST_SIZE (1 << HASH_SHIFT)
#define pid_hashfn(x) (hash64(x, HASH_SHIFT))

static List_entry hash_list[HASH_LIST_SIZE];

Proc *initproc = nullptr;

static ulong next_pid = 1;
static ulong nr_process = 0;
struct bitmap *kernel_stack_bitmap = nullptr;

// The struct members list_link, parent, hash_link, *cptr, *yptr,
// and *optr of a process can only be read or written while holding the proc_lock.

// to prevent deadlocks,the locking order must be procs_lock--->process lock.
Spinlock procs_lock;

Spinlock print_procs_lock;

extern char trampoline[];  // trampoline.S
extern char kernel_etext[];

void swtch(struct context *, struct context *);

int alloc_pid() {
    int pid;
    // acquire(&procs_lock);
    Proc *proc;
    List_entry *le = &proc_list;
    pid = next_pid;
repeat:
    if (++next_pid == MAX_PID) next_pid = 1;
    while ((le = list_next(le)) != &proc_list) {
        proc = le2proc(le, list_link);
        if (proc->pid == next_pid) goto repeat;
    }
    // release(&procs_lock);
    return pid;
}

void proc_init(void) {
    initlock(&procs_lock, "procs_lock");
    initlock(&print_procs_lock, "print_procs_lock");
    list_init(&proc_list);
    kernel_stack_bitmap = bitmap_init(2 * NPROC);
    for (int i = 0; i < HASH_LIST_SIZE; i++) { list_init(hash_list + i); }
}

void fork_ret(void) {
    release(&myproc()->lock);
    user_trap_ret();
}

pagetable_t *proc_pagetable(Proc *p) {
    pagetable_t *pagetable;
    pagetable = alloc_pagetable();
    if (pagetable == nullptr) return nullptr;
    mappages(pagetable, TRAMPOLINE, PGSIZE, (uint64_t)trampoline, PTE_R | PTE_X);

    mappages(pagetable, TRAPFRAME, PGSIZE, (uint64_t)(p->trapframe), PTE_R | PTE_W);
    return pagetable;
}

// static int setup_kstack(Proc *proc) {
//     Page *page = alloc_pages(KSTACKPAGE);
//     if (page != nullptr) {
//         proc->kstack = (uintptr_t)page2kva(page);
//         return 0;
//     }
//     return -E_NO_MEM;
// }

static void hash_proc(Proc *proc) { list_add(hash_list + pid_hashfn(proc->pid), &proc->hash_link); }

static void unhash_proc(Proc *proc) { list_del(&proc->hash_link); }

Proc *find_proc(int pid) {
    if (0 < pid && pid < MAX_PID) {
        List_entry *list = hash_list + pid_hashfn(pid), *le = list;
        while ((le = list_next(le)) != list) {
            Proc *proc = le2proc(le, hash_link);
            if (proc->pid == pid) { return proc; }
        }
    }
    return nullptr;
}

static void set_links(Proc *proc) {
    list_add(&proc_list, &proc->list_link);
    proc->yptr = nullptr;
    if (proc->parent != nullptr) {
        if ((proc->optr = proc->parent->cptr) != nullptr) { proc->optr->yptr = proc; }
        proc->parent->cptr = proc;
    }
    if (++nr_process >= NPROC) panic("Exceeded maximum number of processes");
}

static void remove_links(Proc *proc) {
    list_del(&proc->list_link);
    if (proc->optr != nullptr) { proc->optr->yptr = proc->yptr; }
    if (proc->yptr != nullptr) {
        proc->yptr->optr = proc->optr;
    } else {
        if (proc->parent != nullptr) { proc->parent->cptr = proc->optr; }
    }
    nr_process--;
}

static void proc_initlock(Proc *proc, char *name) {
    proc->lock.name = name;
    proc->lock.locked = 0;
    proc->lock.cpu = 0;
    proc->lock.info_index = KSTACK2INDEX(proc->kstack) + 200;
    proc->lock.info_nest = 0;
    assert(proc->lock.info_index < lock_info_nums);
}

Proc *alloc_proc() {
    acquire(&procs_lock);
    Proc *proc = kmalloc(sizeof(Proc));
    assert(proc != nullptr);
    proc->state = UNUSED;
    proc->trapframe = (Trapframe *)page2pa(AllocPage());
    assert(proc->trapframe != nullptr);
    memset((void *)(proc->trapframe), 0, PGSIZE);  // proc->trapframe->a0 set 0
    proc->runs = 0;
    proc->pid = 0;
    proc->parent = nullptr;
    proc->mm = mm_create();
    assert(proc->mm != nullptr);
    proc->flags = 0;
    memset(proc->name, 0, PROC_NAME_LEN);
    Page *pages = alloc_pages(KSTACKPAGE - 1);
    assert(pages != nullptr);
    long kernel_stack_bit_index = bitmap_scan_set(kernel_stack_bitmap, 1);
    assert(kernel_stack_bit_index != -1);
    uint64_t va = KSTACK(kernel_stack_bit_index);
    int ret = pages_insert(kernel_pagetable, pages, va, PTE_W | PTE_R, KSTACKPAGE - 1);
    assert(ret == 0);
    proc->kstack = va;
    memset(&proc->context, 0, sizeof(proc->context));
    proc->context.sp = (uint64_t)(proc->kstack + PGSIZE);
    proc->wait_state = 0;
    proc_initlock(proc, "process lock");
    proc->cptr = proc->optr = proc->yptr = nullptr;
    release(&procs_lock);
    return proc;
}

char *set_proc_name(Proc *proc, const char *name) {
    memset(proc->name, 0, sizeof(proc->name));
    return memcpy(proc->name, name, PROC_NAME_LEN);
}

char *get_proc_name(Proc *proc) {
    static char name[PROC_NAME_LEN + 1];
    memset(name, 0, sizeof(name));
    return memcpy(name, proc->name, PROC_NAME_LEN);
}

static void uvm_init(Mm_struct *mm) {
    pagetable_t *pagetable = mm->pagetable;
    // uintptr_t mem = page2kva(alloc_pages(USTACKPAGE));
    // mappages(pagetable, USTACKADDR, USTACKSIZE, mem, PTE_W | PTE_R | PTE_U | PTE_X);
    mm_map(mm, USTACKADDR, USTACKSIZE, VM_USER | VM_STACK | VM_READ | VM_WRITE | VM_EXEC, nullptr);
    mappages(pagetable, KERNBASE, (uint64_t)kernel_etext - KERNBASE, KERNBASE, PTE_R | PTE_U | PTE_X);
    mappages(pagetable, (uint64_t)kernel_etext, PHYSTOP - (uint64_t)kernel_etext, (uint64_t)kernel_etext,
             PTE_R | PTE_W | PTE_U);
}

void user_init(void (*start_routin)(void)) {
    Proc *p;
    p = alloc_proc();
    assert(p != nullptr);
    initproc = p;
    p->mm->pagetable = proc_pagetable(p);
    assert(p->mm->pagetable != nullptr);
    uvm_init(p->mm);
    p->state = RUNNABLE;
    p->kernel_proc = 0;
    p->context.ra = (uint64_t)fork_ret;
    p->trapframe->epc = (uint64_t)start_routin;  // user program counter
    p->trapframe->sp = USTACKADDR + USTACKSIZE;
    acquire(&procs_lock);
    p->pid = alloc_pid();
    // set_proc_name(p, "initcode");
    snprintf(p->name, sizeof(p->name), "initcode u_process_%d", p->pid);
    hash_proc(p);
    set_links(p);
    release(&procs_lock);
}

static void kernel_thread_ret(void (*start_roution)(void *), void *arg) {
    release(&myproc()->lock);
    intr_on();
    start_roution(arg);
}

void kernel_thread_init(void (*start_roution)(void *), void *arg) {
    Proc *proc = alloc_proc();
    assert(proc != nullptr);
    assert(kernel_pagetable != nullptr);
    proc->mm->pagetable = kernel_pagetable;
    proc->context.ra = (uint64_t)kernel_thread_ret;
    proc->context.a0 = (uint64_t)start_roution;
    if (arg != nullptr) proc->context.a1 = (uint64_t)arg;
    proc->state = RUNNABLE;
    proc->kernel_proc = true;
    acquire(&procs_lock);
    proc->pid = alloc_pid();
    snprintf(proc->name, sizeof(proc->name), "k_process_%d", proc->pid);
    hash_proc(proc);
    set_links(proc);
    release(&procs_lock);
}

int cpuid() {
    int id = r_tp();
    return id;
}

Cpu *mycpu(void) {
    int id = cpuid();
    Cpu *c = &cpus[id];
    return c;
}

Proc *myproc(void) {
    Cpu *c = mycpu();
    Proc *p = c->proc;
    return p;
}

void sched(void) {
    Proc *p = myproc();
    swtch(&p->context, &mycpu()->context);
}

void do_yield(void) {
    Proc *p = myproc();
    acquire(&p->lock);
    p->state = RUNNABLE;
    sched();
    release(&p->lock);
}

void task_delay(volatile int count) {
    count *= 50000;
    while (count--);
}

void proc_dump(void) {
    acquire(&print_procs_lock);
    static char *states[] = {
        [UNUSED] "unused", [SLEEPING] "sleep ", [RUNNABLE] "runble", [RUNNING] "run   ", [ZOMBIE] "zombie",
    };
    Proc *p;
    char *state;
    cprintf("\nproc dump....................................\n\n");
    List_entry *le = &proc_list;
    while ((le = list_next(le)) != &proc_list) {
        p = le2proc(le, list_link);
        if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
            state = states[p->state];
        else
            state = "???";
        if (p->parent != nullptr)
            cprintf("pid : %d state: %s name : %s kstack index : %d cpuid is %d parent pid : %d", p->pid,
                    state, p->name, KSTACK2INDEX(p->kstack), p->cpu - cpus, p->parent->pid);
        else
            cprintf("pid : %d state: %s name : %s kstack index : %d cpuid is %d", p->pid, state,
                    p->name, KSTACK2INDEX(p->kstack), p->cpu - cpus);
        cprintf("\n");
    }
    cprintf("\nend of proc dump.............................\n");
    release(&print_procs_lock);
}

void scheduler(void) {
    Proc *next;
    List_entry *le, *last;
    Cpu *c = mycpu();
    c->proc = nullptr;
    c->prev = nullptr;
    c->next = nullptr;
    c->has_zombie = false;
    while (1) {
    start:
        acquire(&procs_lock);
        next = nullptr;
        last = (c->prev == nullptr) ? &proc_list : &c->prev->list_link;
        // last = &proc_list;
        le = last;
        do {
            if ((le = list_next(le)) != &proc_list) {
                next = le2proc(le, list_link);
                if (next->state == RUNNABLE) break;
            }
        } while (le != last);
        if (next != nullptr && next->state == RUNNABLE) {
            for (int i = 0; i < NCPU; i++) {
                if (next == cpus[i].next) { 
                    c->prev = cpus[i].next;
                    release(&procs_lock);
                    goto start;
                }
            }
            c->next = next;
        }
        release(&procs_lock);
        if (c->next != nullptr) {
            next = c->next;
            assert(c->next->state == RUNNABLE);
            acquire(&next->lock);
            c->next->state = RUNNING;
            c->next->runs++;
            c->proc = c->prev = c->next;
            c->proc->cpu = c;
            sfence_vma();
            swtch(&c->context, &c->next->context);
            c->proc = nullptr;
            c->next = nullptr;
            release(&next->lock);
            if(c->has_zombie == true){
                assert(c->prev->state == ZOMBIE);
                c->has_zombie = false;
                release(&procs_lock);
            }
        } else {
            intr_on();
            asm volatile("wfi");
        }
    }
}

void either_copy_user2kernel(void *dst, int user_src, uint64_t src, uint64_t len) {
    Proc *p = myproc();
    if (user_src)
        copy_user2kernel(p->mm->pagetable, dst, src, len);
    else { memmove(dst, (char *)src, len); }
}

void do_sleep(void *chan, Spinlock *lk) {
    Proc *p = myproc();
    if (lk != &p->lock) {
        acquire(&p->lock);
        release(lk);
    }
    p->chan = chan;
    p->state = SLEEPING;
    sched();
    p->chan = nullptr;

    // This check is for kernel-mode processes. After entering kerneltrap() or usertrap(), hardware interrupts
    // are disabled. after entering scheduler() via yield() and after acquire(&p -> lock),interrupts
    // remain disabled even after release(). To ensure that kernel processes can still respond to interrupts
    // after calling the do_sleep() function, this check and adjustment are made.
    if (p->kernel_proc && mycpu()->intena == 0) { mycpu()->intena = 1; }
    if (lk != &p->lock) {
        release(&p->lock);
        acquire(lk);
    }
}

void do_wakeup(void *chan) {
    Proc *p;
    List_entry *le = &proc_list;
    acquire(&procs_lock);
    while ((le = list_next(le)) != &proc_list) {
        p = le2proc(le, list_link);
        acquire(&p->lock);
        if (p->state == SLEEPING && p->chan == chan) { p->state = RUNNABLE; }
        release(&p->lock);
    }
    release(&procs_lock);
}

static void put_pagetable(Mm_struct *mm) { FreePage(kva2page((uintptr_t)(mm->pagetable))); }

static void put_kstack(Proc *proc) {
    // free_pages(kva2page((proc->kstack)), KSTACKPAGE);
    unmap_range(kernel_pagetable, proc->kstack, proc->kstack + (KSTACKSIZE - PGSIZE));
}

static void free_proc(Proc *proc) {
    assert(proc->state == ZOMBIE);
    put_kstack(proc);
    bitmap_scan_clear(kernel_stack_bitmap, KSTACK2INDEX(proc->kstack), 1);
    kfree(proc);
}

void wakeup_proc(Proc *proc) {
    acquire(&proc->lock);
    assert(proc->state != ZOMBIE);
    if (proc->chan == proc && proc->state != RUNNABLE) {
        proc->state = RUNNABLE;
        proc->wait_state = 0;
    }
    release(&proc->lock);
}

static void copy_mm(uint32_t clone_flags, Proc *proc) {
    int ret = -1;
    Proc *current = myproc();
    Mm_struct *mm, *oldmm = current->mm;
    if (oldmm == nullptr) { return; }
    if (clone_flags & CLONE_VM) {
        mm = oldmm;
        goto good_mm;
    }
    mm = proc->mm;
    assert(mm != nullptr);
    lock_mm(oldmm);
    ret = dup_mmap(mm, oldmm);
    assert(ret == 0);
    unlock_mm(oldmm);
good_mm:
    mm_count_inc(mm);
}

int do_fork(uint32_t clone_flags) {
    Proc *proc, *current;
    current = myproc();
    // vm_print(current->mm->pagetable);
    // print_vma_list(current->mm);
    proc = alloc_proc();

    proc->mm->pagetable = proc_pagetable(proc);
    proc->state = RUNNABLE;
    proc->kernel_proc = 0;
    proc->context.ra = (uint64_t)fork_ret;

    assert(proc->mm->pagetable != nullptr);
    *(proc->trapframe) = *(current->trapframe);
    proc->trapframe->a0 = 0;
    assert(current->wait_state == 0);
    copy_mm(clone_flags, proc);

    // mappages(proc->mm->pagetable, KERNBASE, (uint64_t)kernel_etext - KERNBASE, KERNBASE,
    //          PTE_R | PTE_U | PTE_X);
    // mappages(proc->mm->pagetable, (uint64_t)kernel_etext, PHYSTOP - (uint64_t)kernel_etext,
    //          (uint64_t)kernel_etext, PTE_R | PTE_W | PTE_U);

    acquire(&procs_lock);
    proc->pid = alloc_pid();
    // set_proc_name(proc, current->name);
    snprintf(proc->name, sizeof(proc->name), "u_process_%d", proc->pid);
    proc->parent = current;
    hash_proc(proc);
    set_links(proc);
    release(&procs_lock);
    // proc_dump();
    return proc->pid;
}

void do_exit(int error_code) {
    Proc *current = myproc();
    if (current == initproc) panic("init exiting");
    acquire(&procs_lock);
    current->state = ZOMBIE;
    current->exit_code = error_code;
    Proc *proc = current->parent;
    if (proc->wait_state == WT_CHILD) wakeup_proc(proc);
    while (current->cptr != nullptr) {
        proc = current->cptr;
        current->cptr = proc->optr;
        proc->yptr = nullptr;
        if ((proc->optr = initproc->cptr) != nullptr) { initproc->cptr->yptr = proc; }
        proc->parent = initproc;
        initproc->cptr = proc;
        if (proc->state == ZOMBIE) {
            if (initproc->wait_state == WT_CHILD) { wakeup_proc(initproc); }
        }
    }
    mycpu()->has_zombie = true;
    // proc_dump();
    acquire(&current->lock);
    sched();
    panic("do_exit will not return!!.\n");
}

int do_wait(int pid, int *code_store) {
    Proc *proc, *current = myproc();
    bool haskid;
repeat:
    acquire(&procs_lock);
    haskid = false;
    if (pid != 0) {
        proc = find_proc(pid);
        if (proc != nullptr && proc->parent == current) {
            haskid = true;
            if (proc->state == ZOMBIE) goto found;
        }
    } else {
        for (proc = current->cptr; proc != nullptr; proc = proc->optr) {
            haskid = true;
            if (proc->state == ZOMBIE) goto found;
        }
    }
    if (haskid) {
        current->wait_state = WT_CHILD;
        release(&procs_lock);
        acquire(&current->lock);
        do_sleep(current, &current->lock);
        release(&current->lock);
        if (current->flags & PF_EXITING) do_exit(-E_KILLED);
        goto repeat;
    }
    release(&procs_lock);
    return -E_BAD_PROC;
found:
    release(&procs_lock);
    if (proc == initproc) panic("wait initproc");

    if (code_store != nullptr) {
        copy_kernel2user(current->mm->pagetable, (uintptr_t)code_store, (char *)&proc->exit_code,
                         sizeof(proc->exit_code));
    }
    Mm_struct *mm = proc->mm;
    if (mm != nullptr) {
        if (mm_count_dec(mm) == 0) {
            exit_mmap(mm);
            put_pagetable(mm);
            mm_destroy(mm);
        }
    }
    acquire(&procs_lock);
    for (int i = 0; i < NCPU; i++) {
        if (cpus[i].prev && cpus[i].prev == proc) {
            List_entry *prev_list = &proc->list_link;
            while (cpus[i].prev->state == ZOMBIE) {
                prev_list = list_prev(prev_list);
                if (prev_list == &proc_list) {
                    cpus[i].prev = nullptr;
                    break;
                } else {
                    cpus[i].prev = le2proc(prev_list, list_link);
                }
            }
        }
    }
    unhash_proc(proc);
    remove_links(proc);
    free_proc(proc);
    release(&procs_lock);
    return 0;
}

int do_kill(int pid) {
    Proc *proc;
    if ((proc = find_proc(pid)) != nullptr) {
        if (!(proc->flags & PF_EXITING)) {
            proc->flags |= PF_EXITING;
            if (proc->wait_state & WT_INTERAUPTED) { wakeup_proc(proc); }
            return 0;
        }
        return -E_KILLED;
    }
    return -E_INVAL;
}

static int load_icode(Proc *current, char *binary) {
    assert(current->mm == nullptr);
    int ret;
    Mm_struct *mm;
    mm = mm_create();
    assert(mm != nullptr);
    mm->pagetable = proc_pagetable(current);
    assert(mm->pagetable != nullptr);
    Page *page;
    struct elfhdr *elf = (struct elfhdr *)binary;
    struct proghdr *ph = (struct proghdr *)(binary + elf->phoff);
    assert(elf->magic == ELF_MAGIC);

    uint32_t vm_flags, perm;
    struct proghdr *ph_end = ph + elf->phnum;
    for (; ph < ph_end; ph++) {
        if (ph->type != ELF_PROG_LOAD) { continue; }
        assert(ph->filesz <= ph->memsz);
        if (ph->filesz == 0) continue;
        vm_flags = VM_USER, perm = PTE_U | PTE_X;
        if (ph->flags & ELF_PROG_FLAG_EXEC) vm_flags |= VM_EXEC;
        if (ph->flags & ELF_PROG_FLAG_WRITE) vm_flags |= VM_WRITE;
        if (ph->flags & ELF_PROG_FLAG_READ) vm_flags |= VM_READ;
        if (vm_flags & VM_WRITE) perm |= PTE_W;
        ret = mm_map(mm, ph->vaddr, ph->memsz, vm_flags, nullptr);
        assert(ret == 0);
        char *from = binary + ph->off;
        size_t off, size;
        uintptr_t start = ph->vaddr, end, va = PGROUNDDOWN(start);
        end = ph->vaddr + ph->filesz;
        while (start < end) {
            page = pagetable_alloc_page(mm->pagetable, va, perm);
            assert(page != nullptr);
            off = start - va, size = PGSIZE - off, va += PGSIZE;
            if (end < va) size -= va - end;
            memcpy((void *)(page2kva(page) + off), from, size);
            start += size, from += size;
        }
        end = ph->vaddr + ph->memsz;
        if (start < va) {
            if (start == end) continue;
            size = va - start, off = PGSIZE - size;
            if (end < va) size -= va - end;
            memset((void *)(page2kva(page) + off), 0, size);
            start += size;
            assert((end < va && start == end) || (end >= va && start == va));
        }
        while (start < end) {
            page = pagetable_alloc_page(mm->pagetable, va, perm);
            assert(page != nullptr);
            size = PGSIZE, va += PGSIZE;
            if (end < va) size -= va - end;
            memset((void *)(page2kva(page)), 0, size);
            start += size;
        }
    }
    // ret = mm_map(mm, USTACKADDR, USTACKSIZE, VM_USER | VM_STACK | VM_READ | VM_WRITE | VM_EXEC, nullptr);
    // uintptr_t mem = page2kva(alloc_pages(USTACKPAGE));
    // mappages(mm->pagetable, USTACKADDR, USTACKSIZE, mem, PTE_W | PTE_R | PTE_U | PTE_X);
    ret = mm_map(mm, USTACKADDR, USTACKSIZE, VM_USER | VM_STACK | VM_READ | VM_WRITE | VM_EXEC, nullptr);
    assert(ret == 0);
    Page *pages = alloc_pages(USTACKPAGE);
    assert(pages != nullptr);
    ret = pages_insert(mm->pagetable, pages, USTACKADDR, PTE_W | PTE_R | PTE_U | PTE_X, USTACKPAGE);
    assert(ret == 0);
    mm_count_inc(mm);
    current->mm = mm;
    ret = 0;
    return ret;
}

int do_execve(char *path, char **argv) {
    struct blk_buf req;
    int rdata[SECTOR_SZIE / 4] = {0};
    req.addr = 0;
    req.data = rdata;
    req.data_len = SECTOR_SZIE;
    req.is_write = 0;
    virtio_blk_rw(&req, "fs.img");
    size_t pages_num = PGROUNDUP(rdata[0]) / PGSIZE;
    Page *pages = alloc_pages(pages_num);
    char *binary = (char *)page2kva(pages);
    size_t count = rdata[0] / SECTOR_SZIE + 1;
    for (size_t i = 0; i < count; i++) {
        req.addr = (i + 1) * SECTOR_SZIE;
        req.data = rdata;
        req.data_len = SECTOR_SZIE;
        req.is_write = 0;
        virtio_blk_rw(&req, "fs.img");
        memcpy((void *)(binary + (i * SECTOR_SZIE)), (void *)(rdata), SECTOR_SZIE);
    }
    char *s, *last;
    for (last = s = path; *s; s++) {
        if (*s == '/') last = s + 1;
    }
    Proc *current = myproc();
    safestrcpy(current->name, last, sizeof(current->name));
    Mm_struct *mm = current->mm;
    if (mm != nullptr) {
        if (mm_count_dec(mm) == 0) {
            exit_mmap(mm);
            put_pagetable(mm);
            mm_destroy(mm);
        }
        current->mm = nullptr;
    }
    int ret;
    ret = load_icode(current, binary);
    assert(ret == 0);

    uint64_t argc, sp, ustack[MAXARG + 1], stackbase;
    sp = USTACKADDR + USTACKSIZE;
    stackbase = USTACKADDR + PGSIZE;
    for (argc = 0; argv[argc]; argc++) {
        assert(argc < MAXARG);
        sp -= strlen(argv[argc]) + 1;
        sp -= sp % 16;
        assert(sp >= stackbase);
        copy_kernel2user(current->mm->pagetable, sp, argv[argc], strlen(argv[argc]) + 1);
        ustack[argc] = sp;
    }
    ustack[argc] = 0;
    sp -= (argc + 1) * sizeof(uint64_t);
    sp -= sp % 16;
    assert(sp >= stackbase);
    copy_kernel2user(current->mm->pagetable, sp, (char *)ustack, (argc + 1) * sizeof(uint64_t));
    current->trapframe->a1 = sp;
    current->trapframe->epc = ((struct elfhdr *)binary)->entry;
    current->trapframe->sp = sp;
    free_pages(pages, pages_num);
    return argc;
}