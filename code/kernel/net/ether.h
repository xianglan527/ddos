#ifndef __NET_ETHER_H__
#define __NET_ETHER_H__

#include "error.h"
#include "net.h"
#include "net_config.h"
#include "types.h"
#include "protocol.h"
#include "netif.h"
#include "pktbuf.h"

#define ETH_HWA_SIZE 6  
#define ETH_MTU 1500     
#define ETH_DATA_MIN 46

#pragma pack(1)
typedef struct ether_hdr {
    uint8_t dest[ETH_HWA_SIZE];  
    uint8_t src[ETH_HWA_SIZE];   
    uint16_t protocol;         
} Ether_hdr;

typedef struct ether_pkt {
    Ether_hdr hdr;         
    uint8_t data[ETH_MTU]; 
} Ether_pkt;
#pragma pack()

int ether_init(void);
uint8_t* ether_broadcast_addr(void);
int ether_raw_out(Netif* netif, Protocol protocol, const uint8_t* dest, Pktbuf* buf);
#endif