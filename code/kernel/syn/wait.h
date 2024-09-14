#ifndef __SYN_WAIT_H__
#define __SYN_WAIT_H__

#include "list.h"

typedef struct wait_queue Wait_queue;
struct wait_queue{
    List_entry wait_head;
};

typedef struct proc Proc;

typedef struct wait Wait;
struct wait{
    Proc *proc;
    uint32_t wakeup_flags;
    Wait_queue *wait_queue;
    List_entry wait_link;
};

#define le2wait(le, member) to_struct((le), Wait, member)

void wait_init(Wait *wait, Proc *proc);
void wait_queue_init(Wait_queue *queue);
void wait_queue_add(Wait_queue *queue, Wait *wait);
void wait_queue_del(Wait_queue *queue, Wait *wait);
Wait *wait_queue_next(Wait_queue *queue, Wait *wait);
Wait *wait_queue_prev(Wait_queue *queue, Wait *wait);
Wait *wait_queue_first(Wait_queue *queue);
Wait *wait_queue_last(Wait_queue *queue);
bool wait_queue_empty(Wait_queue *queue);
bool wait_in_queue(Wait *wait);
void wakeup_wait(Wait_queue *queue, Wait *wait, uint32_t wakeup_flags, bool del);
void wakeup_first(Wait_queue *queue, uint32_t wakeup_flags, bool del);
void wakeup_queue(Wait_queue *queue, uint32_t wakeup_flags, bool del);
void wait_current_set(Wait_queue *queue, Wait *wait, uint32_t wait_state);
void wait_current_del(Wait_queue *queue, Wait *wait);
#endif