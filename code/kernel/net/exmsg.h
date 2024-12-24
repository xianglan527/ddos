#ifndef __NET_EXMSG_H__
#define __NET_EXMSG_H__

#include "types.h"
#include "netif.h"

typedef struct msg_netif Msg_netif;
struct msg_netif {
    Netif* netif;  
};

typedef struct exmsg Exmsg;
struct exmsg{
    enum{
        NET_EXMSG_NETIF_IN,
    }type;
    union {
        Msg_netif netif;
    };
    int id;
};


int exmsg_init(void);
int exmsg_start(void);

int exmsg_netif_in(Netif *netif);
#endif