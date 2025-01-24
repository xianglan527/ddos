#ifndef __NET_UDP_H__
#define __NET_UDP_H__

#include "error.h"
#include "net_config.h"
#include "nettool.h"
#include "types.h"
// #include "sock.h"
#include "list.h"
#include "pktbuf.h"
#include "spinlock.h"

#pragma pack(1)

typedef struct udp_hdr {
    uint16_t src_port;  
    uint16_t dest_port;  
    uint16_t total_len;
    uint16_t checksum;   
} Udp_hdr;

typedef struct udp_pkt {
    Udp_hdr hdr;    
    uint8_t data[1];  
} Udp_pkt;

#pragma pack()

typedef struct udp_from {
    Ipaddr from;
    uint16_t port;
} Udp_from;

typedef struct udp {
    List_entry recv_pkt_list;  // Since recv_pkt_list is only called by the network worker thread,
                               // there is no race condition, so no locking is required.
} Udp;

int udps_init(void);
int udp_out(Ipaddr* dest, uint16_t dport, Ipaddr* src, uint16_t sport, Pktbuf* buf);
int udp_in(Pktbuf* buf, Ipaddr* src_ip, Ipaddr* dest_ip);
#endif