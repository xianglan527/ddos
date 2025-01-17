#ifndef __NET_NET_CONFIG_H__
#define __NET_NET_CONFIG_H__

#include "debug.h"
#include "string.h"
#include "types.h"

#define NET_NEED_WAIT 1

#define DBG_MSG DBG_LEVEL_ERROR
#define DBG_BUF DBG_LEVEL_ERROR
#define DBG_INIT DBG_LEVEL_ERROR
#define DBG_NETIF DBG_LEVEL_ERROR
#define DBG_ETHER DBG_LEVEL_ERROR
#define DBG_TOOLS DBG_LEVEL_ERROR
#define DBG_ARP DBG_LEVEL_ERROR
#define DBG_IP DBG_LEVEL_ERROR
#define DBG_ICMP DBG_LEVEL_ERROR
#define DBG_SOCKET DBG_LEVEL_ERROR
#define DBG_RAW DBG_LEVEL_ERROR

#define NET_ENDIAN_LITTLE 1

#define EXMSG_MSG_CNT 4000
#define RX_TX_RX_MSG_CNT 2000

#define PKTBUF_BLK_SIZE 512
#define PKTBUF_BLK_CNT 20000

#define NETIF_HWADDR_SIZE 10
#define NETIF_NAME_SIZE 64

#define ARP_CACHE_SIZE 100
#define ARP_MAX_PKT_WAIT 20
#define ARP_ENTRY_STABLE_TMO 200  // unit is 1000ticks
#define ARP_ENTRY_PENDING_TMO 3  // unit is 1000ticks
#define ARP_ENTRY_RETRY_CNT 5 
#define ARP_TIMER_TMO 1       //unit is 1000ticks
#define IP_FRAGS_MAX_NR 20
#define IP_FRAG_MAX_BUF_NR 20
#define IP_FRAG_SCAN_PERIOD 1  // unit is 1000ticks
#define IP_FRAG_TMO 5          // unit is 1000ticks

#define MAX_SOCKET_NUM 4096

#define RAW_MAX_RECV 2000

#define IP_RTABLE_SIZE 32

#endif