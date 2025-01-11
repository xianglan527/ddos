#ifndef __NET_RAW_H__
#define __NET_RAW_H__

#include "error.h"
#include "net_config.h"
#include "nettool.h"
#include "types.h"
// #include "sock.h"
#include "list.h"
#include "spinlock.h"
#include "pktbuf.h"

typedef struct raw {
    List_entry recv_pkt_list;  // Since recv_pkt_list is only called by the network worker thread,
                               // there is no race condition, so no locking is required.
} Raw;

int raws_init(void);
int raw_in(Pktbuf* pktbuf);
#endif