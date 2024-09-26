#include "sem.h"

#include "assert.h"
#include "proc.h"
#include "slab.h"
#include "config.h"
#include "error.h"

#define VALID_SEMID(sem_id) ((uintptr_t)(sem_id) < (uintptr_t)(sem_id) + KERNBASE)

#define semid2sem(sem_id) ((Sem *)((uintptr_t)(sem_id) + KERNBASE))

#define sem2semid(sem) ((sem_t)((uintptr_t)(sem) - KERNBASE))

extern Spinlock timer_lock;
extern ulong ticks;

void sem_init(Sem *sem, int value) {
    sem->value = value;
    sem->valid = true;
    wait_queue_init(&sem->wait_queue);
    initlock(&sem->sem_lock, "sem_lock");
    atomic_set(&sem->count, 0);
}

static void __up(Sem *sem, uint32_t wait_state) {
    assert(sem->valid);
    Wait *wait;
    acquire(&sem->sem_lock);
    if ((wait = wait_queue_first(&sem->wait_queue)) == nullptr) {
        sem->value++;
    } else {
        assert(wait->proc->wait_state == wait_state);
        wakeup_wait(&sem->wait_queue, wait, wait_state, 1);
    }
    release(&sem->sem_lock);
}

static void sleeping_with_sem(void *chan, Spinlock *lk, Sem *sem){
    Proc *p = myproc();
    if (lk != &p->lock) {
        acquire(&p->lock);
        release(lk);
        release(&sem->sem_lock);

    }
    p->chan = chan;
    p->state = SLEEPING;
    sched();
    p->chan = nullptr;

    if (p->kernel_proc && mycpu()->intena == 0) { mycpu()->intena = 1; }
    if (lk != &p->lock) {
        release(&p->lock);
        acquire(&sem->sem_lock);
        acquire(lk);
    }
}

static uint32_t __down(Sem *sem, uint32_t wait_state, Timer *timer) {
    assert(sem->valid);
    Proc *current = myproc();
    acquire(&sem->sem_lock);
    if (sem->value > 0) {
        sem->value--;
        release(&sem->sem_lock);
        return 0;
    }
    Wait __wait, *wait = &__wait;
    wait_current_set(&sem->wait_queue, wait, wait_state);
    acquire(&timer_lock);
    ipc_add_timer(timer);
    assert(current != nullptr);
    sleeping_with_sem(current, &timer_lock, sem);
    ipc_del_timer(timer);
    release(&timer_lock);
    wait_current_del(&sem->wait_queue, wait);
    release(&sem->sem_lock);
    if (wait->wakeup_flags != wait_state) { return wait->wakeup_flags; }
    return 0;
}

void up(Sem *sem) { __up(sem, WT_KSEM); }

void down(Sem *sem) {
    uint32_t flags = __down(sem, WT_KSEM, nullptr);
    assert(flags == 0);
}

bool try_down(Sem *sem) {
    bool ret = false;
    acquire(&sem->sem_lock);
    if (sem->value > 0) {
        sem->value--;
        ret = true;
    }
    release(&sem->sem_lock);
    return ret;
}

static int usem_up(Sem *sem){
    __up(sem, WT_USEM);
    return 0;
}

static int usem_down(Sem *sem, ulong timeout){
    ulong saved_ticks;
    Timer __timer, *timer = ipc_timer_init(timeout, &saved_ticks, &__timer);
    uint32_t flags;
    if((flags = __down(sem, WT_USEM, timer)) == 0)
        return 0;
    assert(flags == WT_INTERAUPTED);
    return ipc_check_timeout(timeout, saved_ticks);
}

Sem_queue *sem_queue_create(void){
    Sem_queue *sem_queue;
    if((sem_queue = kmalloc(sizeof(Sem_queue))) != nullptr){
        initlock(&sem_queue->sem_queue_lock, "sem_queue_lock");
        atomic_set(&sem_queue->count, 0);
        list_init(&sem_queue->semu_list);
    }
    return sem_queue;
}

void sem_queue_destroy(Sem_queue *sem_queue){
    kfree(sem_queue);
}

Sem_undo *semu_create(Sem *sem, int value){
    Sem_undo *semu;
    if((semu = kmalloc(sizeof(Sem_undo))) != nullptr){
        if(sem == nullptr && (sem = kmalloc(sizeof(Sem))) != nullptr){
            sem_init(sem, value);
        }
        if(sem != nullptr){
            atomic_inc(&sem->count);
            semu->sem = sem;
            return semu;
        }
        kfree(semu);
    }
    return nullptr;
}

void semu_destory(Sem_undo *semu){
    if(atomic_sub_return(&semu->sem->count, 1) == 0){
        kfree(semu->sem);
    }
    kfree(semu);
}

int dup_sem_queue(Sem_queue *to, Sem_queue *from){
    assert(to != nullptr && from != nullptr);
    List_entry *list = &from->semu_list, *le = list;
    while((le = list_next(le)) != list){
        Sem_undo *semu = le2semu(le, semu_link);
        if(semu->sem->valid){
            if ((semu = semu_create(semu->sem, 0)) == nullptr) { 
                return -E_NO_MEM; 
            }
            list_add(&to->semu_list, &semu->semu_link);
        }
    }
    return 0;
}

void exit_sem_queue(Sem_queue *sem_queue){
    assert(sem_queue != nullptr && atomic_read(&sem_queue->count) == 0);
    List_entry *list = &sem_queue->semu_list, *le = list;
    while((le = list_next(le)) != list){
        list_del(le);
        semu_destory(le2semu(le, semu_link));
    }
}

static Sem_undo *semu_list_search(List_entry *list, sem_t sem_id){
    if(VALID_SEMID(sem_id)){
        Sem *sem = semid2sem(sem_id);
        List_entry *le = list;
        while((le = list_next(le)) != list){
            Sem_undo *semu = le2semu(le, semu_link);
            if(semu->sem == sem){
                list_del(le);
                if(sem->valid){
                    list_add_after(list, le);
                    return semu;
                }
                else{
                    semu_destory(semu);
                    return nullptr;
                }
            }
        }
    }
    return nullptr;
}

long ipc_sem_init(int value){
    Proc *current = myproc();
    assert(current != nullptr && current->sem_queue != nullptr);
    Sem_undo *semu;
    if((semu = semu_create(nullptr, value)) == nullptr){
        return -E_NO_MEM;
    }
    Sem_queue *sem_queue = current->sem_queue;
    acquire(&sem_queue->sem_queue_lock);
    list_add_after(&sem_queue->semu_list, &semu->semu_link);
    release(&sem_queue->sem_queue_lock);
    return (long)sem2semid(semu->sem);
}

int ipc_sem_post(sem_t sem_id){
    Proc *current = myproc();
    assert(current != nullptr && current->sem_queue != nullptr);
    Sem_undo *semu;
    Sem_queue *sem_queue = current->sem_queue;
    acquire(&sem_queue->sem_queue_lock);
    semu = semu_list_search(&sem_queue->semu_list, sem_id);
    release(&sem_queue->sem_queue_lock);
    if(semu != nullptr){
        return usem_up(semu->sem);
    }
    return -E_INVAL;
}

int ipc_sem_wait(sem_t sem_id, ulong timeout) {
    Proc *current = myproc();
    assert(current != nullptr && current->sem_queue != nullptr);
    Sem_undo *semu;
    Sem_queue *sem_queue = current->sem_queue;
    acquire(&sem_queue->sem_queue_lock);
    semu = semu_list_search(&sem_queue->semu_list, sem_id);
    release(&sem_queue->sem_queue_lock);
    if (semu != nullptr) {
        return usem_down(semu->sem, timeout);
    }
    return -E_INVAL;
}

int ipc_sem_free(sem_t sem_id){
    Proc *current = myproc();
    assert(current->sem_queue != nullptr);
    Sem_undo *semu;
    Sem_queue *sem_queue = current->sem_queue;
    acquire(&sem_queue->sem_queue_lock);
    semu = semu_list_search(&sem_queue->semu_list, sem_id);
    release(&sem_queue->sem_queue_lock);
    int ret = -E_INVAL;
    if(semu != nullptr){
         acquire(&semu->sem->sem_lock);
         semu->sem->valid = 0, ret = 0;
         wakeup_queue(&semu->sem->wait_queue, WT_INTERAUPTED, 1);
         release(&semu->sem->sem_lock);
    }
    return ret;
}

int ipc_sem_get_value(sem_t sem_id, int *value_store){
    Proc *current = myproc();
    assert(current != nullptr && current->sem_queue != nullptr);
    int ret = -E_INVAL;
    if(value_store == nullptr){
        return ret;
    }
    Mm_struct *mm = current->mm;
    Sem_undo *semu;
    Sem_queue *sem_queue = current->sem_queue;
    acquire(&sem_queue->sem_queue_lock);
    semu = semu_list_search(&sem_queue->semu_list, sem_id);
    release(&sem_queue->sem_queue_lock);
    if(semu != nullptr){
        int value = semu->sem->value;
        lock_mm(mm);
        copy_kernel2user(mm->pagetable, (uintptr_t)value_store, (char *)&value, sizeof(value));
        ret = 0;
        unlock_mm(mm);
    }
    return ret;
}