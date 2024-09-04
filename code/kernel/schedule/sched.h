#ifndef __SCHEDULE_FCFS_H__
#define __SCHEDULE_FCFS_H__
#include "stdarg.h"
#include "types.h"
#include "list.h"
#include "atomic.h"

typedef struct cpu Cpu;
typedef struct proc Proc;

typedef struct run_queue Run_queue;
struct run_queue {
    List_entry run_list;
    Atomic proc_num;
    ulong max_time_slice;
};

typedef struct sched_class Sched_class;
struct sched_class {
    const char *name;
    void (*init)(Cpu *cpu);
    void (*enqueue)(Cpu *cpu, Proc *proc);
    void (*dequeue)(Cpu *cpu, Proc *proc);
    Proc *(*pick_next)(Cpu *cpu);
    void (*proc_tick)(Proc *proc);
};

void sched_init(void);
void sched_class_enqueue(Proc *proc);
void sched_class_dequeue(Proc *proc);
Proc *sched_class_pick_next(void);
void sched_class_proc_tick(Proc *proc);
#endif