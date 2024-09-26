#include "sched_CFS.h"

#include "list.h"
#include "proc.h"
#include "stdio.h"

#define NICE_OFFSET 20
#define PRIO_0_WEIGHT 1024
extern Cpu cpus[NCPU];
static void CFS_remove_rbtree(Proc *proc);

const int sched_nice_to_weight[40] = {
    /* -20 */ 88761, 71755, 56483, 46273, 36291,
    /* -15 */ 29154, 23254, 18705, 14949, 11916,
    /* -10 */ 9548,  7620,  6100,  4904,  3906,
    /*  -5 */ 3121,  2501,  1991,  1586,  1277,
    /*   0 */ 1024,  820,   655,   526,   423,
    /*   5 */ 335,   272,   215,   172,   137,
    /*  10 */ 110,   87,    70,    56,    45,
    /*  15 */ 36,    29,    23,    18,    15,
};

static void CFS_init(Cpu *cpu) {
    list_init(&cpu->rq.run_list);
    // cpu->rq.proc_num = 0;
    atomic_set(&cpu->rq.proc_num, 0);
    cpu->rq.proc_vruntime_rbtree = rb_tree_create(proc_vruntime_compare_node);
    assert(cpu->rq.proc_vruntime_rbtree != nullptr);
}

static Rq_run_info CFS_get_rq_run_info(Cpu *cpu) {
    Rq_run_info info;
    info.proc_count = 0;
    info.rq_total_value = 0;
    info.rq_total_vruntime = 0;
    Proc *next;
    List_entry *le = list_next(&cpu->rq.run_list);
    while (le != &cpu->rq.run_list) {
        next = le2proc(le, run_link);
        if (next->state == RUNNABLE) {
            info.rq_total_value += sched_nice_to_weight[next->priority + NICE_OFFSET];
            info.rq_total_vruntime += next->vruntime;
            info.proc_count++;
        }
        le = list_next(le);
    }
    return info;
}

static void CFS_enqueue(Cpu *cpu, Proc *proc) {
    assert(proc->priority >= -20 && proc->priority <= 19);
    assert(list_empty(&proc->run_link));
    List_entry *le = list_next(&cpu->rq.run_list);
    while (le != &cpu->rq.run_list) {
        Proc *next = le2proc(le, run_link);
        if (proc->priority > next->priority) {
            break;
        }
        le = list_next(le);
    }
    list_add_before(le, &proc->run_link);
    // cprintf("111111 proc->pid %d cpu id %d\n", proc->pid, cpu - cpus);
    Rq_run_info info = CFS_get_rq_run_info(cpu);
    uint64_t total_time_lantency = ((cpu->rq.min_time_slice + cpu->rq.max_time_slice) * info.proc_count / 2);
    proc->alloc_time_slice = proc->time_slice =
        (total_time_lantency * sched_nice_to_weight[proc->priority + NICE_OFFSET] /
         (info.rq_total_value + 1));
    // assert(proc->time_slice >= cpu->rq.min_time_slice && proc->time_slice <= cpu->rq.max_time_slice);
    if (proc->time_slice < proc->cpu->rq.min_time_slice) {
        proc->alloc_time_slice = proc->time_slice = proc->cpu->rq.min_time_slice;
    }
    if (proc->time_slice > proc->cpu->rq.max_time_slice) {
        proc->alloc_time_slice = proc->time_slice = proc->cpu->rq.max_time_slice;
    }
    proc->vruntime = (info.rq_total_vruntime / (info.proc_count + 1));
    if (proc->state == RUNNABLE) { rb_insert(cpu->rq.proc_vruntime_rbtree, &(proc->rb_link)); }
    atomic_inc(&cpu->rq.proc_num);
}

static void CFS_dequeue(Cpu *cpu, Proc *proc) {
    assert(!list_empty(&proc->run_link));
    list_del_init(&proc->run_link);
    if (proc->state == RUNNABLE) {
        // cprintf("7777777 proc pid is %d cpu id is %d\n", proc->pid, cpu - cpus);
        rb_delete(cpu->rq.proc_vruntime_rbtree, &proc->rb_link); 
    }
    atomic_dec(&cpu->rq.proc_num);
}

static Proc *CFS_pick_next(Cpu *cpu) {
    Proc *next;
    Rb_node *node = rb_node_root(cpu->rq.proc_vruntime_rbtree);
    if (node == nullptr) { return nullptr; }
    Rb_node *prev_node = node;
    while (node != cpu->rq.proc_vruntime_rbtree->nil) {
        prev_node = node;
        node = node->left;
    }
    next = rbn2proc(prev_node);
    assert(next->state == RUNNABLE);
    // rb_delete(cpu->rq.proc_vruntime_rbtree, &next->rb_link);
    // cprintf("888888 proc->pid %d cpu id %d\n", next->pid, next->cpu - cpus);
    CFS_remove_rbtree(next);
    next->state = RUNNING;
    // cprintf("999999 proc->pid %d cpu id %d proc->state %d\n", next->pid, next->cpu - cpus, next->state);
    return next;
}

static Proc *CFS_get_proc(Cpu *cpu) {
    Proc *next;
    List_entry *le = list_next(&cpu->rq.run_list);
    while (le != &cpu->rq.run_list) {
        next = le2proc(le, run_link);
        if (next->state == RUNNABLE) {
            CFS_dequeue(cpu, next);
            return next;
        }
        le = list_next(le);
    }
    return nullptr;
}

static void CFS_insert_rbtree(Proc *proc) {
    assert(proc->state == RUNNING || proc->state == SLEEPING);
    // cprintf("222222 proc->pid %d cpu id %d\n", proc->pid, proc->cpu - cpus);
    if (proc->state == RUNNING) {
        proc->vruntime += ((proc->alloc_time_slice - proc->time_slice) * PRIO_0_WEIGHT /
                           sched_nice_to_weight[proc->priority + NICE_OFFSET]);
        rb_insert(proc->cpu->rq.proc_vruntime_rbtree, &(proc->rb_link));
    } else if (proc->state == SLEEPING) {
        Rq_run_info info = CFS_get_rq_run_info(proc->cpu);
        proc->vruntime = (info.rq_total_vruntime / (info.proc_count + 1));
        rb_insert(proc->cpu->rq.proc_vruntime_rbtree, &(proc->rb_link));
    }
    proc->state = RUNNABLE;
    // proc_dump();
}

static void CFS_remove_rbtree(Proc *proc) {
    // assert(proc->state == RUNNING);
    rb_delete(proc->cpu->rq.proc_vruntime_rbtree, &proc->rb_link);
    // cprintf("333333 proc->pid %d cpu id %d\n", proc->pid, proc->cpu - cpus);
    Rq_run_info info = CFS_get_rq_run_info(proc->cpu);
    uint64_t total_time_lantency =
        ((proc->cpu->rq.min_time_slice + proc->cpu->rq.max_time_slice) * (info.proc_count + 1) / 2);
    proc->alloc_time_slice = proc->time_slice =
        (total_time_lantency * sched_nice_to_weight[proc->priority + NICE_OFFSET] /
         (info.rq_total_value + 1));
    // assert(proc->time_slice >= proc->cpu->rq.min_time_slice && proc->time_slice <=
    // proc->cpu->rq.max_time_slice);
    if (proc->time_slice < proc->cpu->rq.min_time_slice) {
        proc->alloc_time_slice = proc->time_slice = proc->cpu->rq.min_time_slice;
    }
    if (proc->time_slice > proc->cpu->rq.max_time_slice) {
        proc->alloc_time_slice = proc->time_slice = proc->cpu->rq.max_time_slice;
    }
}

static void CFS_proc_tick(Proc *proc) {
    proc->runs++;
    if (proc->time_slice > 0) { proc->time_slice--; }
    if (proc->time_slice == 0) { proc->need_resched = true; }
    // if(proc->runs == 5000){
    //     proc_dump();
    //     while(1);
    // }
}

Sched_class CFS_sched_class = {
    .name = "CFS_scheduler",
    .init = CFS_init,
    .enqueue = CFS_enqueue,
    .dequeue = CFS_dequeue,
    .pick_next = CFS_pick_next,
    .proc_tick = CFS_proc_tick,
    .get_rq_run_info = CFS_get_rq_run_info,
    .insert_rbtree = CFS_insert_rbtree,
    .remove_rbtree = CFS_remove_rbtree,
    .get_proc = CFS_get_proc,
};