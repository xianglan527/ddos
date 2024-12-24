#ifndef __NET_NET_DEVICE_H__
#define __NET_NET_DEVICE_H__

#include "netif.h"
#include "types.h"

// typedef struct net_device_data {
//     const char* ip;         // 使用的网卡
//     const uint8_t* hwaddr;  // 网卡的mac地址
// } Net_device_data;

int net_tx_rx_create(Netif *netif);

#endif