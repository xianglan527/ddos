#include "sched.h"
#include "sched_RR.h"
#include "proc.h"
#include "spinlock.h"
#include "config.h"

extern Cpu cpus[NCPU];

void sched_init(void) {
    Cpu *cpu = mycpu();
    // cpu->sc = &FCFS_sched_class;
    cpu->sc = &RR_sched_class;
    initlock(&cpu->cpu_lock, "cpu_lock");
    cpu->rq.max_time_slice = MAX_TIME_SLICE;
    cpu->sc->init(cpu);
}

void sched_class_enqueue(Proc *proc){
    Cpu *cpu;
    if(proc->set_cpu != nullptr){
        cpu = proc->set_cpu;
        goto insert_proc;
    }
    long min_proc_num = atomic_read(&cpus[0].rq.proc_num);
    int min_proc_num_index = 0;
    for (int i = 1; i < NCPU; i++) {
        if(cpus[i].sc != nullptr && atomic_read(&cpus[i].rq.proc_num) < min_proc_num){
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

void sched_class_dequeue(Proc *proc){
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

void sched_class_proc_tick(Proc *proc){
    mycpu()->sc->proc_tick(proc);
}
