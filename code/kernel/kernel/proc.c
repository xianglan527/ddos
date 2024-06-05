#include "proc.h"

#include "assert.h"
#include "config.h"
#include "printf.h"
#include "riscv.h"
#include "spinlock.h"
#include "string.h"
#include "trap.h"

Cpu cpus[NCPU];
Proc proc[NPROC];
Trapframe trapframes[NPROC];
ulong next_pid = 1;
Spinlock pid_lock;
Spinlock user_lock;
extern char trampoline[];  // trampoline.S
extern char kernel_etext[];

void swtch(struct context *, struct context *);

ulong alloc_pid() {
    ulong pid;
    acquire(&pid_lock);
    pid = next_pid;
    next_pid++;
    release(&pid_lock);
    return pid;
}

void proc_init(void) {
    Proc *p;
    initlock(&pid_lock, "nextpid");
    for (p = proc; p < &proc[NPROC]; p++) {
        p->state = UNUSED;
        initlock(&p->lock, "proc");
        uintptr_t pa = page2pa(AllocPage());
        // assert(pa != nullptr);
        uint64_t va = KSTACK((int)(p - proc));
        kvmmap(va, (uint64_t)pa, PGSIZE, PTE_R | PTE_W);
        p->kstack = va;
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
    mappages(pagetable, TRAMPOLINE, PGSIZE, (uint64_t)trampoline, PTE_R | PTE_X );

    mappages(pagetable, TRAPFRAME, PGSIZE, (uint64_t)(p->trapframe), PTE_R | PTE_W);
    return  pagetable;
}

Proc *alloc_proc() {
    Proc *p;
    for (p = proc; p < &proc[NPROC]; p++) {
        acquire(&p->lock);
        if (p->state == UNUSED)
            goto found;
        else { release(&p->lock); }
    }
    panic("Exceeded maximum number of processes");
    return nullptr;

found:
    p->pid = alloc_pid();
    p->trapframe = (Trapframe *)page2pa(AllocPage());
    memset((void *)(p->trapframe), 0, PGSIZE);
    assert(p->trapframe != nullptr);
    p->pagetable = proc_pagetable(p);
    assert(p->pagetable != nullptr);
    memset(&p->context, 0, sizeof(p->context));
    p->context.ra = (uint64_t)fork_ret;
    p->context.sp = (uint64_t)(p->kstack + PGSIZE);
    return p;
}

static void uvm_init(pagetable_t *pagetable){
    uintptr_t mem = page2pa(AllocPage());
    mappages(pagetable, PGSIZE, PGSIZE, mem, PTE_W|PTE_R|PTE_U|PTE_X);
    mappages(pagetable, KERNBASE, (uint64_t)kernel_etext - KERNBASE, KERNBASE, PTE_R | PTE_U | PTE_X);
    mappages(pagetable, (uint64_t)kernel_etext, PHYSTOP - (uint64_t)kernel_etext, (uint64_t)kernel_etext,
             PTE_R | PTE_W | PTE_U);
}

void user_init(void (*start_routin)(void)) {
    struct proc *p;
    p = alloc_proc();
    snprintf(p->name, sizeof(p->name), "process_%d", p - proc);
    uvm_init(p->pagetable);
    p->state = RUNNABLE;
    p->trapframe->epc = (uint64_t)start_routin;  // user program counter
    p->trapframe->sp = 2 * PGSIZE;
    release(&p->lock);
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
    Proc *p;
    Cpu *c = mycpu();
    c->proc = 0;
    while (1) {
        intr_on();
        for (p = proc; p < &proc[NPROC]; p++) {
            acquire(&p->lock);
            if (p->state == RUNNABLE) {
                p->state = RUNNING;
                c->proc = p;
                swtch(&c->context, &p->context);
                c->proc = 0;
            }
            release(&p->lock);
        }
    }
}

void user_lock_init(void) { initlock(&user_lock, "user_lock"); }

void either_copy_from_user2kernel(void *dst, int user_src, uint64_t src, uint64_t len){
    Proc *p = myproc();
    if(user_src)
        copy_from_user2kernel(p->pagetable, dst, src, len);
    else{
        memmove(dst, (char *)src, len); 
    }
}