#ifndef __NET_ICMPV4_H__
#define __NET_IPCMV4_H__

#include "error.h"
#include "ipaddr.h"
#include "net.h"
#include "net_config.h"
#include "netif.h"
#include "pktbuf.h"
#include "types.h"
#include "ipv4.h"

typedef enum icmp_type {
    ICMPv4_ECHO_REPLY = 0,
    ICMPv4_ECHO_REQUEST = 8,
    ICMPv4_UNREACH = 3,
} Icmp_type;

typedef enum icmp_code {
    ICMPv4_ECHO = 0,
    ICMPv4_UNREACH_PRO = 2,  
    ICMPv4_UNREACH_PORT = 3, 
} Icmp_code;

#pragma pack(1)

typedef struct icmpv4_hdr {
    uint8_t type;      
    uint8_t code;      
    uint16_t checksum;  
} Icmpv4_hdr;

typedef struct icmpv4_pkt {
    Icmpv4_hdr hdr;  
    union {
        uint32_t reverse;  
    };
    uint8_t data[1];  
} Icmpv4_pkt;

#pragma pack()

int icmpv4_init(void);
int icmpv4_in(Ipaddr *src_ip, Ipaddr *netif_ip, Pktbuf *buf);
int icmpv4_out_unreach(Ipaddr* dest_addr, Ipaddr* src, uint8_t code, Pktbuf* ip_buf);
#endif