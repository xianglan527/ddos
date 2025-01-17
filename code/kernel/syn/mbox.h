#ifndef __SYN_MBOX_H__
#define __SYN_MBOX_H__

#include "mboxbuf.h"
#include "list.h"
#include "types.h"
#include "wait.h"
#include "riscv.h"
#include "spinlock.h"

typedef struct msg_seg Msg_seg;
struct msg_seg{
    Msg_seg *next;
};

typedef struct msg_msg Msg_msg;
struct msg_msg{
    int pid;
    size_t bytes;
    Msg_seg *next;
    List_entry msg_link;
};

#define le2msg(le, member)  to_struct((le), Msg_msg, member)

typedef enum mbox_state Mbox_state;
enum mbox_state{
    CLOSED = 0,
    OPENED = 1,
    CLOSING = 2,
};

typedef struct msg_mbox Msg_mbox;
struct msg_mbox{
    int id;
    int inuse;
    Mbox_state state;
    size_t max_slots, slots;
    List_entry msg_link;
    Wait_queue senders;
    Wait_queue receivers;
    Spinlock msg_mbox_lock;
};

#define le2mbox(le, member) to_struct((le), Msg_mbox, member)

#define MAX_MBOX_NUM    8192
#define MBOX_P_PAGE      (PGSIZE / sizeof(Msg_mbox))
#define MAX_MBOX_PAGES ((MAX_MBOX_NUM + MBOX_P_PAGE - 1) / MBOX_P_PAGE)
#define MAX_MSG_DATELEN (512 - sizeof(Msg_msg))

void mbox_init(void);
Msg_mbox *get_mbox(int id);
int ipc_mbox_init(size_t max_slots);
int ipc_mbox_send(int id, Mboxbuf *buf, long timeout);
int ipc_mbox_recv(int id, Mboxbuf *buf, long timeout);
int ipc_mbox_free(int id);
int ipc_mbox_info(int id, Mboxinfo *info);
void mbox_cleanup(void);
#endif