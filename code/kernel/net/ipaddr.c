#include "ipaddr.h"

void ipaddr_set_any(Ipaddr* ip) { ip->q_addr = 0; }

int ipaddr_from_str(Ipaddr* dest, char* str) {
    if (!dest || !str) { return -E_NET_PARAM; }
    dest->q_addr = 0;
    char c;
    uint8_t* p = dest->a_addr;
    uint8_t sub_addr = 0;
    while ((c = *str++) != '\0') {
        if ((c >= '0') && (c <= '9')) {
            sub_addr = sub_addr * 10 + c - '0';
        } else if (c == '.') {
            *p++ = sub_addr;
            sub_addr = 0;
        } else {
            return -E_NET_PARAM;
        }
    }
    *p++ = sub_addr;
    return NET_OK;
}

Ipaddr* ipaddr_get_any(void) {
    static Ipaddr ipaddr_any = {.type = IPADDR_V4, .q_addr = 0};
    return &ipaddr_any;
}

void ipaddr_copy(Ipaddr* dest, Ipaddr* src) {
    if (!dest || !src) { return; }
    dest->q_addr = src->q_addr;
    dest->type = src->type;
}

int ipaddr_is_equal(Ipaddr* ipaddr1, Ipaddr* ipaddr2) { return ipaddr1->q_addr == ipaddr2->q_addr; }

void ipaddr_to_buf(Ipaddr* src, uint8_t* ip_buf) {
    *(uint32_t *)ip_buf = src->q_addr;
}

void ipaddr_from_buf(Ipaddr* dest, uint8_t* ip_buf) {
    dest->q_addr = *(uint32_t*)ip_buf;
}

int ipaddr_is_local_broadcast(Ipaddr* ipaddr) { return ipaddr->q_addr == IPV4_ADDR_BROADCAST; }

Ipaddr ipaddr_get_host(const Ipaddr *ipaddr, const Ipaddr *netmask) {
    Ipaddr hostid;
    hostid.q_addr = ipaddr->q_addr & ~netmask->q_addr;
    return hostid;
}

int ipaddr_is_direct_broadcast(Ipaddr *ipaddr, Ipaddr *netmask){
    Ipaddr hostid = ipaddr_get_host(ipaddr, netmask);
    return hostid.q_addr == (IPV4_ADDR_BROADCAST & ~netmask->q_addr);
}

int ipaddr_is_any(Ipaddr *ip) { return ip->q_addr == 0; }

Ipaddr ipaddr_get_net(Ipaddr* ipaddr, Ipaddr* netmask) {
    Ipaddr netid;
    netid.q_addr = ipaddr->q_addr & netmask->q_addr;
    return netid;
}

int ipaddr_is_match(Ipaddr* dest, Ipaddr* src, Ipaddr* netmask){
    Ipaddr dest_netid = ipaddr_get_net(dest, netmask);
    Ipaddr src_netid = ipaddr_get_net(src, netmask);
    if (ipaddr_is_local_broadcast(dest)) { return 1; }
    if (ipaddr_is_direct_broadcast(dest, netmask) && ipaddr_is_equal(&dest_netid, &src_netid)) { return 1; }
    return ipaddr_is_equal(dest, src);
}

void ipaddr_set_all_1(Ipaddr *ip) { ip->q_addr = 0xFFFFFFFF; }

int ipaddr_1_cnt(Ipaddr *ip) {
    int cnt = 0;
    uint32_t addr = ip->q_addr;
    while (addr) {
        if (addr & 0x1) { cnt++; }
        addr >>= 1;
    }
    return cnt;
}
