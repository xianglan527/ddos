#ifndef __NET_PROTOCOL_H__
#define __NET_PROTOCOL_H__

#include "error.h"
#include "net_config.h"
#include "types.h"

typedef enum protocol {
    NET_PROTOCOL_ARP = 0x0806,   
    NET_PROTOCOL_IPv4 = 0x0800,  
} Protocol;

#endif