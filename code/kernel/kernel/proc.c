#include "proc.h"
#include "assert.h"
#include "config.h"
#include "printf.h"
#include "riscv.h"
#include "string.h"
#include "trap.h"

Cpu cpus[NCPU];
Proc proc[NPROC];
uint8_t __attribute__((aligned(16))) task_kernel_stack[NPROC][STACK_SIZE];
uint8_t __attribute__((aligned(16))) task_usert_stack[NPROC][STACK_SIZE];
Trapframe trapframes[NPROC];
ulong next_pid = 1;
Spinlock pid_lock;
static int _top = 0;

void swtch(struct context *, struct context *);

ulong alloc_pid(){
    ulong pid;
    acquire(&pid_lock);
    pid = next_pid;
    next_pid++;
    release(&pid_lock);
    return pid;
}

void proc_init(void) {
    Proc *p;
    for (p = proc; p < &proc[NPROC]; p++) { 
        p->state = UNUSED; 
        initlock(&p->lock, "proc");
        p->kstack = (uint64_t)task_kernel_stack[p - proc];
    }
}



void fork_ret(void){
    release(&myproc()->lock);
    user_trap_ret();
}


Proc *alloc_proc() {
    Proc *p;
    for(p = proc; p < &proc[NPROC]; p++){
        acquire(&p->lock);
        if(p->state == UNUSED)
            goto found;
        else{
            release(&p->lock);
        }
    }
    panic("Exceeded maximum number of processes");
    return nullptr;

found:
    p->pid = alloc_pid();
    int index = p - proc;
    p->trapframe = &trapframes[index];
    memset(&p->context, 0, sizeof(p->context));
    p->context.ra = (uint64_t)fork_ret;
    p->context.sp = (uint64_t)(p->kstack + STACK_SIZE);
    return p;
}

void user_init(void (*start_routin)(void)) {
    struct proc *p;
    p = alloc_proc();
    snprintf(p->name, sizeof(p->name), "process_%d", p - proc);
    p->state = RUNNABLE;
    p->trapframe->epc = (uint64_t)start_routin;      // user program counter
    p->trapframe->sp = (uint64_t)&task_usert_stack[p - proc][STACK_SIZE];
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
    Cpu *c= mycpu();
    Proc *p = c->proc;
    return p;
}

void sched(void){
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


void scheduler(void){
    Proc *p;
    Cpu *c = mycpu();
    c->proc = 0;
    while(1){
        intr_on();
        for(p = proc; p < &proc[NPROC]; p++){
            acquire(&p->lock);
            if(p->state == RUNNABLE){
                p->state = RUNNING;
                c->proc = p;
                swtch(&c->context, &p->context);
                c->proc = 0;
            }
            release(&p->lock);
        }   
    } 
}



