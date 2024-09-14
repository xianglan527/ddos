#ifndef __SYN_EVENT_H__
#define __SYN_EVENT_H__
#include "spinlock.h"
#include "stdarg.h"
#include "types.h"
#include "wait.h"

typedef struct event Event;
struct event{
    int event_num;
    Wait_queue wait_queue;
    Spinlock event_lock;
};

void event_init(Event *event);
int ipc_event_send(int pid, int event_num, ulong timeout);
int ipc_event_recv(int *pid_store, int *event_num_store, ulong timeout);
#endif