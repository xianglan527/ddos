#ifndef __SYN_SEM_H__
#define __SYN_SEM_H__
#include "stdarg.h"
#include "types.h"
#include "wait.h"
#include "spinlock.h"
#include "list.h"

typedef struct sem Sem;
struct sem{
    int value;
    bool valid;
    Wait_queue wait_queue;
    Spinlock sem_lock;
    Atomic count;
};

typedef struct sem_undo Sem_undo;
struct sem_undo{
    Sem *sem;
    List_entry semu_link;
};

#define le2semu(le, member) to_struct((le), Sem_undo, member)

typedef struct sem_queue Sem_queue;
struct sem_queue{
    Spinlock sem_queue_lock;
    Atomic count;
    List_entry semu_list;
};

void sem_init(Sem *sem, int value);
void up(Sem *sem);
void down(Sem *sem);
bool try_down(Sem *sem);

Sem_queue *sem_queue_create(void);
void sem_queue_destroy(Sem_queue *sem_queue);
Sem_undo *semu_create(Sem *sem, int value);
void semu_destory(Sem_undo *semu);
int dup_sem_queue(Sem_queue *to, Sem_queue *from);
void exit_sem_queue(Sem_queue *sem_queue);
long ipc_sem_init(int value);
int ipc_sem_post(sem_t sem_id);
int ipc_sem_wait(sem_t sem_id, ulong timeout);
int ipc_sem_free(sem_t sem_id);
int ipc_sem_get_value(sem_t sem_id, int *value_store);
#endif