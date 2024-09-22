#ifndef __SCHEDULE_FCFS_H__
#define __SCHEDULE_FCFS_H__
#include "stdarg.h"
#include "types.h"
#include "list.h"
#include "atomic.h"
#include "rbtree.h"

typedef struct cpu Cpu;
typedef struct proc Proc;

typedef struct run_queue Run_queue;
struct run_queue {
    List_entry run_list;
    Atomic proc_num;
    ulong max_time_slice;
    ulong min_time_slice;
    Rb_tree *proc_vruntime_rbtree;
};

typedef struct rq_run_info Rq_run_info;
struct rq_run_info{
    ulong rq_total_value;  // for RR mean all runnable proc->time_slice
                           // for CFS mean all runnable proc->priority(weight)
    ulong proc_count;
    ulong rq_total_vruntime;
};

typedef struct sched_class Sched_class;
struct sched_class {
    const char *name;
    void (*init)(Cpu *cpu);
    void (*enqueue)(Cpu *cpu, Proc *proc);
    void (*dequeue)(Cpu *cpu, Proc *proc);
    Proc *(*pick_next)(Cpu *cpu);
    void (*proc_tick)(Proc *proc);
    Rq_run_info (*get_rq_run_info)(Cpu *cpu);
    void (*insert_rbtree)(Proc *proc);
    void (*remove_rbtree)(Proc *proc);
};

void sched_init(void);
void sched_class_enqueue(Proc *proc);
void sched_class_dequeue(Proc *proc);
Proc *sched_class_pick_next(void);
void sched_class_proc_tick(Proc *proc);
void sched_class_insert_rbtree(Proc *proc);
void sched_class_remove_rbtree(Proc *proc);
#endif