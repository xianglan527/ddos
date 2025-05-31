#ifndef __NET_TCP_OUT_H__
#define __NET_TCP_OUT_H__
#include "tcp.h"

typedef enum tcp_oevent {
    TCP_OEVENT_SEND,
    TCP_OEVENT_XMIT,
} Tcp_oevent;

static inline bool tcp_do_output_nagle(Tcp *tcp, ssize_t dlen) {
    return (tcp->snd.una == tcp->snd.nxt) || (tcp->flags.nagle_dis_enble) || (dlen >= (tcp->mss)) ||
           (tcp->flags.syn_out) || (tcp->flags.fin_out);
}

static inline bool tcp_do_input_nagle(Tcp *tcp) {
    return (tcp->flags.nagle_dis_enble) || (tcp_rcv_window(tcp) >= tcp->mss) ||
           (tcp->flags.syn_out) || (tcp->flags.fin_out);
}

int tcp_send_reset(Tcp_seg *seg);
int tcp_send_syn(Tcp *tcp);
int tcp_ack_process(Tcp *tcp, Tcp_seg *seg);
int tcp_send_ack(Tcp *tcp, Tcp_seg *seg);
int tcp_send_fin(Tcp *tcp);
int tcp_transmit(Tcp *tcp);
int tcp_retransmit(Tcp *tcp, bool no_newdata);
int tcp_send_keepalive(Tcp *tcp);
int tcp_send_reset_for_tcp(Tcp *tcp);
char *tcp_ostate_name(Tcp *tcp);
char *tcp_oevent_name(Tcp_oevent event);
void tcp_out_event(Tcp *tcp, Tcp_oevent event);
void tcp_set_ostate(Tcp *tcp, Tcp_ostate state);
size_t tcp_write_sndbuf(Tcp *tcp, uint8_t *buf, size_t len);
int tcp_send_win_update(Tcp *tcp);
#endif