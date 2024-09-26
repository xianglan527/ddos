#include "sched.h"

#include "atomic.h"
#include "config.h"
#include "proc.h"
#include "rbtree.h"
#include "sched_CFS.h"
#include "sched_RR.h"
#include "spinlock.h"
#include "stdio.h"

extern Cpu cpus[NCPU];
extern Atomic cpus_num;

void sched_init(void) {
    Cpu *cpu = mycpu();
    cpu->sc = &CFS_sched_class;
    // cpu->sc = &RR_sched_class;
    initlock(&cpu->cpu_lock, "cpu_lock");
    cpu->rq.max_time_slice = MAX_TIME_SLICE;
    cpu->rq.min_time_slice = MIN_TIME_SLICE;
    cpu->sc->init(cpu);
    for (int i = 0; i < CPU_LOAD_IDX_MAX; i++) { cpu->rq.cpu_load[i] = 0; }
}

void sched_class_enqueue(Proc *proc) {
    Cpu *cpu;
    if (proc->set_cpu != nullptr) {
        // cprintf("55555 : proc->cpu is %d  setcpu is %d\n", proc->cpu - cpus, proc->set_cpu - cpus);
        cpu = proc->set_cpu;
        goto insert_proc;
    }
    long min_proc_num = atomic_read(&cpus[0].rq.proc_num);
    int min_proc_num_index = 0;
    for (int i = 1; i < NCPU; i++) {
        if (cpus[i].sc != nullptr && atomic_read(&cpus[i].rq.proc_num) < min_proc_num) {
            min_proc_num = atomic_read(&cpus[i].rq.proc_num);
            min_proc_num_index = i;
        }
    }
    cpu = &cpus[min_proc_num_index];
insert_proc:
    proc->cpu = cpu;
    acquire(&cpu->cpu_lock);
    cpu->sc->enqueue(cpu, proc);
    release(&cpu->cpu_lock);
}

void sched_class_dequeue(Proc *proc) {
    Cpu *cpu = proc->cpu;
    acquire(&cpu->cpu_lock);
    cpu->sc->dequeue(cpu, proc);
    release(&cpu->cpu_lock);
}

Proc *sched_class_pick_next(void) {
    Cpu *cpu = mycpu();
    acquire(&cpu->cpu_lock);
    Proc *proc = cpu->sc->pick_next(cpu);
    release(&cpu->cpu_lock);
    return proc;
}

void sched_class_insert_rbtree(Proc *proc) {
    Cpu *cpu = proc->cpu;
    acquire(&cpu->cpu_lock);
    cpu->sc->insert_rbtree(proc);
    release(&cpu->cpu_lock);
}

void sched_class_remove_rbtree(Proc *proc) {
    Cpu *cpu = proc->cpu;
    acquire(&cpu->cpu_lock);
    cpu->sc->remove_rbtree(proc);
    release(&cpu->cpu_lock);
}

void sched_class_proc_tick(Proc *proc) { mycpu()->sc->proc_tick(proc); }

Rq_run_info sched_class_get_rq_run_info(Cpu *cpu) {
    acquire(&cpu->cpu_lock);
    Rq_run_info info = cpu->sc->get_rq_run_info(cpu);
    release(&cpu->cpu_lock);
    return info;
}

// As the value of j increases, cpu_load[j] is less affected by the real-time value of load, representing the
// average load over a longer period. Meanwhile, cpu_load[0] indicates the real-time load.
void updata_cpu_rq_load() {
    long local_cpus_num = atomic_read(&cpus_num);
    assert(local_cpus_num > 0 && local_cpus_num <= NCPU);
    ulong max = ~0UL;
    ulong min = 0;
    for (int i = 0; i < local_cpus_num; i++) {
        for (int j = 0, scale = 1; j < CPU_LOAD_IDX_MAX; j++, scale += scale) {
            ulong old_load = cpus[i].rq.cpu_load[j];
            Rq_run_info info = sched_class_get_rq_run_info(&cpus[i]);
            ulong new_load = info.rq_total_value;
            // round up
            if (new_load > old_load) { new_load += scale - 1; }
            // cpus[i].rq.cpu_load[j] = old_load + (new_load - old_load) / 2^j
            cpus[i].rq.cpu_load[j] = (old_load * (scale - 1) + new_load) >> j;
        }
    }
}

Proc *sched_class_get_proc(Cpu *cpu) {
    acquire(&cpu->cpu_lock);
    Proc *proc = cpu->sc->get_proc(cpu);
    release(&cpu->cpu_lock);
    return proc;
}

void load_balance(int cpu_load_index) {
    ulong min = ~0UL, min_index = 0, max_index = 0;
    ulong max = 0;
    Proc *proc;
    ulong load_value;
    long local_cpus_num = atomic_read(&cpus_num);
    for (int i = 0; i < local_cpus_num; i++) {
        if(min > cpus[i].rq.cpu_load[cpu_load_index]){
            min = cpus[i].rq.cpu_load[cpu_load_index];
            min_index = i;
        }
        if(max < cpus[i].rq.cpu_load[cpu_load_index]){
            max = cpus[i].rq.cpu_load[cpu_load_index];
            max_index = i;
        }
    }
    if(min_index == max_index) return;
    assert(max > min);
    while(max > cpus[max_index].rq.max_time_slice * load_balance_proc_num_threshold &&
        (max - min) > (max / load_balance_diff_threshold)) {
        proc = sched_class_get_proc(&cpus[max_index]);
        if(proc == nullptr) return;
        assert(proc->state == RUNNABLE);
        if(proc->cpu->sc == &CFS_sched_class){
            load_value = proc->priority;
        }else if(proc->cpu->sc == &RR_sched_class){
            load_value = proc->alloc_time_slice;
        }else{
            panic("proc->cpu->sc is not correct.\n");
        }
        // cprintf("444444  proc id is %d\n", proc->pid);
        set_proc_cpu(proc, &cpus[min_index]);
        max -= load_value;
        min += load_value;
        if(min > max) return;
    }        
}