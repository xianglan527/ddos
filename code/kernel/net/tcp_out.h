#ifndef __NET_TCP_OUT_H__
#define __NET_TCP_OUT_H__
#include "tcp.h"

typedef enum tcp_oevent {
    TCP_OEVENT_SEND,  
    TCP_OEVENT_XMIT, 
    TCP_OEVENT_TMO,
} Tcp_oevent;

int tcp_send_reset(Tcp_seg *seg);
int tcp_send_syn(Tcp *tcp);
int tcp_ack_process(Tcp *tcp, Tcp_seg *seg);
int tcp_send_ack(Tcp *tcp, Tcp_seg *seg);
int tcp_send_fin(Tcp *tcp);
int tcp_transmit(Tcp *tcp);
int tcp_send_keepalive(Tcp *tcp);
int tcp_send_reset_for_tcp(Tcp *tcp);
char *tcp_ostate_name(Tcp *tcp);
void tcp_out_event(Tcp *tcp, Tcp_oevent event);
void tcp_set_ostate(Tcp *tcp, Tcp_ostate state);
#endif