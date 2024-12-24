#ifndef __NET_IPADDR_H__
#define __NET_IPADDR_H__

#include "error.h"
#include "net_config.h"
#include "types.h"
#include "net.h"

#define IPV4_ADDR_SIZE 4

typedef struct ipaddr Ipaddr;
struct ipaddr {
    enum{
        IPADDR_V4,
    }type;
    union{
        uint32_t q_addr;
        uint8_t a_addr[IPV4_ADDR_SIZE];
    };
};

void ipaddr_set_any(Ipaddr* ip);
int ipaddr_from_str(Ipaddr* dest, char* str);
Ipaddr* ipaddr_get_any(void);
void ipaddr_copy(Ipaddr* dest, Ipaddr* src);
#endif