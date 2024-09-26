#include "list.h"
#include "proc.h"
#include "sched_RR.h"

static void RR_init(Cpu *cpu) {
    list_init(&cpu->rq.run_list);
    // cpu->rq.proc_num = 0;
    atomic_set(&cpu->rq.proc_num, 0);
}

static void RR_enqueue(Cpu *cpu, Proc *proc) {
    assert(list_empty(&proc->run_link));
    List_entry *le = list_next(&cpu->rq.run_list);
    while (le != &cpu->rq.run_list) {
        Proc *next = le2proc(le, run_link);
        if (proc->time_slice > next->time_slice) { break; }
        le = list_next(le);
    }
    list_add_before(le, &proc->run_link);
    if (proc->time_slice == 0 || proc->time_slice > cpu->rq.max_time_slice) {
        proc->time_slice = cpu->rq.max_time_slice;
    }
    else if(proc->time_slice < cpu->rq.max_time_slice){
        proc->time_slice = cpu->rq.min_time_slice;
    }
    atomic_inc(&cpu->rq.proc_num);
}

static void RR_dequeue(Cpu *cpu, Proc *proc) {
    // assert(!list_empty(&proc->run_link) && proc->cpu == mycpu());
    assert(!list_empty(&proc->run_link));
    list_del_init(&proc->run_link);
    // cpu->rq.proc_num--;
    atomic_dec(&cpu->rq.proc_num);
}

static Proc *RR_pick_next(Cpu *cpu) {
    Proc *next;
    List_entry *le = list_next(&cpu->rq.run_list);
    while (le != &cpu->rq.run_list) {
        next = le2proc(le, run_link);
        if (next->state == RUNNABLE) {
            list_del_init(&next->run_link);
            list_add_before(&cpu->rq.run_list, &next->run_link);
            return next;
        }
        le = list_next(le);
    }
    return nullptr;
}

static void RR_proc_tick(Proc *proc) { 
    if(proc->time_slice > 0){
        proc->time_slice--;
    }
    if(proc->time_slice == 0){
        proc->need_resched = true;
        proc->time_slice = proc->cpu->rq.max_time_slice;
    }
}

static Proc *RR_get_proc(Cpu *cpu) {
    Proc *next;
    List_entry *le = list_next(&cpu->rq.run_list);
    while (le != &cpu->rq.run_list) {
        next = le2proc(le, run_link);
        if (next->state == RUNNABLE) {
            RR_dequeue(cpu, next);
            return next; 
        }
        le = list_next(le);
    }
    return nullptr;
}

static Rq_run_info RR_get_rq_run_info(Cpu *cpu) {
    Rq_run_info info;
    info.proc_count = 0;
    info.rq_total_value = 0;
    Proc *next;
    List_entry *le = list_next(&cpu->rq.run_list);
    while (le != &cpu->rq.run_list) {
        next = le2proc(le, run_link);
        if (next->state == RUNNABLE && next->state == RUNNING) {
            info.rq_total_value += next->alloc_time_slice;
            info.proc_count++;
        }
        le = list_next(le);
    }
    return info;
}

Sched_class RR_sched_class = {
    .name = "RR_scheduler",
    .init = RR_init,
    .enqueue = RR_enqueue,
    .dequeue = RR_dequeue,
    .pick_next = RR_pick_next,
    .proc_tick = RR_proc_tick,
    .get_rq_run_info = RR_get_rq_run_info,
    .get_proc = RR_get_proc,
};