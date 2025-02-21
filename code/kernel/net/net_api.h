#ifndef __NET_NET_API_H__
#define __NET_NET_API_H__

#include "error.h"
#include "net_config.h"
#include "nettool.h"
#include "types.h"
#include "socket.h"

#define SOCK_DGRAM SOCK_UDP
#define SOCK_STREAM SOCK_TCP

#define htons(v) x_htons(v)
#define ntohs(v) x_ntohs(v)
#define htonl(v) x_htonl(v)
#define ntohl(v) x_ntohl(v)

char *inet_ntoa(In_addr in);
uint32_t inet_addr(char *str);
int inet_pton(int family, char *strptr, void *addrptr);
char *inet_ntop(int family, void *addrptr, char *strptr, size_t len);

#endif