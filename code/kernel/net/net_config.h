#ifndef __NET_NET_CONFIG_H__
#define __NET_NET_CONFIG_H__

#include "debug.h"
#include "string.h"
#include "types.h"

#define DBG_MSG DBG_LEVEL_INFO
#define DBG_BUF DBG_LEVEL_INFO
#define DBG_INIT DBG_LEVEL_INFO
#define DBG_NETIF DBG_LEVEL_INFO
#define DBG_ETHER DBG_LEVEL_INFO
#define DBG_TOOLS DBG_LEVEL_INFO

#define NET_ENDIAN_LITTLE 1

#define EXMSG_MSG_CNT 1000
#define RX_TX_RX_MSG_CNT 500

#define PKTBUF_BLK_SIZE 512
#define PKTBUF_BLK_CNT 20000

#define NETIF_HWADDR_SIZE 10
#define NETIF_NAME_SIZE 64

#endif