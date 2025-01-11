#include "net_api.h"
#include "printf.h"

#define IPV4_STR_SIZE 16  

char *inet_ntoa(In_addr in){
    static char buf[IPV4_STR_SIZE];
    memset(buf, 0, IPV4_STR_SIZE);
    snprintf(buf, IPV4_STR_SIZE, "%d.%d.%d.%d", in.addr0, in.addr1, in.addr2, in.addr3);
    return buf;
}

uint32_t inet_addr(char *str){
    if (!str) { return INADDR_ANY; }
    Ipaddr ipaddr;
    ipaddr_from_str(&ipaddr, str);
    return ipaddr.q_addr;
}

int inet_pton(int family, char *strptr, void *addrptr){
    if ((family != AF_INET) || !strptr || !addrptr) { return -E_NET_NOT_SUPPORT; }
    In_addr *addr = (In_addr *)addrptr;
    Ipaddr dest;
    ipaddr_from_str(&dest, strptr);
    addr->s_addr = dest.q_addr;
    return NET_OK;
}

char *inet_ntop(int family, void *addrptr, char *strptr, size_t len){
    if ((family != AF_INET) || !addrptr || !strptr || !len) { return nullptr; }
    In_addr *addr = (In_addr *)addrptr;
    char buf[IPV4_STR_SIZE];
    memset(buf, 0, IPV4_STR_SIZE);
    snprintf(buf, IPV4_STR_SIZE,"%d.%d.%d.%d", addr->addr0, addr->addr1, addr->addr2, addr->addr3);
    strncpy(strptr, buf, len - 1);
    strptr[len - 1] = '\0';
    return strptr;
}