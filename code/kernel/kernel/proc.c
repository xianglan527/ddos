#include "proc.h"

#include "assert.h"
#include "config.h"
#include "error.h"
#include "hash.h"
#include "printf.h"
#include "riscv.h"
#include "slab.h"
#include "spinlock.h"
#include "string.h"
#include "syscall.h"
#include "trap.h"

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

// The struct members list_link, parent, hash_link, *cptr, *yptr,
// and *optr of a process can only be read or written while holding the proc_lock.

// to prevent deadlocks,the locking order must be procs_lock--->process lock.
Spinlock procs_lock;

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
    list_init(&proc_list);
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

static void remove_links(Proc *proc){
    list_del(&proc->list_link);
    if(proc->optr != nullptr){
        proc->optr->yptr = proc->yptr;
    }
    if(proc->yptr != nullptr){
        proc->yptr->optr = proc->optr;
    }
    else{
        if(proc->parent != nullptr){
            proc->parent->cptr = proc->optr;
        }
    }
    nr_process--;
}

Proc *alloc_proc() {
    Proc *proc = kmalloc(sizeof(*proc));
    assert(proc != nullptr);
    proc->state = UNUSED;
    initlock(&proc->lock, "process lock");
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
    uintptr_t pa = (uintptr_t)page2kva(AllocPage());
    assert(pa != 0);
    uint64_t va = KSTACK((ulong)(nr_process));
    kvmmap(va, (uint64_t)pa, PGSIZE, PTE_R | PTE_W);
    proc->kstack = va;
    memset(&proc->context, 0, sizeof(proc->context));
    proc->context.sp = (uint64_t)(proc->kstack + PGSIZE);
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

void scheduler(void) {
    Proc *next, *prev = nullptr;
    List_entry *le, *last;
    Cpu *c = mycpu();
    c->proc = 0;
    while (1) {
        next = nullptr;
        acquire(&procs_lock);
        // if (prev != nullptr) {
        //     last = &prev->list_link;
        // } else {
        //     last = &proc_list;
        // }
        last = (prev == nullptr) ? &proc_list : &prev->list_link;
        le = last;
        do {
            if ((le = list_next(le)) != &proc_list) {
                next = le2proc(le, list_link);
                if (next->state == RUNNABLE) break;
            }
        } while (le != last);
        release(&procs_lock);
        if (next != nullptr && next->state == RUNNABLE) {
            acquire(&next->lock);
            if (next->state == RUNNABLE) {
                next->state = RUNNING;
                next->runs++;
                c->proc = next;
                swtch(&c->context, &next->context);
                prev = c->proc;
                c->proc = 0;
            }
            release(&next->lock);
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
    if (lk != &p->lock) {
        release(&p->lock);
        acquire(lk);
    }
    // This check is for kernel-mode processes. After entering kerneltrap() or usertrap(), hardware interrupts
    // are disabled. after entering scheduler() via yield() and after acquire(&p -> lock),interrupts
    // remain disabled even after release(). To ensure that kernel processes can still respond to interrupts
    // after calling the do_sleep() function, this check and adjustment are made.
    if (p->kernel_proc && mycpu()->intena == 0) { mycpu()->intena = 1; }
}

void do_wakeup(void *chan) {
    Proc *p;
    List_entry *le = &proc_list;
    acquire(&procs_lock);
    while ((le = list_next(le)) != &proc_list) {
        p = le2proc(le, list_link);
        acquire(&p->lock);
        if (p->state == SLEEPING && p->chan == chan) { 
            p->state = RUNNABLE; 
        }
        release(&p->lock);
    }
    release(&procs_lock);
}

static void put_pagetable(Mm_struct *mm){
    FreePage(kva2page((uintptr_t)(mm->pagetable)));
}

static void put_kstack(Proc *proc){
    FreePage(kva2page((proc->kstack)));
}

void wakeup_proc(Proc *proc) {
    acquire(&proc->lock);
    assert(proc->state != ZOMBIE);
    if (proc->chan == proc && proc->state == SLEEPING) {
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
    // proc->mm = mm;
}

int do_fork(uint32_t clone_flags) {
    Proc *proc, *current;
    current = myproc();
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

    mappages(proc->mm->pagetable, KERNBASE, (uint64_t)kernel_etext - KERNBASE, KERNBASE,
             PTE_R | PTE_U | PTE_X);
    mappages(proc->mm->pagetable, (uint64_t)kernel_etext, PHYSTOP - (uint64_t)kernel_etext,
             (uint64_t)kernel_etext, PTE_R | PTE_W | PTE_U);

    acquire(&procs_lock);
    proc->pid = alloc_pid();
    // set_proc_name(proc, current->name);
    snprintf(proc->name, sizeof(proc->name), "u_process_%d", proc->pid);
    proc->parent = current;
    hash_proc(proc);
    set_links(proc);
    release(&procs_lock);
    return proc->pid;
}

void do_exit(int error_code) {
    Proc *current = myproc();
    if (current == initproc) panic("init exiting");
    acquire(&current->lock);
    current->state = ZOMBIE;
    current->exit_code = error_code;
    release(&current->lock);
    acquire(&procs_lock);
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
    release(&procs_lock);
    acquire(&current->lock);
    sched();
    panic("do_exit will not return !!.\n", current->pid);
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
    if (proc == initproc) panic("wait initproc");
    unhash_proc(proc);
    remove_links(proc);
    release(&procs_lock);
    if (code_store != nullptr) {
        copy_kernel2user(current->mm->pagetable, (uintptr_t)code_store, (char *)&proc->exit_code, sizeof(proc->exit_code));
    }
    Mm_struct *mm = proc->mm;
    if(mm != nullptr){
        if(mm_count_dec(mm) == 0){
            exit_mmap(mm);
            put_pagetable(mm);
            mm_destroy(mm);
        }
    }
    put_kstack(proc);
    kfree(proc);
    return 0;
}