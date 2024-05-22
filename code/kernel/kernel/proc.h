#ifndef __KERNEL_PROC_H__
#define __KERNEL_PROC_H__
#include "stdarg.h"
#include "types.h"
typedef enum proc_state Proc_state;
enum proc_state { UNUSED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

typedef struct context Context;
struct context {
    uint64_t ra;
    uint64_t sp;
    uint64_t a0;
    uint64_t a1;
    uint64_t a2;
    uint64_t a3;
    uint64_t a4;
    uint64_t a5;
    uint64_t a6;
    uint64_t a7;

    // callee-saved
    uint64_t s0;
    uint64_t s1;
    uint64_t s2;
    uint64_t s3;
    uint64_t s4;
    uint64_t s5;
    uint64_t s6;
    uint64_t s7;
    uint64_t s8;
    uint64_t s9;
    uint64_t s10;
    uint64_t s11;
};

typedef struct proc Proc;
struct proc {
    char name[64];
    Proc_state state;
    Context context;
};

typedef struct cpu Cpu;
struct cpu {
    struct proc *proc;       // The process running on this cpu, or null.
    struct context context;  // swtch() here to enter scheduler().
};

int cpuid();
void scheduler(void);
void task_delay(volatile int count);
void yield(void);
void sched(void);
Proc *myproc(void);
Cpu *mycpu(void);
void task_create(void (*start_routin)(Proc));
void proc_init(void);
#endif