#ifndef __NET_PROTOCOL_H__
#define __NET_PROTOCOL_H__

#include "error.h"
#include "net_config.h"
#include "types.h"

#define NET_PORT_DYN_START 1024
#define NET_PORT_DYN_END 65535

typedef enum protocol {
    NET_PROTOCOL_ARP = 0x0806,
    NET_PROTOCOL_IPv4 = 0x0800,
    NET_PROTOCOL_ICMPv4 = 0x1,  
    NET_PROTOCOL_UDP = 0x11,  
    NET_PROTOCOL_TCP = 0x06,    
} Protocol;

#endif