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
#include "trap.h"

Cpu cpus[NCPU];
// Proc proc[NPROC];
List_entry proc_list;

#define HASH_SHIFT 10
#define HASH_LIST_SIZE (1 << HASH_SHIFT)
#define pid_hashfn(x) (hash64(x, HASH_SHIFT))

static List_entry hash_list[HASH_LIST_SIZE];

Proc *initproc = nullptr;

static ulong next_pid = 1;
static ulong nr_process = 0;
Spinlock procs_lock;
extern char trampoline[];  // trampoline.S
extern char kernel_etext[];

void swtch(struct context *, struct context *);

ulong alloc_pid() {
    ulong pid;
    // acquire(&procs_lock);
    Proc *proc;
    List_entry *le = &proc_list;
    pid = next_pid;
repeat:
    if(++next_pid == MAX_PID)
        next_pid = 1;
    while ((le = list_next(le)) != &proc_list) {
        proc = le2proc(le, list_link);
        if(proc->pid == next_pid)
            goto repeat;
    }
    // release(&procs_lock);
    return pid;
}

void proc_init(void) {
    initlock(&procs_lock, "procs_lock");
    list_init(&proc_list);
    for(int i = 0; i < HASH_LIST_SIZE; i++){
        list_init(hash_list + i);
    }
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

static void hash_proc(Proc *proc){
    list_add(hash_list + pid_hashfn(proc->pid), &proc->hash_link);
}

Proc *alloc_proc() {
    Proc *proc = kmalloc(sizeof(*proc));
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
    uintptr_t pa = (uintptr_t)page2kva(AllocPage());
    assert(pa != 0);
    uint64_t va = KSTACK((ulong)(nr_process));
    kvmmap(va, (uint64_t)pa, PGSIZE, PTE_R | PTE_W);
    proc->kstack = va;
    memset(&proc->context, 0, sizeof(proc->context));
    proc->context.sp = (uint64_t)(proc->kstack + PGSIZE);
    return proc;
}


char *set_proc_name(Proc *proc, const char *name){
    memset(proc->name, 0, sizeof(proc->name));
    return memcpy(proc->name, name, PROC_NAME_LEN);
}

char *get_proc_name(Proc *proc){
    static char name[PROC_NAME_LEN + 1];
    memset(name, 0, sizeof(name));
    return memcpy(name, proc->name, PROC_NAME_LEN);
}

static void uvm_init(pagetable_t *pagetable) {
    uintptr_t mem = page2kva(alloc_pages(USTACKPAGE));
    mappages(pagetable, USTACKADDR, USTACKSIZE, mem, PTE_W | PTE_R | PTE_U | PTE_X);
    mappages(pagetable, KERNBASE, (uint64_t)kernel_etext - KERNBASE, KERNBASE, PTE_R | PTE_U | PTE_X);
    mappages(pagetable, (uint64_t)kernel_etext, PHYSTOP - (uint64_t)kernel_etext, (uint64_t)kernel_etext,
             PTE_R | PTE_W | PTE_U);
}

void user_init(void (*start_routin)(void)) {
    Proc *p;
    p = alloc_proc();
    assert(p != nullptr);
    p->mm->pagetable = proc_pagetable(p);
    uvm_init(p->mm->pagetable);
    p->state = RUNNABLE;
    p->kernel_proc = 0;
    p->context.ra = (uint64_t)fork_ret;
    p->trapframe->epc = (uint64_t)start_routin;  // user program counter
    p->trapframe->sp = PGSIZE + USTACKSIZE;
    acquire(&procs_lock);
    p->pid = alloc_pid();
    snprintf(p->name, sizeof(p->name), "u_process_%d", p->pid);
    hash_proc(p);
    list_add(&proc_list, &p->list_link);
    if (++nr_process >= NPROC) panic("Exceeded maximum number of processes");
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
    if(arg != nullptr)
        proc->context.a1 = (uint64_t)arg;
    proc->state = RUNNABLE;
    proc->kernel_proc = true;
    acquire(&procs_lock);
    proc->pid = alloc_pid();
    snprintf(proc->name, sizeof(proc->name), "k_process_%d", proc->pid);
    hash_proc(proc);
    list_add(&proc_list, &proc->list_link);
    if (++nr_process >= NPROC) panic("Exceeded maximum number of processes");
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

void yield(void) {
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
        if (prev != nullptr) {
            last = &prev->list_link;
        } else {
            last = &proc_list;
        }
        // last = (prev == nullptr) ? &proc_list : &prev->list_link;
        le = last;
        do{
            if((le = list_next(le)) != &proc_list){
                next = le2proc(le, list_link);
                if (next->state == RUNNABLE) break;
            }
        }while(le != last);
        release(&procs_lock);
        if (next != nullptr && next->state == RUNNABLE) {
            acquire(&next->lock);
            if (next->state == RUNNABLE){
                next->state = RUNNING;
                next->runs++;
                c->proc = next;
                swtch(&c->context, &next->context);
                prev = c->proc;
                c->proc = 0;
            }
            release(&next->lock);
        }
        else {
            intr_on();
            asm volatile("wfi");
        }
    }
}

void either_copy_from_user2kernel(void *dst, int user_src, uint64_t src, uint64_t len) {
    Proc *p = myproc();
    if (user_src)
        copy_from_user2kernel(p->mm->pagetable, dst, src, len);
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
    while((le = list_next(le)) != &proc_list){
        p = le2proc(le, list_link);
        acquire(&p->lock);
        if (p->state == SLEEPING && p->chan == chan) { p->state = RUNNABLE; }
        release(&p->lock);
    }
    release(&procs_lock);
}
