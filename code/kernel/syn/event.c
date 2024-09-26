#include "event.h"

#include "assert.h"
#include "error.h"
#include "proc.h"
#include "stdio.h"

extern Spinlock timer_lock;
extern ulong ticks;

extern Proc *initproc;
extern Proc *daemonproc;

void event_init(Event *event) {
    wait_queue_init(&event->wait_queue);
    initlock(&event->event_lock, "event_lock");
}

static void sleeping_with_event(void *chan, Spinlock *lk, Event *event) {
    Proc *p = myproc();
    if (lk != &p->lock) {
        acquire(&p->lock);
        release(lk);
        release(&event->event_lock);
    }
    p->chan = chan;
    p->state = SLEEPING;
    sched();
    p->chan = nullptr;

    if (p->kernel_proc && mycpu()->intena == 0) { mycpu()->intena = 1; }
    if (lk != &p->lock) {
        release(&p->lock);
        acquire(&event->event_lock);
        acquire(lk);
    }
}

static uint32_t send_event(Proc *proc, Timer *timer) {
    Proc *current = myproc();
    assert(current != nullptr && proc != nullptr);
    Event *event = &proc->event;
    Wait __wait, *wait = &__wait;
    Wait_queue *wait_queue = &proc->event.wait_queue;
    acquire(&event->event_lock);

    if (proc->wait_state == WT_EVENT_RECV) {
        // cprintf(".....ipc_send11111 current id : %d proc id : %d event_num is  : %d\n", current->pid,
        //         proc->pid, current->event.event_num);
        wakeup_proc(proc);
    }

    wait_current_set(wait_queue, wait, WT_EVENT_SEND);
    acquire(&timer_lock);
    ipc_add_timer(timer);
    // cprintf(".....send11111 current id : %d proc id : %d event_num is  : %d\n", current->pid, proc->pid,
    //         current->event.event_num);

    sleeping_with_event(current, &timer_lock, event);
    // cprintf(".....send22222 current id : %d proc id : %d event_num is  : %d\n", current->pid, proc->pid,
    //         current->event.event_num);
    ipc_del_timer(timer);
    release(&timer_lock);
    wait_current_del(wait_queue, wait);
    release(&event->event_lock);
    if (wait->wakeup_flags != WT_EVENT_SEND) { return wait->wakeup_flags; }
    return 0;
}

int ipc_event_send(int pid, int event_num, ulong timeout) {
    Proc *proc, *current = myproc();
    if ((proc = find_proc(pid)) == nullptr || proc->state == ZOMBIE) { return -E_INVAL; }
    if (proc == current || proc == initproc || proc == daemonproc) { return -E_INVAL; }
    // acquire(&proc->event.event_lock);
    // if (proc->wait_state == WT_EVENT_RECV) {
    //     cprintf(".....ipc_send11111 current id : %d proc id : %d event_num is  : %d\n", current->pid, proc->pid,
    //             current->event.event_num);
    //     wakeup_proc(proc); 
    // }
    // release(&proc->event.event_lock);
    current->event.event_num = event_num;
    ulong saved_ticks;
    Timer __timer, *timer = ipc_timer_init(timeout, &saved_ticks, &__timer);
    uint32_t flags;
    if ((flags = send_event(proc, timer)) == 0) { return 0; }
    assert(flags == WT_INTERAUPTED);
    return ipc_check_timeout(timeout, saved_ticks);
}

static int recv_event(int *pid_store, int *event_num_store, Timer *timer) {
    Proc *current = myproc();
    assert(current != nullptr);
    Event *event = &current->event;
    Wait_queue *wait_queue = &current->event.wait_queue;
    acquire(&event->event_lock);
    if (wait_queue_empty(wait_queue)) {
        current->wait_state = WT_EVENT_RECV;
        acquire(&timer_lock);
        ipc_add_timer(timer);
        // cprintf(".....recv11111 current id %d\n", current->pid);
        sleeping_with_event(current, &timer_lock, event);
        // cprintf(".....recv22222 current id %d\n", current->pid);
        ipc_del_timer(timer);
        release(&timer_lock);
    }
    int ret = -1;
    Wait *wait;
    if ((wait = wait_queue_first(wait_queue)) != nullptr) {
        Proc *proc = wait->proc;
        *pid_store = proc->pid, *event_num_store = proc->event.event_num, ret = 0;
        wakeup_wait(wait_queue, wait, WT_EVENT_SEND, 1);
        // cprintf(".....recv33333 current id : %d proc id : %d event_num is  : %d\n", current->pid, proc->pid,
        //         proc->event.event_num);
    }
    release(&event->event_lock);
    return ret;
}

int ipc_event_recv(int *pid_store, int *event_num_store, ulong timeout) {
    Proc *current = myproc();
    if (event_num_store == nullptr) { return -E_INVAL; }
    Mm_struct *mm = current->mm;
    ulong saved_ticks;
    Timer __timer, *timer = ipc_timer_init(timeout, &saved_ticks, &__timer);
    int pid, event_num, ret;
    if ((ret = recv_event(&pid, &event_num, timer)) == 0) {
        lock_mm(mm);
        if (pid_store != nullptr) {
            copy_kernel2user(mm->pagetable, (uintptr_t)pid_store, (char *)&pid, sizeof(pid));
        }
        copy_kernel2user(mm->pagetable, (uintptr_t)event_num_store, (char *)&event_num, sizeof(event_num));
        unlock_mm(mm);
        return 0;
    }
    return ipc_check_timeout(timeout, saved_ticks);
}