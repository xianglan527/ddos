#ifndef __NET_TCP_IN_H__
#define __NET_TCP_IN_H__
#include "tcp.h"

int tcp_in(Pktbuf *buf, Ipaddr *src_ip, Ipaddr *dest_ip);
void tcp_seg_init(Tcp_seg *seg, Pktbuf *buf, Ipaddr *local, Ipaddr *remote);
int tcp_data_in(Tcp *tcp, Tcp_seg *seg);
#endif