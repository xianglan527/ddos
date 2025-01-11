#ifndef __NET_EXMSG_H__
#define __NET_EXMSG_H__

#include "types.h"
#include "netif.h"
#include "proc.h"
#include "sem.h"

typedef struct msg_netif Msg_netif;
struct msg_netif {
    Netif* netif;  
};

typedef struct func_msg Func_msg;
typedef int (*Exmsg_func)(Func_msg* msg);

typedef struct func_msg {
    Proc *proc;  
    Exmsg_func func;   
    void* param;         
    int err;
    Sem wait_sem;
} Func_msg;

typedef struct exmsg Exmsg;
struct exmsg{
    enum {
        NET_EXMSG_NETIF_IN,
        NET_EXMSG_FUN,
    } type;
    union {
        Msg_netif netif;
        Func_msg *func;
    };
    int id;
};


int exmsg_init(void);
int exmsg_start(void);

int exmsg_netif_in(Netif *netif);
int exmsg_func_exec(Exmsg_func func, void* param);
int test_func(Func_msg* msg);
#endif