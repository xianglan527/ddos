#include "wait.h"
#include "proc.h"

void wait_init(Wait *wait, Proc *proc){
    wait->proc = proc;
    wait->wakeup_flags = WT_INTERAUPTED;
    list_init(&wait->wait_link);
}

void wait_queue_init(Wait_queue *queue){
    list_init(&queue->wait_head);
}

void wait_queue_add(Wait_queue *queue, Wait *wait){
    assert(list_empty(&wait->wait_link) && wait->proc != nullptr);
    wait->wait_queue = queue;
    list_add_before(&queue->wait_head, &wait->wait_link);
}

void wait_queue_del(Wait_queue *queue, Wait *wait){
    assert(!list_empty(&wait->wait_link) && wait->wait_queue == queue);
    list_del_init(&wait->wait_link);
}

Wait *wait_queue_next(Wait_queue *queue, Wait *wait){
    assert(!list_empty(&wait->wait_link) && wait->wait_queue == queue);
    List_entry *le = list_next(&wait->wait_link);
    if(le != &queue->wait_head){
        return le2wait(le, wait_link);
    }
    return nullptr;
}

Wait *wait_queue_prev(Wait_queue *queue, Wait *wait) {
    assert(!list_empty(&wait->wait_link) && wait->wait_queue == queue);
    List_entry *le = list_prev(&wait->wait_link);
    if (le != &queue->wait_head) { return le2wait(le, wait_link); }
    return nullptr;
}

Wait *wait_queue_first(Wait_queue *queue){
    List_entry *le = list_next(&queue->wait_head);
    if(le != &queue->wait_head){
        return le2wait(le, wait_link);
    }
    return nullptr;
}

Wait *wait_queue_last(Wait_queue *queue){
    List_entry *le = list_prev(&queue->wait_head);
    if(le != &queue->wait_head){
        return le2wait(le, wait_link);
    }
    return nullptr;
}

bool wait_queue_empty(Wait_queue *queue){
    return list_empty(&queue->wait_head);
}

bool wait_in_queue(Wait *wait){
    return !list_empty(&wait->wait_link);
}

void wakeup_wait(Wait_queue *queue, Wait *wait, uint32_t wakeup_flags, bool del){
    if(del){
        wait_queue_del(queue, wait);
    }
    wait->wakeup_flags = wakeup_flags;
    wakeup_proc(wait->proc);
}

void wakeup_first(Wait_queue *queue, uint32_t wakeup_flags, bool del){
    Wait *wait;
    if((wait = wait_queue_first(queue)) != nullptr){
        wakeup_wait(queue, wait, wakeup_flags, del);
    }
}

void wakeup_queue(Wait_queue *queue, uint32_t wakeup_flags, bool del){
    Wait *wait;
    if((wait = wait_queue_first(queue)) != nullptr){
        if(del){
            do{
                wakeup_wait(queue, wait, wakeup_flags ,1);
            }while((wait = wait_queue_first(queue)) != nullptr);
        }else{
            do {
                wakeup_wait(queue, wait, wakeup_flags, 0);
            } while ((wait = wait_queue_next(queue, wait)) != nullptr);
        }
    }
}

void wait_current_set(Wait_queue *queue, Wait *wait, uint32_t wait_state){
    Proc *current = myproc();
    assert(current != nullptr);
    wait_init(wait, current);
    // current->state = SLEEPING;
    current->wait_state = wait_state;
    wait_queue_add(queue, wait);
}

void wait_current_del(Wait_queue *queue, Wait *wait) {
    if(wait_in_queue(wait)){
        wait_queue_del(queue, wait);
    }
}