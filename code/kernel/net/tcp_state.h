#ifndef __NET_TCP_STATE_H__
#define __NET_TCP_STATE_H__

#include "error.h"
#include "types.h"
#include "tcp.h"

typedef int (*Tcp_proc)(Tcp *tcp, Tcp_seg *seg);

int tcp_closed_in(Tcp *tcp, Tcp_seg *seg);
int tcp_syn_sent_in(Tcp *tcp, Tcp_seg *seg);
int tcp_established_in(Tcp *tcp, Tcp_seg *seg);
int tcp_close_wait_in(Tcp *tcp, Tcp_seg *seg);
int tcp_last_ack_in(Tcp *tcp, Tcp_seg *seg);
int tcp_fin_wait_1_in(Tcp *tcp, Tcp_seg *seg);
int tcp_fin_wait_2_in(Tcp *tcp, Tcp_seg *seg);
int tcp_closing_in(Tcp *tcp, Tcp_seg *seg);
int tcp_time_wait_in(Tcp *tcp, Tcp_seg *seg);
int tcp_listen_in(Tcp *tcp, Tcp_seg *seg);
int tcp_syn_recvd_in(Tcp *tcp, Tcp_seg *seg);

char* tcp_state_name(Tcp_state state);
void tcp_set_state(Tcp* tcp, Tcp_state state);

#endif