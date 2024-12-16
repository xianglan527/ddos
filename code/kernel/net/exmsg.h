#ifndef __NET_EXMSG_H__
#define __NET_EXMSG_H__

#include "types.h"

typedef struct exmsg Exmsg;
struct exmsg{
    enum{
        NET_EXMSG_NETIF_IN,
    }type;
    int id;
};


int exmsg_init(void);
int exmsg_start(void);

int exmsg_netif_in(void);
#endif