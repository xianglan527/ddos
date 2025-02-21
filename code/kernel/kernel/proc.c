#include "proc.h"

#include "assert.h"
#include "atomic.h"
#include "bitmap.h"
#include "config.h"
#include "elf.h"
#include "error.h"
#include "hash.h"
#include "printf.h"
#include "riscv.h"
#include "sched.h"
#include "slab.h"
#include "spinlock.h"
#include "stdio.h"
#include "string.h"
#include "swap.h"
#include "syscall.h"
#include "trap.h"
#include "vfs.h"
#include "virtio-blk.h"

Cpu cpus[NCPU];
Proc *initproc;
List_entry proc_list;
static List_entry timer_list;
static List_entry timer_func_list;
List_entry proc_mm_list;

#define HASH_SHIFT 10
#define HASH_LIST_SIZE (1 << HASH_SHIFT)
#define pid_hashfn(x) (hash64(x, HASH_SHIFT))

static List_entry hash_list[HASH_LIST_SIZE];

Proc *initproc = nullptr;
Proc *daemonproc = nullptr;

static ulong next_pid = 1;
// static ulong nr_process = 0;
Atomic nr_process;
struct bitmap *kernel_stack_bitmap = nullptr;

// The struct members list_link, parent, hash_link, *cptr, *yptr,
// and *optr of a process can only be read or written while holding the proc_lock.

// to prevent deadlocks,the locking order must be procs_lock--->process lock.
Spinlock procs_lock;
Spinlock timer_lock;
Spinlock timer_func_lock;
Spinlock print_struct_lock;
Spinlock proc_mm_list_lock;

static volatile bool timer_init_ok = false;

extern char trampoline[];  // trampoline.S
extern char kernel_etext[];

extern ulong ticks;

extern Sched_class CFS_sched_class;

static void __do_exit(void);
static int __do_kill(Proc *proc, int error_code);

void swtch(struct context *, struct context *);

int alloc_pid() {
    acquire(&procs_lock);
    int pid;
    Proc *proc;
    List_entry *le = &proc_list;
    pid = next_pid;
repeat:
    if (++next_pid == MAX_PID) next_pid = 1;
    while ((le = list_next(le)) != &proc_list) {
        proc = le2proc(le, list_link);
        if (proc->pid == next_pid) goto repeat;
    }
    release(&procs_lock);
    return pid;
}

void timer_start_init(void) {
    list_init(&timer_list);
    list_init(&timer_func_list);
    initlock(&timer_lock, "timer_lock");
    initlock(&timer_func_lock, "timer_func_lock");
    timer_init_ok = true;
}

void proc_init(void) {
    initlock(&procs_lock, "procs_lock");
    initlock(&print_struct_lock, "print_struct_lock");
    initlock(&proc_mm_list_lock, "proc_mm_list_lock");
    list_init(&proc_list);
    list_init(&proc_mm_list);
    kernel_stack_bitmap = bitmap_init(NPROC);
    for (int i = 0; i < HASH_LIST_SIZE; i++) { list_init(hash_list + i); }
    atomic_set(&nr_process, 0);
    timer_start_init();
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
    page_insert(pagetable, pa2page((uint64_t)(p->trapframe)), TRAPFRAME(p->mm_index), PTE_R | PTE_W);
    return pagetable;
}

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
    assert(proc->state == RUNNABLE);
    list_add(&proc_list, &proc->list_link);
    sched_class_enqueue(proc);
    proc->yptr = nullptr;
    if (proc->parent != nullptr) {
        if ((proc->optr = proc->parent->cptr) != nullptr) { proc->optr->yptr = proc; }
        proc->parent->cptr = proc;
    }
    // if (++nr_process >= NPROC) panic("Exceeded maximum number of processes");
}

static void remove_links(Proc *proc) {
    list_del(&proc->list_link);
    sched_class_dequeue(proc);
    if (proc->optr != nullptr) { proc->optr->yptr = proc->yptr; }
    if (proc->yptr != nullptr) {
        proc->yptr->optr = proc->optr;
    } else {
        if (proc->parent != nullptr) { proc->parent->cptr = proc->optr; }
    }
    // nr_process--;
}

void set_proc_cpu(Proc *proc, Cpu *cpu) {
    acquire(&proc->lock);
    proc->set_cpu = cpu;
    assert(proc->cpu != nullptr);
    if (proc->cpu == proc->set_cpu) {
        release(&proc->lock);
        return;
    }
    if (!list_empty(&proc->run_link)) { sched_class_dequeue(proc); }
    sched_class_enqueue(proc);
    release(&proc->lock);
    // proc->set_cpu = nullptr;
}

void clear_proc_setcpu(Proc *proc) {
    acquire(&proc->lock);
    proc->set_cpu = nullptr;
    release(&proc->lock);
}

// static void proc_initlock(Proc *proc, char *name) {
//     proc->lock.name = name;
//     proc->lock.locked = 0;
//     proc->lock.cpu = 0;
//     proc->lock.info_index = KSTACK2INDEX(proc->kstack) + 200;
//     atomic_set(&proc->lock.info_nest, 0);
//     assert(proc->lock.info_index < lock_info_nums);
// }

Proc *alloc_proc() {
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
    proc->mm->proc = proc;
    proc->flags = 0;
    memset(proc->name, 0, PROC_NAME_LEN);
    Page *pages = alloc_pages(KSTACKPAGE - 1);
    assert(pages != nullptr);
    // long kernel_stack_bit_index = atomic_read(&nr_process);
    // if (kernel_stack_bit_index == NPROC) panic("Exceeded maximum number of processes");
    atomic_inc(&nr_process);
    if (atomic_read(&nr_process) > NPROC) panic("Exceeded maximum number of processes");
    long kernel_stack_bit_index = bitmap_scan_set(kernel_stack_bitmap, 1);
    assert(kernel_stack_bit_index != -1);
    uint64_t va = KSTACK(kernel_stack_bit_index);
    acquire(&procs_lock);
    int ret = pages_insert(kernel_pagetable, pages, va, PTE_W | PTE_R, KSTACKPAGE - 1);
    release(&procs_lock);
    assert(ret == 0);
    proc->kstack = va;
    // memset(&proc->context, 0, sizeof(proc->context));
    proc->context.sp = (uint64_t)(proc->kstack + KSTACKSIZE - PGSIZE);
    proc->wait_state = 0;
    // proc_initlock(proc, "process lock");
    initlock(&proc->lock, "process lock");
    proc->cptr = proc->optr = proc->yptr = nullptr;
    // acquire(&procs_lock);
    proc->pid = alloc_pid();
    // release(&procs_lock);
    list_init(&proc->thread_group);
    proc->mm_index = 0;
    list_init(&proc->run_link);
    proc->alloc_time_slice = proc->time_slice = 0;
    proc->need_resched = false;
    proc->sem_queue = nullptr;
    event_init(&proc->event);
    proc->sig_blocked = 0;
    list_init(&proc->siginfo_list);
    signal_init(&proc->signal);
    proc->priority = 0;  // default: weight is 1024;
    proc->vruntime = 0;
    proc->fs_struct = nullptr;
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
    mm_map(mm, USTACKADDR, USTACKSIZE, VM_USER | VM_STACK | VM_READ | VM_WRITE | VM_EXEC, nullptr);
    mappages(pagetable, KERNBASE, (uint64_t)kernel_etext - KERNBASE, KERNBASE, PTE_R | PTE_U | PTE_X);
    mappages(pagetable, (uint64_t)kernel_etext, PHYSTOP - (uint64_t)kernel_etext, (uint64_t)kernel_etext,
             PTE_R | PTE_W | PTE_U);
}

void user_init(void (*start_routin)(void)) {
    Proc *p;
    // acquire(&procs_lock);
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
    snprintf(p->name, sizeof(p->name), "initcode u_process_%d", p->pid);
    hash_proc(p);
    set_links(p);
    release(&procs_lock);
}

static void kernel_thread_ret(void (*start_roution)(void *), void *arg) {
    Proc *p = myproc();
    release(&myproc()->lock);
    intr_on();
    start_roution(arg);
    acquire(&myproc()->lock);
    myproc()->state = ZOMBIE;
    wakeup_queue(&myproc()->event.wait_queue, WT_INTERAUPTED, 1);
    sched();
}

Proc *kernel_thread_init(void (*start_roution)(void *), void *arg) {
    // acquire(&procs_lock);
    Proc *proc = alloc_proc();
    assert(proc != nullptr);
    daemonproc = proc;
    assert(kernel_pagetable != nullptr);
    proc->mm->pagetable = kernel_pagetable;
    proc->context.ra = (uint64_t)kernel_thread_ret;
    proc->context.a0 = (uint64_t)start_roution;
    if (arg != nullptr) proc->context.a1 = (uint64_t)arg;
    proc->state = RUNNABLE;
    proc->kernel_proc = true;
    assert((proc->fs_struct = fs_create()) != nullptr);
    fs_count_inc(proc->fs_struct);
    acquire(&procs_lock);
    snprintf(proc->name, sizeof(proc->name), "kernel_proc%d", proc->pid);
    hash_proc(proc);
    set_links(proc);
    release(&procs_lock);
    return proc;
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

    // cprintf("cccccccccccccccccccccc   ra is %p current pid is %d current cpu is %d\n", mycpu()->context.ra,
    //         myproc()->pid, mycpu() - cpus);
    swtch(&p->context, &mycpu()->context);
    // cprintf("hhhhhhhhhhhhhhhhhhhhhh  ra is %p current pid is %d current cpu is %d\n", mycpu()->context.ra,
    //         myproc()->pid, mycpu() - cpus);
}

void do_yield(void) {
    Proc *p = myproc();
    assert(p->state == RUNNING);
    acquire(&p->lock);
    if (p->cpu->sc == &CFS_sched_class) {
        sched_class_insert_rbtree(p);
    } else {
        p->state = RUNNABLE;
    }
    p->need_resched = false;
    sched();
    // if (p->cpu->sc == &CFS_sched_class) { sched_class_remove_rbtree(p); }
    release(&p->lock);
}

void task_delay(volatile int count) {
    count *= 50000;
    while (count--);
}

void proc_dump(void) {
    // acquire(&print_struct_lock);
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
            cprintf(
                "pid : %d state: %s name : %s kstack index : %d kstack is :%p cpuid is %d mm_index : %d "
                "alloc time slice :%lu time slice :%lu vruntime :%lu priority :%d "
                "parent pid : %d",
                p->pid, state, p->name, KSTACK2INDEX(p->kstack), p->kstack, p->cpu - cpus, p->mm_index,
                p->alloc_time_slice, p->time_slice, p->vruntime, p->priority, p->parent->pid);
        else
            cprintf(
                "pid : %d state: %s name : %s kstack index : %d kstack is :%p cpuid is %d mm_index : %d "
                "alloc time slice :%lu time slice :%lu vruntime :%lu priority :%d",
                p->pid, state, p->name, KSTACK2INDEX(p->kstack), p->kstack, p->cpu - cpus, p->mm_index,
                p->alloc_time_slice, p->time_slice, p->vruntime, p->priority);
        cprintf("\n");
    }
    cprintf("\nend of proc dump.............................\n");
    // release(&print_struct_lock);
}

void proc_mm_dump(void) {
    List_entry *le = &proc_list;
    Proc *p;
    while ((le = list_next(le)) != &proc_list) {
        p = le2proc(le, list_link);
        // if (p->pid == 3) {
        //     vm_map_print(p->mm->pagetable);
        //     // vm_print(p->mm->pagetable);
        //     print_vma_list(p->mm);
        // }
        if (p->state == RUNNING) {
            vm_map_print(p->mm->pagetable);
            // vm_print(p->mm->pagetable);
            print_vma_list(p->mm);
        }
    }
}

void scheduler(void) {
    // Proc *next;
    List_entry *le, *last;
    Cpu *c = mycpu();
    c->proc = nullptr;
    c->prev = nullptr;
    c->next = nullptr;
    while (1) {
        // acquire(&procs_lock);
        // next = nullptr;
        // c->next = nullptr;
        // last = (c->prev == nullptr) ? &proc_list : &c->prev->list_link;
        // // last = &proc_list;
        // le = last;
        // do {
        //     if ((le = list_next(le)) != &proc_list) {
        //         next = le2proc(le, list_link);
        //         if (next->state == RUNNABLE) {
        //             for (int i = 0; i < NCPU; i++) {
        //                 if (next == cpus[i].next) { goto end; }
        //             }
        //             if (next->set_cpu == nullptr || next->set_cpu == mycpu()) {
        //                 c->next = next;
        //                 break;
        //             }
        //         }
        //     }
        // end:;
        // } while (le != last);

        c->next = sched_class_pick_next();

        // acquire(&print_struct_lock);
        // proc_dump();
        // release(&print_struct_lock);

        if (c->next != nullptr) {
            // if(c->next->pid == 200){
            //     proc_dump();
            //     while(1);
            // }
            // next = c->next;
            // cprintf("666666 : c->next->pid is %d c->next->cpu is %d  current cpu is %d\n",
            //         c->next->pid ,c->next->cpu - cpus, c - cpus);
            // if(!(c->next->state == RUNNING && c->next->cpu == c)){
            //     int pp = 3;
            // }
            assert(c->next->state == RUNNING && c->next->cpu == c);
            acquire(&c->next->lock);
            // c->next->state = RUNNING;
            // c->next->runs++;
            // if (c->next->cpu->sc == &CFS_sched_class) { sched_class_remove_rbtree(c->next); }
            c->proc = c->prev = c->next;
            // c->proc->cpu = c;
            // release(&procs_lock);
            sfence_vma();
            // cprintf("ssssssssssssssssssssss  ra is %p current pid is %d current cpu is %d\n",
            //         c->next->context.ra, myproc()->pid, mycpu() - cpus);
            swtch(&c->context, &c->next->context);
            // cprintf("vvvvvvvvvvvvvvvvvvvvv ra is %p current pid is %d current cpu is %d\n",
            //         c->next->context.ra, myproc()->pid, mycpu() - cpus);
            c->proc = nullptr;
            release(&c->next->lock);
            if (c->prev->kernel_proc == false && c->prev->state == ZOMBIE) { release(&procs_lock); }
        } else {
            // release(&procs_lock);
            // int old_intr = intr_get();
            intr_on();
            asm volatile("wfi");
            // if(old_intr == 0){
            //     intr_off();
            // }
        }
    }
}

void either_copy_user2kernel(void *dst, int user_src, uint64_t src, uint64_t len) {
    Proc *p = myproc();
    if (user_src) {
        copy_user2kernel(p->mm->pagetable, dst, src, len);
    } else {
        memmove(dst, (char *)src, len);
    }
}

void either_copy_kernel2user(uint64_t dst, int user_dst, void *src, uint64_t len) {
    Proc *p = myproc();
    if (user_dst) {
        copy_kernel2user(p->mm->pagetable, dst, src, len);
    } else {
        memmove((char *)dst, src, len);
    }
}

void sleeping(void *chan, Spinlock *lk) {
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
    // after calling the sleeping() function, this check and adjustment are made.
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
        if (p->state == SLEEPING && p->chan == chan) {
            if (p->cpu->sc == &CFS_sched_class) {
                sched_class_insert_rbtree(p);
            } else {
                p->state = RUNNABLE;
            }
        }
        release(&p->lock);
    }
    release(&procs_lock);
}

static void put_pagetable(Mm_struct *mm) {
    // unmap_range_without_free_page(mm->pagetable, TRAMPOLINE, TRAMPOLINE + PGSIZE);
    // unmap_range(mm->pagetable, TRAPFRAME, TRAPFRAME + PGSIZE);
    free_pagetable(mm->pagetable, 1);
}

static void put_kstack(Proc *proc) {
    unmap_range(kernel_pagetable, proc->kstack, proc->kstack + (KSTACKSIZE - PGSIZE));
    exit_range(kernel_pagetable, proc->kstack, proc->kstack + (KSTACKSIZE - PGSIZE));
}

static void free_proc(Proc *proc) {
    assert(proc->state == ZOMBIE);
    put_kstack(proc);
    bitmap_scan_clear(kernel_stack_bitmap, KSTACK2INDEX(proc->kstack), 1);
    atomic_dec(&nr_process);
    // list_del(&proc->run_link);
    kfree(proc);
}

void wakeup_proc(Proc *proc) {
    acquire(&proc->lock);
    // if (myproc() != 0)
    //     cprintf("uuuuuuuuuu11111111  current pid is %d current cpu is %d wakeup is %d\n", myproc()->pid,
    //             mycpu() - cpus, proc->pid);
    assert(proc->state != ZOMBIE && proc->state != UNUSED);
    // if (myproc() != 0)
    //     cprintf("uuuuuuuuuu22222222  current pid is %d current cpu is %d wakeup is %d state is %d\n",
    //     myproc()->pid,
    //             mycpu() - cpus, proc->pid, proc->state);
    if (proc->chan == proc && proc->state == SLEEPING) {
        if (proc->cpu->sc == &CFS_sched_class) {
            sched_class_insert_rbtree(proc);
        } else {
            proc->state = RUNNABLE;
        }
        proc->wait_state = 0;
        // if (myproc() != 0)
        //     cprintf("uuuuuuuuu33333333  current pid is %d current cpu is %d wakeup is %d\n", myproc()->pid,
        //             mycpu() - cpus, proc->pid);
    }
    release(&proc->lock);
}

static void de_thread(Proc *proc) {
    if (!list_empty(&proc->thread_group)) list_del_init(&proc->thread_group);
}

static Proc *next_thread(Proc *proc) { return le2proc(list_next(&proc->thread_group), thread_group); }

static void copy_mm(uint32_t clone_flags, Proc *proc) {
    int ret = -1;
    Proc *current = myproc();
    Mm_struct *mm, *oldmm = current->mm;
    if (oldmm == nullptr) { return; }
    if (clone_flags & CLONE_VM) {
        assert(proc->mm != nullptr);
        assert(proc->mm_index == 0);
        unmap_range(proc->mm->pagetable, TRAPFRAME(proc->mm_index), TRAPFRAME(proc->mm_index) + PGSIZE);
        put_pagetable(proc->mm);
        proc->trapframe = (Trapframe *)page2pa(AllocPage());
        kfree(proc->mm);
        proc->mm = oldmm;
        mm_count_inc(oldmm);
        atomic_inc(&oldmm->mm_share_count);
        oldmm->share = true;
        return;
    }
    mm = proc->mm;
    assert(mm != nullptr);
    lock_mm(oldmm);
    ret = dup_mmap(mm, oldmm);
    assert(ret == 0);
    unlock_mm(oldmm);
    if (mm != oldmm) {
        mm->brk_start = oldmm->brk_start;
        mm->brk = oldmm->brk;
        acquire(&proc_mm_list_lock);
        list_add(&proc_mm_list, &mm->proc_mm_link);
        release(&proc_mm_list_lock);
    }
    mm_count_inc(mm);
    atomic_inc(&mm->mm_share_count);
}

static void copy_sem(uint32_t clone_flags, Proc *proc) {
    Proc *current = myproc();
    Sem_queue *sem_queue, *old_sem_queue = current->sem_queue;
    assert(old_sem_queue != nullptr);
    if (clone_flags & CLONE_SEM) {
        sem_queue = old_sem_queue;
        goto good_sem_queue;
    }
    assert((sem_queue = sem_queue_create()) != nullptr);
    acquire(&old_sem_queue->sem_queue_lock);
    dup_sem_queue(sem_queue, old_sem_queue);
    release(&old_sem_queue->sem_queue_lock);
good_sem_queue:
    atomic_inc(&sem_queue->count);
    proc->sem_queue = sem_queue;
}

static void put_sem_queue(Proc *proc) {
    Sem_queue *sem_queue = proc->sem_queue;
    if (sem_queue != nullptr) {
        if (atomic_sub_return(&sem_queue->count, 1) == 0) {
            exit_sem_queue(sem_queue);
            sem_queue_destroy(sem_queue);
        }
    }
}

static void put_siginfo(Proc *proc) {
    List_entry *list = &proc->siginfo_list, *le = list;
    while ((le = list_next(le)) != list) {
        list_del(le);
        kfree(le2siginfo(le, siginfo_link));
    }
}

static int copy_fs(uint32_t clone_flags, Proc *proc) {
    Fs_struct *fs_struct, *old_fs_struct = myproc()->fs_struct;
    assert(old_fs_struct != nullptr);
    if (clone_flags & CLONE_FS) {
        fs_struct = old_fs_struct;
        goto good_fs_struct;
    }
    int ret = -E_NO_MEM;
    if ((fs_struct = fs_create()) == nullptr) { goto bad_fs_struct; }
    if ((ret = dup_fs(fs_struct, old_fs_struct)) != 0) { goto bad_dup_cleanup_fs; }
good_fs_struct:
    fs_count_inc(fs_struct);
    proc->fs_struct = fs_struct;
    return 0;
bad_dup_cleanup_fs:
    fs_destroy(fs_struct);
bad_fs_struct:
    return ret;
}

static void put_fs(Proc *proc) {
    Fs_struct *fs_struct = proc->fs_struct;
    if (fs_struct != nullptr) {
        if (fs_count_dec(fs_struct) == 0) { fs_destroy(fs_struct); }
    }
}

void may_killed(void) {
    if (myproc() != nullptr && myproc()->flags & PF_EXITING) { __do_exit(); }
}

int do_fork(uint32_t clone_flags, uintptr_t stack) {
    Proc *proc, *current;
    // acquire(&procs_lock);
    current = myproc();
    proc = alloc_proc();
    proc->mm->pagetable = proc_pagetable(proc);
    assert(proc->mm->pagetable != nullptr);
    proc->state = RUNNABLE;
    proc->kernel_proc = 0;
    proc->context.ra = (uint64_t)fork_ret;
    assert(copy_fs(clone_flags, proc) == 0);
    copy_mm(clone_flags, proc);
    proc->mm_index = atomic_read(&proc->mm->mm_share_count) - 1;
    *(proc->trapframe) = *(current->trapframe);
    if (stack != 0) { proc->trapframe->sp = stack; }
    copy_sem(clone_flags, proc);
    if (clone_flags & CLONE_SIGACTION) { sigaction_copy(&proc->signal, &current->signal); }
    proc->trapframe->a0 = 0;
    assert(current->wait_state == 0);
    snprintf(proc->name, sizeof(proc->name), "u_process_%d", proc->pid);
    // acquire(&procs_lock);
    if (clone_flags & CLONE_THREAD) {
        list_add_before(&current->thread_group, &proc->thread_group);
        assert(proc->mm_index >= 1);
        page_insert(proc->mm->pagetable, pa2page((uint64_t)(proc->trapframe)), TRAPFRAME(proc->mm_index),
                    PTE_R | PTE_W);
    }
    proc->parent = current;
    // proc->time_slice = current->time_slice / 2;
    // current->time_slice -= proc->time_slice;
    acquire(&procs_lock);
    hash_proc(proc);
    set_links(proc);
    release(&procs_lock);
    // sinode_dump_struct_lock();
    return proc->pid;
}

static void __do_exit() {
    Proc *current = myproc();
    if (current == initproc) panic("init exiting");
    // acquire(&procs_lock);
    Mm_struct *mm = current->mm;
    // lock_mm(mm);
    if (mm != nullptr) {
        unmap_range(mm->pagetable, TRAPFRAME(current->mm_index), TRAPFRAME(current->mm_index) + PGSIZE);
        if (mm_count_dec(mm) == 0) {
            exit_mmap(mm);
            put_pagetable(mm);
            acquire(&proc_mm_list_lock);
            list_del(&mm->proc_mm_link);
            release(&proc_mm_list_lock);
            mm_destroy(mm);
        }
        // current->mm = nullptr;
    }
    // unlock_mm(mm);
    put_fs(current);
    put_sem_queue(current);
    put_siginfo(current);
    Proc *proc, *parent;
    acquire(&procs_lock);
    proc = parent = current->parent;
    do {
        if (proc->wait_state == WT_CHILD) { wakeup_proc(proc); }
        proc = next_thread(proc);
    } while (proc != parent);
    if ((parent = next_thread(current)) == current) { parent = initproc; }
    de_thread(current);
    while (current->cptr != nullptr) {
        proc = current->cptr;
        current->cptr = proc->optr;
        proc->yptr = nullptr;
        if ((proc->optr = parent->cptr) != nullptr) { parent->cptr->yptr = proc; }
        proc->parent = parent;
        parent->cptr = proc;
        if (proc->state == ZOMBIE) {
            if (parent->wait_state == WT_CHILD) { wakeup_proc(parent); }
        }
    }
    acquire(&current->lock);
    wakeup_queue(&current->event.wait_queue, WT_INTERAUPTED, 1);
    current->state = ZOMBIE;
    sched();
    panic("do_exit will not return!!.\n");
}

void do_exit_thread(int error_code) {
    // acquire(&procs_lock);
    myproc()->exit_code = error_code;
    return __do_exit();
}

void do_exit(int error_code) {
    acquire(&procs_lock);
    Proc *current = myproc();
    List_entry *list = &current->thread_group, *le = list;
    while ((le = list_next(le)) != list) {
        while ((le = list_next(le)) != list) {
            Proc *proc = le2proc(le, thread_group);
            __do_kill(proc, error_code);
        }
    }
    release(&procs_lock);
    return do_exit_thread(error_code);
}

int do_wait(int pid, int *code_store) {
    Proc *proc, *cproc, *current;
    bool haskid;
repeat:
    acquire(&procs_lock);
    cproc = current = myproc();
    haskid = false;
    if (pid != 0) {
        proc = find_proc(pid);
        if (proc != nullptr) {
            do {
                if (proc->parent == cproc) {
                    haskid = true;
                    if (proc->state == ZOMBIE) goto found;
                }
                break;
            } while (cproc != current);
        }
    } else {
        do {
            proc = cproc->cptr;
            for (; proc != nullptr; proc = proc->optr) {
                haskid = true;
                if (proc->state == ZOMBIE) goto found;
            }
            cproc = next_thread(cproc);
        } while (cproc != current);
    }
    if (haskid) {
        current->wait_state = WT_CHILD;
        sleeping(current, &procs_lock);
        release(&procs_lock);
        may_killed();
        goto repeat;
    }
    release(&procs_lock);
    return -E_BAD_PROC;
found:
    if (proc == initproc) panic("wait initproc");
    // for (int i = 0; i < NCPU; i++) {
    //     if (cpus[i].prev && cpus[i].prev == proc) {
    //         List_entry *prev_list = &proc->list_link;
    //         while (cpus[i].prev->state == ZOMBIE) {
    //             prev_list = list_prev(prev_list);
    //             if (prev_list == &proc_list) {
    //                 cpus[i].prev = nullptr;
    //                 break;
    //             } else {
    //                 cpus[i].prev = le2proc(prev_list, list_link);
    //             }
    //         }
    //     }
    // }
    unhash_proc(proc);
    remove_links(proc);
    release(&procs_lock);
    if (code_store != nullptr) {
        lock_mm(current->mm);
        copy_kernel2user(current->mm->pagetable, (uintptr_t)code_store, (char *)&proc->exit_code,
                         sizeof(proc->exit_code));
        unlock_mm(current->mm);
    }
    // cprintf("do exit...current pid is %d proc pid is %d mm_count is %d cpuid is %d\n", myproc()->pid,
    // proc->pid,
    //         mm_count(current->mm), mycpu() - cpus);
    free_proc(proc);
    return 0;
}

int __do_kill(Proc *proc, int error_code) {
    if (!(proc->flags & PF_EXITING)) {
        proc->flags |= PF_EXITING;
        proc->exit_code = error_code;
        if (proc->wait_state & WT_INTERAUPTED) { wakeup_proc(proc); }
        return 0;
    }
    return -E_KILLED;
}

int do_kill(int pid) {
    Proc *proc;
    if ((proc = find_proc(pid)) != nullptr) { return __do_kill(proc, E_KILLED); }
    return -E_INVAL;
}

static int load_icode(Proc *current, char *binary) {
    assert(current->mm == nullptr);
    int ret;
    Mm_struct *mm;
    mm = mm_create();
    assert(mm != nullptr);
    mm->proc = current;
    mm->pagetable = proc_pagetable(current);
    assert(mm->pagetable != nullptr);
    mm->brk_start = 0;
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
        if (mm->brk_start < ph->vaddr + ph->memsz) { mm->brk_start = ph->vaddr + ph->memsz; }
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
    mm->brk_start = mm->brk = PGROUNDUP(mm->brk_start);
    ret = mm_map(mm, USTACKADDR, USTACKSIZE, VM_USER | VM_STACK | VM_READ | VM_WRITE | VM_EXEC, nullptr);
    assert(ret == 0);
    Page *pages = alloc_pages(USTACKPAGE);
    assert(pages != nullptr);
    ret = pages_insert(mm->pagetable, pages, USTACKADDR, PTE_W | PTE_R | PTE_U | PTE_X | PTE_S, USTACKPAGE);
    assert(ret == 0);
    if (current != initproc) {
        acquire(&proc_mm_list_lock);
        list_add(&proc_mm_list, &mm->proc_mm_link);
        release(&proc_mm_list_lock);
    }
    mm_count_inc(mm);
    current->mm = mm;
    ret = 0;
    return ret;
}

int do_execve(char *path, char **argv) {
    int ret;
    Proc *current = myproc();
    Mm_struct *mm = current->mm;
    lock_mm(mm);
    if (mm != nullptr) {
        if (mm_count_dec(mm) == 0) {
            exit_mmap(mm);
            put_pagetable(mm);
            acquire(&proc_mm_list_lock);
            list_del(&mm->proc_mm_link);
            release(&proc_mm_list_lock);
            mm_destroy(mm);
        }
        current->mm = nullptr;
    }
    unlock_mm(mm);
    if (myproc() == initproc) {
        put_fs(current);
        assert((current->fs_struct = fs_create()) != nullptr);
        fs_count_inc(current->fs_struct);
        char bootfs_name[] = "disk0:";
        vfs_set_bootfs(bootfs_name);
    }
    put_sem_queue(current);
    assert((current->sem_queue = sem_queue_create()) != nullptr);
    atomic_inc(&current->sem_queue->count);
    // char bootfs_name[] = "disk0:";
    // vfs_set_bootfs(bootfs_name);
    Stat __stat, *stat = &__stat;
    Inode *inode;
    begin_op();
    ret = vfs_exec(path, &inode);
    // assert(ret == 0);
    if (ret != 0) {
        end_op();
        return ret;
    }
    vop_fstat(inode, stat);
    size_t load_size = stat->st_size;
    size_t load_pages = PGROUNDUP(load_size) / PGSIZE;
    Page *pages = alloc_pages(load_pages);
    char *binary = (char *)page2kva(pages);
    Iobuf __iob, *iob = iobuf_init(&__iob, binary, load_size, 0);
    ret = vop_read(inode, iob);
    assert(ret == 0 && iobuf_used(iob) == load_size);
    vop_ref_dec(inode);
    end_op();
    char *s, *last;
    for (last = s = path; *s; s++) {
        if (*s == '/') last = s + 1;
    }
    if ((path = strchr(path, '/')) != nullptr) {
        *--last = 0;
    } else {
        path = nullptr;
    }
    safestrcpy(current->name, last, sizeof(current->name));
    if (path != nullptr) { vfs_chdir(path); }
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
        lock_mm(current->mm);
        copy_kernel2user(current->mm->pagetable, sp, argv[argc], strlen(argv[argc]) + 1);
        unlock_mm(current->mm);
        ustack[argc] = sp;
    }
    ustack[argc] = 0;
    sp -= (argc + 1) * sizeof(uint64_t);
    sp -= sp % 16;
    assert(sp >= stackbase);
    lock_mm(current->mm);
    copy_kernel2user(current->mm->pagetable, sp, (char *)ustack, (argc + 1) * sizeof(uint64_t));
    unlock_mm(current->mm);
    current->trapframe->a1 = sp;
    current->trapframe->epc = ((struct elfhdr *)binary)->entry;
    current->trapframe->sp = sp;
    current->trapframe->a0 = argc;
    free_pages(pages, load_pages);
    return argc;
}

int do_brk(uintptr_t *brk_store) {
    assert(myproc() != nullptr);
    Mm_struct *mm = myproc()->mm;
    if (myproc()->kernel_proc == true) panic("kernel thread call sys_brk!!!\n");
    if (brk_store == nullptr) return -E_INVAL;
    uintptr_t brk;
    lock_mm(mm);
    either_copy_user2kernel(&brk, 1, (uint64_t)brk_store, sizeof(uintptr_t));
    if (brk < mm->brk_start) goto out_unlock;
    uintptr_t newbrk = PGROUNDUP(brk), oldbrk = mm->brk;
    assert(oldbrk % PGSIZE == 0);
    if (newbrk == oldbrk) goto out_unlock;
    if (newbrk < oldbrk) {
        if (mm_unmap(mm, newbrk, oldbrk - newbrk) != 0) goto out_unlock;
    } else {
        if (find_vma_intersection(mm, oldbrk, newbrk) != nullptr) goto out_unlock;
        if (mm_brk(mm, oldbrk, newbrk - oldbrk) != 0) goto out_unlock;
    }
    mm->brk = newbrk;
out_unlock:
    copy_kernel2user(myproc()->mm->pagetable, (uintptr_t)brk_store, (char *)&mm->brk, sizeof(mm->brk));
    unlock_mm(mm);
    return 0;
}

static inline Timer *timer_proc_init(Timer *timer, Proc *proc, ulong expires) {
    timer->timer_type = TIMER_PROC;
    timer->expires = expires;
    timer->proc = proc;
    list_init(&timer->timer_link);
    return timer;
}

static inline Timer *timer_func_init(Timer *timer, Timer_func func, void *arg, ulong expires,
                                     uint32_t flags) {
    timer->timer_type = TIMER_FUNC;
    timer->expires = expires;
    timer->func = func;
    timer->arg = arg;
    timer->expires = timer->reload = expires;
    timer->flags = flags;
    list_init(&timer->timer_link);
    return timer;
}

static void add_timer(Timer *timer) {
    // acquire(&timer_lock);
    assert(timer->expires >= 0);
    assert(list_empty(&timer->timer_link));
    List_entry *le = list_next(&timer_list);
    while (le != &timer_list) {
        Timer *next = le2timer(le, timer_link);
        if (timer->expires < next->expires) {
            next->expires -= timer->expires;
            break;
        }
        if(timer->expires < next->expires){
            timer->expires = 0;
        }
        else{
            timer->expires -= next->expires;
        }
        le = list_next(le);
    }
    list_add_before(le, &timer->timer_link);
    // release(&timer_lock);
}

static void del_timer(Timer *timer) {
    if (!list_empty(&timer->timer_link)) {
        if (timer->expires != 0) {
            List_entry *le = list_next(&timer->timer_link);
            if (le != &timer_list) {
                Timer *next = le2timer(le, timer_link);
                next->expires += timer->expires;
            }
        }
        list_del_init(&timer->timer_link);
    }
}

void timer_dump(void) {
    int index = 0;
    List_entry *le = list_next(&timer_list);
    if (le == &timer_list) { return; }
    acquire(&print_struct_lock);
    cprintf("\ntimer dump....................................\n\n");
    while (le != &timer_list) {
        Timer *timer = le2timer(le, timer_link);
        if (timer->timer_type == TIMER_PROC) {
            cprintf("%d timer's proc pid is : %d  expires is %lu \n", index++, timer->proc->pid,
                    timer->expires);
        } else {
            assert(timer->timer_type == TIMER_FUNC);
            cprintf("%d timer's func address is : %x  expires is %lu \t", index++, timer->func,
                    timer->expires);
            cprintf("is reload : %s  reload value : %lu\n", timer->flags & TIMER_RELOAD ? "true" : "false",
                    timer->reload);
        }
        le = list_next(le);
    }
    cprintf("\nend of timer dump.............................\n");
    release(&print_struct_lock);
}

void timer_func_dump(void) {
    int index = 0;
    // acquire(&timer_lock);
    List_entry *le = list_next(&timer_func_list);
    if (le == &timer_func_list) { return; }
    acquire(&print_struct_lock);
    cprintf("\n......timer_func dump....................................\n\n");
    while (le != &timer_func_list) {
        Timer *timer = le2timer(le, timer_link);
        assert(timer->timer_type == TIMER_FUNC);
        cprintf("%d timer's func address is : %x  expires is %lu \t", index++, timer->func, timer->expires);
        cprintf("is reload : %s  reload value : %lu\n", timer->flags & TIMER_RELOAD ? "true" : "false",
                timer->reload);
        le = list_next(le);
    }
    cprintf("\n......end of timer_func dump.............................\n");
    release(&print_struct_lock);
}

void run_timer_list(void) {
    if (timer_init_ok == false) return;
    List_entry free_list;
    list_init(&free_list);
    acquire(&timer_lock);
    List_entry *le = list_next(&timer_list);
    if (le != &timer_list) {
        Timer *timer = le2timer(le, timer_link);
        if (timer->expires > 0) { timer->expires--; }
        while (timer->expires == 0) {
            le = list_next(le);
            if (timer->timer_type == TIMER_PROC) {
                Proc *proc = timer->proc;
                // assert((proc->wait_state & WT_TIMER) == WT_TIMER);
                if (proc->wait_state != 0) { assert(proc->wait_state & WT_INTERAUPTED); }
                wakeup_proc(proc);
                list_del_init(&timer->timer_link);
            } else {
                assert(timer->timer_type == TIMER_FUNC);
                list_del_init(&timer->timer_link);
                list_add_after(&free_list, &timer->timer_link);
            }
            if (le == &timer_list) { break; }
            timer = le2timer(le, timer_link);
        }
    }
    release(&timer_lock);
    acquire(&timer_func_lock);
    while ((le = list_next(&free_list)) != &free_list) {
        list_del(le);
        list_add_after(&timer_func_list, le);
    }
    release(&timer_func_lock);
}

int do_sleep(ulong time) {
    if (time == 0) return 0;
    Proc *current = myproc();
    acquire(&timer_lock);
    Timer __timer, *timer = timer_proc_init(&__timer, current, time);
    current->wait_state = WT_TIMER;
    add_timer(timer);
    sleeping(current, &timer_lock);
    del_timer(timer);
    release(&timer_lock);
    return 0;
}

Timer *timer_func_add(Timer_func func, void *arg, ulong time, uint32_t flags) {
    if (time == 0) {
        warn("timer_func_add time should not equal 0");
        return nullptr;
    }
    Timer *__timer = kmalloc(sizeof(Timer));
    assert(__timer != nullptr);
    acquire(&timer_lock);
    Timer *timer = timer_func_init(__timer, func, arg, time, flags);
    add_timer(timer);
    release(&timer_lock);
    return timer;
}

void del_func_timer(Timer *timer) {
    acquire(&timer_lock);
    acquire(&timer_func_lock);
    del_timer(timer);
    release(&timer_func_lock);
    release(&timer_lock);
    assert(timer->timer_type == TIMER_FUNC);
    kfree(timer);
}

void exec_timer_func(void) {
    List_entry free_list, *le;
    list_init(&free_list);
    List_entry do_func_timer_list;
    list_init(&do_func_timer_list);
    acquire(&timer_func_lock);
    while ((le = list_next(&timer_func_list)) != &timer_func_list) {
        list_del_init(le);
        list_add(&free_list, le);
        Timer *timer = le2timer(le, timer_link);
        Timer *do_func_timer = (Timer *)kmalloc(sizeof(Timer));
        assert(do_func_timer != nullptr);
        *do_func_timer = *timer;
        list_add(&do_func_timer_list, &do_func_timer->timer_link);
    }
    release(&timer_func_lock);
    // list_for_each(le, &free_list) {
    //     Timer *timer = le2timer(le, timer_link);
    //     if(!(timer->timer_type == TIMER_FUNC && timer->expires == 0)){
    //         int pp = 3;
    //     }
    //     assert(timer->timer_type == TIMER_FUNC && timer->expires == 0);
    //     timer->func(timer);
    // }
    while ((le = list_next(&do_func_timer_list)) != &do_func_timer_list) {
        Timer *timer = le2timer(le, timer_link);
        assert(timer->timer_type == TIMER_FUNC && timer->expires == 0);
        timer->func(timer);
        list_del(le);
        kfree(timer);
    }
    acquire(&timer_lock);
     while ((le = list_next(&free_list)) != &free_list){
         Timer *timer = le2timer(le, timer_link);
         list_del_init(le);
         if (timer->flags & TIMER_RELOAD) {
             timer->expires = timer->reload;
             add_timer(timer);
         }
     }
    release(&timer_lock);
}

void ipc_add_timer(Timer *timer) {
    if (timer != nullptr) {
        acquire(&timer_lock);
        add_timer(timer);
        release(&timer_lock);
    }
}

void ipc_del_timer(Timer *timer) {
    if (timer != nullptr) {
        acquire(&timer_lock);
        del_timer(timer);
        release(&timer_lock);
    }
}

Timer *ipc_timer_init(ulong timeout, ulong *saved_ticks, Timer *timer) {
    if (timeout != 0) {
        *saved_ticks = ticks;
        return timer_proc_init(timer, myproc(), timeout);
    }
    return nullptr;
}

int ipc_check_timeout(ulong timeout, ulong saved_ticks) {
    ulong delt;
    if (timeout != 0) {
        delt = (ulong)(ticks - saved_ticks);
        if (delt >= timeout - 1) { return -E_TIMEOUT; }
    }
    return -1;
}

int do_mmap(uintptr_t *addr_store, size_t len, uint32_t mmap_flags) {
    Mm_struct *mm = myproc()->mm;
    if (mm == nullptr) panic("kernel thread call mmap!!.\n");
    if (addr_store == nullptr || len == 0) return -E_INVAL;
    int ret = -E_INVAL;
    uintptr_t addr;
    lock_mm(mm);
    either_copy_user2kernel(&addr, 1, (uint64_t)addr_store, sizeof(uintptr_t));
    uintptr_t start = PGROUNDDOWN(addr), end = PGROUNDUP(addr + len);
    addr = start, len = end - start;
    uint32_t vm_flags = VM_READ | VM_USER;
    if (mmap_flags & MMAP_WRITE) vm_flags |= VM_WRITE;
    if (mmap_flags & MMAP_STACK) vm_flags |= VM_STACK;
    ret = -E_NO_MEM;
    if (addr == 0) {
        if ((addr = get_unmapped_area(mm, len)) < 0) { goto out_unlock; }
    }
    if ((ret = mm_map(mm, addr, len, vm_flags, nullptr)) == 0) {
        copy_kernel2user(mm->pagetable, (uintptr_t)addr_store, (char *)&addr, sizeof(addr));
    }
out_unlock:
    unlock_mm(mm);
    return ret;
}

int do_munmap(uintptr_t addr, size_t len) {
    Mm_struct *mm = myproc()->mm;
    if (mm == nullptr) panic("kernel thread call mmap!!.\n");
    if (len == 0) return -E_INVAL;
    int ret;
    lock_mm(mm);
    ret = mm_unmap(mm, addr, len);
    unlock_mm(mm);
    return ret;
}

int do_shmem(uintptr_t *addr_store, size_t len, uint32_t mmap_flags) {
    Mm_struct *mm = myproc()->mm;
    if (mm == nullptr) panic("kernel thread call mmap!!.\n");
    if (addr_store == nullptr || len == 0) return -E_INVAL;
    int ret = -E_INVAL;
    uintptr_t addr;
    lock_mm(mm);
    either_copy_user2kernel(&addr, 1, (uint64_t)addr_store, sizeof(uintptr_t));
    uintptr_t start = PGROUNDDOWN(addr), end = PGROUNDUP(addr + len);
    addr = start, len = end - start;
    uint32_t vm_flags = VM_READ | VM_USER;
    if (mmap_flags & MMAP_WRITE) vm_flags |= VM_WRITE;
    if (mmap_flags & MMAP_STACK) vm_flags |= VM_STACK;
    ret = -E_NO_MEM;
    if (addr == 0) {
        if ((addr = get_unmapped_area(mm, len)) < 0) { goto out_unlock; }
    }
    Shmem_struct *shmem;
    if ((shmem = shmem_create(len)) == nullptr) goto out_unlock;
    if ((ret = mm_map_shmem(mm, addr, vm_flags, shmem, nullptr)) != 0) {
        assert(shmem_ref(shmem) == 0);
        shmem_destroy(shmem);
        goto out_unlock;
    }
    copy_kernel2user(mm->pagetable, (uintptr_t)addr_store, (char *)&addr, sizeof(addr));
out_unlock:
    unlock_mm(mm);
    return ret;
}

void clean_kernel_proc(void) {
    Proc *p;
    List_entry *le = &proc_list;
    acquire(&procs_lock);
    while ((le = list_next(le)) != &proc_list) {
        p = le2proc(le, list_link);
        acquire(&p->lock);
        if (p->state == ZOMBIE && p->kernel_proc == true) {
            // unmap_range(p->mm->pagetable, TRAPFRAME(p->mm_index), TRAPFRAME(p->mm_index) + PGSIZE);
            assert(mm_count(p->mm) == 0);
            // exit_mmap(p->mm);
            // put_pagetable(p->mm);
            acquire(&proc_mm_list_lock);
            list_del(&p->mm->proc_mm_link);
            release(&proc_mm_list_lock);
            mm_destroy(p->mm);
            unhash_proc(p);
            remove_links(p);
            release(&p->lock);
            free_proc(p);
            continue;
        }
        release(&p->lock);
    }
    release(&procs_lock);
}
