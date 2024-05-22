#include "proc.h"

#include "assert.h"
#include "config.h"
#include "printf.h"
#include "risv.h"

Cpu cpus[NCPU];
Proc proc[NPROC];
uint8_t __attribute__((aligned(16))) task_stack[NPROC][STACK_SIZE];
static int _top = 0;

void swtch(struct context *, struct context *);
void proc_init(void) {
    for (int i = 0; i < NPROC; i++) { proc[i].state = UNUSED; }
}


void task_create(void (*start_routin)(Proc)) {
    if (_top < NPROC - 1) {
        proc[_top].state = RUNNABLE;
        snprintf(proc[_top].name, sizeof(proc[_top].name), "process_%d", _top);
        proc[_top].context.ra = (uint64_t)start_routin;
        proc[_top].context.sp = (uint64_t)&task_stack[_top][STACK_SIZE];
        proc[_top].context.a0 = (uint64_t)&proc[_top];
        _top++;
    } else
        panic("Exceeded maximum number of processes");
}
int fun527(int i){
    int j = i;
    return j;
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
    p->state = RUNNABLE;
    sched();
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
            if(p->state == RUNNABLE){
                p->state = RUNNING;
                c->proc = p;
                swtch(&c->context, &p->context);
                // intr_on();  // Since interrupts are disabled when entering the trap, enable them again here.
                c->proc = 0;
            }
        }   
    } 
}


