#include "sched_FCFS.h"
#include "proc.h"
#include "list.h"


static void FCFS_init(Cpu *cpu){
    list_init(&cpu->rq.run_list);
    // cpu->rq.proc_num = 0;
    atomic_set(&cpu->rq.proc_num, 0);
}

static void FCFS_enqueue(Cpu *cpu, Proc *proc) {
    assert(list_empty(&proc->run_link));
    list_add_before(&cpu->rq.run_list, &proc->run_link);
    // cpu->rq.proc_num++;
    atomic_inc(&cpu->rq.proc_num);
}

static void FCFS_dequeue(Cpu *cpu, Proc *proc){
    // assert(!list_empty(&proc->run_link) && proc->cpu == mycpu());
    assert(!list_empty(&proc->run_link));
    list_del_init(&proc->run_link);
    // cpu->rq.proc_num--;
    atomic_dec(&cpu->rq.proc_num);
}

static Proc *FCFS_pick_next(Cpu *cpu){
    Proc *next;
    List_entry *le = list_next(&cpu->rq.run_list);
    while(le != &cpu->rq.run_list){
        next = le2proc(le, run_link);
        if (next->state == RUNNABLE){
            list_del_init(&next->run_link);
            list_add_before(&cpu->rq.run_list, &next->run_link);
            return next;
        }
        le = list_next(le);
    }
    return nullptr;
}

static void FCFS_proc_tick(Proc *proc){
     /* do nothing */
}

Sched_class FCFS_sched_class = {
    .name = "FCFS_scheduler",
    .init = FCFS_init,
    .enqueue = FCFS_enqueue,
    .dequeue = FCFS_dequeue,
    .pick_next = FCFS_pick_next,
    .proc_tick = FCFS_proc_tick,
};