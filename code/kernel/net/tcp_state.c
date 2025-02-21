#include "tcp_state.h"

#include "sock.h"
#include "tcp_out.h"
#include "tcp_in.h"

char *tcp_state_name(Tcp_state state) {
    static char *state_name[] = {
        [TCP_STATE_FREE] = "FREE",
        [TCP_STATE_CLOSED] = "CLOSED",
        [TCP_STATE_LISTEN] = "LISTEN",
        [TCP_STATE_SYN_SENT] = "SYN_SENT",
        [TCP_STATE_SYN_RECVD] = "SYN_RCVD",
        [TCP_STATE_ESTABLISHED] = "ESTABLISHED",
        [TCP_STATE_FIN_WAIT_1] = "FIN_WAIT_1",
        [TCP_STATE_FIN_WAIT_2] = "FIN_WAIT_2",
        [TCP_STATE_CLOSING] = "CLOSING",
        [TCP_STATE_TIME_WAIT] = "TIME_WAIT",
        [TCP_STATE_CLOSE_WAIT] = "CLOSE_WAIT",
        [TCP_STATE_LAST_ACK] = "LAST_ACK",

        [TCP_STATE_MAX] = "UNKNOWN",
    };

    if (state >= TCP_STATE_MAX) { state = TCP_STATE_MAX; }
    return state_name[state];
}

void tcp_set_state(Tcp *tcp, Tcp_state state) {
    tcp->state = state;
    Socket *socket = info2sk(tcp, tcp);
    tcp_show_info("tcp set state", socket);
}

int tcp_closed_in(Tcp *tcp, Tcp_seg *seg) {
    if (seg->hdr->f_rst == 0) {
        dbg_warning(DBG_TCP, "%s: recieve a rst", tcp ? tcp_state_name(tcp->state) : "unknown");
        tcp_send_reset(seg);
    }
    return NET_OK;
}

int tcp_syn_sent_in(Tcp *tcp, Tcp_seg *seg) {
    Tcp_hdr *tcp_hdr = seg->hdr;
    if (tcp_hdr->f_ack) {
        if ((tcp_hdr->ack - tcp->snd.iss <= 0) || (tcp_hdr->ack - tcp->snd.nxt > 0)) {
            dbg_warning(DBG_TCP, "%s: ack incorrect", tcp_state_name(tcp->state));
            return tcp_send_reset(seg);
        }
    }
    if (tcp_hdr->f_rst) {
        if (!tcp_hdr->f_ack) { return NET_OK; }
        dbg_warning(DBG_TCP, "%s: recieve a rst", tcp_state_name(tcp->state));
        return tcp_abort(tcp, -E_NET_RESET);
    }
    if (tcp_hdr->f_syn) {
        tcp->rcv.iss = tcp_hdr->seq;
        tcp->rcv.nxt = tcp_hdr->seq + 1;
        tcp->flags.irs_valid = 1;
        tcp_read_options(tcp, tcp_hdr);
        if (tcp_hdr->f_ack) {
            tcp_ack_process(tcp, seg);
            tcp_set_ostate(tcp, TCP_OSTATE_IDLE);
            tcp_send_ack(tcp, seg);
            tcp_set_state(tcp, TCP_STATE_ESTABLISHED);
            sock_wakeup(info2sk(tcp, tcp), WT_SOCK_CONN, NET_OK);
        } else {
            tcp_set_state(tcp, TCP_STATE_SYN_RECVD);
            tcp_send_syn(tcp);
        }
    }
    return NET_OK;
}

int tcp_established_in(Tcp *tcp, Tcp_seg *seg) {
    Tcp_hdr *tcp_hdr = seg->hdr;
    if (tcp_hdr->f_rst) {
        dbg_warning(DBG_TCP, "%s: recieve a rst", tcp_state_name(tcp->state));
        return tcp_abort(tcp, -E_NET_RESET);
    }
    if (tcp_hdr->f_syn) {
        dbg_warning(DBG_TCP, "%s: recieve a syn", tcp_state_name(tcp->state));
        tcp_send_reset(seg);
        return tcp_abort(tcp, -E_NET_RESET);
    }
    if (tcp_ack_process(tcp, seg) < 0) {
        dbg_warning(DBG_TCP, "%s:  ack process failed", tcp_state_name(tcp->state));
        return -E_NET;
    }
    tcp_data_in(tcp, seg);
    tcp_out_event(tcp, TCP_OEVENT_SEND);
    // tcp_transmit(tcp);
    if (tcp->flags.fin_in) { tcp_set_state(tcp, TCP_STATE_CLOSE_WAIT); }
    return NET_OK; 
}

int tcp_close_wait_in(Tcp *tcp, Tcp_seg *seg) {
    Tcp_hdr *tcp_hdr = seg->hdr;
    if (tcp_hdr->f_rst) {
        dbg_warning(DBG_TCP, "%s: recieve a rst", tcp_state_name(tcp->state));
        return tcp_abort(tcp, -E_NET_RESET);
    }
    if (tcp_hdr->f_syn) {
        dbg_warning(DBG_TCP, "%s: recieve a syn", tcp_state_name(tcp->state));
        tcp_send_reset(seg);
        return tcp_abort(tcp, -E_NET_RESET);
    }

    if (tcp_ack_process(tcp, seg) < 0) {
        dbg_warning(DBG_TCP, "%s:  ack process failed", tcp_state_name(tcp->state));
        return -E_NET_UNREACH;
    }
    tcp_out_event(tcp, TCP_OEVENT_SEND);
    // tcp_transmit(tcp);
    return NET_OK;
}

static int tcp_up_close_wait(Tcp *tcp){
    Socket *socket = info2sk(tcp, tcp);
    sock_wait_leave(&socket->close_wait, NET_OK);
    return NET_OK;
}

int tcp_last_ack_in(Tcp *tcp, Tcp_seg *seg) {
    Tcp_hdr *tcp_hdr = seg->hdr;
    if (tcp_hdr->f_rst) {
        dbg_warning(DBG_TCP, "%s: recieve a rst", tcp_state_name(tcp->state));
        return tcp_abort(tcp, -E_NET_RESET);
    }
    if (tcp_hdr->f_syn) {
        dbg_warning(DBG_TCP, "%s: recieve a syn", tcp_state_name(tcp->state));
        tcp_send_reset(seg);
        return tcp_abort(tcp, -E_NET_RESET);
    }

    if (tcp_ack_process(tcp, seg) < 0) {
        dbg_warning(DBG_TCP, "%s:  ack process failed", tcp_state_name(tcp->state));
        return -E_NET_UNREACH;
    }
    tcp_out_event(tcp, TCP_OEVENT_SEND);
    // tcp_transmit(tcp);
    if(tcp->flags.fin_out ==0){
        tcp_free(tcp);
        tcp_abort(tcp, -E_NET_CLOSE);
        return tcp_up_close_wait(tcp);
    }
    return NET_OK;
}

static void tcp_timewait_tmo(Timer *timer) {
    Tcp *tcp = (Tcp *)timer->arg;
    dbg_info(DBG_TCP, "tcp free: 2MSL");
    tcp_show_info("tcp free(2MSL)", info2sk(tcp, tcp));
    tcp_free(tcp);
    add_clean_tcp_list(tcp);
}

void tcp_time_wait(Tcp *tcp) {
    tcp_set_state(tcp, TCP_STATE_TIME_WAIT);
    tcp_kill_all_timers(tcp);
    tcp->conn.keep_retry = 0;
    tcp->conn.keep_timer = timer_func_add(tcp_timewait_tmo, tcp, 2 * TCP_TMO_MSL, 0);
    dbg_info(DBG_TCP, "tcp keepalive enabled.");
    sock_wakeup( info2sk(tcp, tcp), WT_SOCK_ALL, NET_OK);
    sock_wait_leave(&info2sk(tcp, tcp)->close_wait, NET_OK);
}

int tcp_fin_wait_1_in(Tcp *tcp, Tcp_seg *seg) {
    Tcp_hdr *tcp_hdr = seg->hdr;
    if (tcp_hdr->f_rst) {
        dbg_warning(DBG_TCP, "%s: recieve a rst", tcp_state_name(tcp->state));
        return tcp_abort(tcp, -E_NET_RESET);
    }
    if (tcp_hdr->f_syn) {
        dbg_warning(DBG_TCP, "%s: recieve a syn", tcp_state_name(tcp->state));
        tcp_send_reset(seg);
        return tcp_abort(tcp, -E_NET_RESET);
    }
    if (tcp_ack_process(tcp, seg) < 0) {
        dbg_warning(DBG_TCP, "%s:  ack process failed", tcp_state_name(tcp->state));
        return -E_NET;
    }
    tcp_data_in(tcp, seg);
    tcp_out_event(tcp, TCP_OEVENT_SEND);
    // tcp_transmit(tcp);
    if (tcp->flags.fin_out == 0){
        if (tcp->flags.fin_in) {
            tcp_time_wait(tcp);
        } else {
            tcp_set_state(tcp, TCP_STATE_FIN_WAIT_2);
        }
    } else if (tcp->flags.fin_in) {
        tcp_set_state(tcp, TCP_STATE_CLOSING);
    }
    return NET_OK;
}

int tcp_fin_wait_2_in(Tcp *tcp, Tcp_seg *seg) {
    Tcp_hdr *tcp_hdr = seg->hdr;
    if (tcp_hdr->f_rst) {
        dbg_warning(DBG_TCP, "%s: recieve a rst", tcp_state_name(tcp->state));
        return tcp_abort(tcp, -E_NET_RESET);
    }
    if (tcp_hdr->f_syn) {
        dbg_warning(DBG_TCP, "%s: recieve a syn", tcp_state_name(tcp->state));
        tcp_send_reset(seg);
        return tcp_abort(tcp, -E_NET_RESET);
    }
    if (tcp_ack_process(tcp, seg) < 0) {
        dbg_warning(DBG_TCP, "%s:  ack process failed", tcp_state_name(tcp->state));
        return -E_NET;
    }
    tcp_data_in(tcp, seg);
    if (tcp->flags.fin_in) { tcp_time_wait(tcp); }
     return NET_OK;
}

int tcp_closing_in(Tcp *tcp, Tcp_seg *seg) {
    Tcp_hdr *tcp_hdr = seg->hdr;
    if (tcp_hdr->f_rst) {
        dbg_warning(DBG_TCP, "%s: recieve a rst", tcp_state_name(tcp->state));
        return tcp_abort(tcp, -E_NET_RESET);
    }
    if (tcp_hdr->f_syn) {
        dbg_warning(DBG_TCP, "%s: recieve a syn", tcp_state_name(tcp->state));
        tcp_send_reset(seg);
        return tcp_abort(tcp, -E_NET_RESET);
    }
    if (tcp_ack_process(tcp, seg) < 0) {
        dbg_warning(DBG_TCP, "%s:  ack process failed", tcp_state_name(tcp->state));
        return -E_NET;
    }
    tcp_out_event(tcp, TCP_OEVENT_SEND);
    // tcp_transmit(tcp);
    if (tcp->flags.fin_out == 0 && tcp->flags.fin_in) { tcp_time_wait(tcp); }
    return NET_OK;
}

int tcp_time_wait_in(Tcp *tcp, Tcp_seg *seg) {
    Tcp_hdr *tcp_hdr = seg->hdr;
    if (tcp_hdr->f_rst) {
        dbg_warning(DBG_TCP, "%s: recieve a rst", tcp_state_name(tcp->state));
        return tcp_abort(tcp, -E_NET_RESET);
    }
    if (tcp_hdr->f_syn) {
        dbg_warning(DBG_TCP, "%s: recieve a syn", tcp_state_name(tcp->state));
        tcp_send_reset(seg);
        return tcp_abort(tcp, -E_NET_RESET);
    }
    if (tcp_ack_process(tcp, seg) < 0) {
        dbg_warning(DBG_TCP, "%s:  ack process failed", tcp_state_name(tcp->state));
        return -E_NET;
    }
    // tcp_data_in(tcp, seg);
    if (tcp->flags.fin_in) {
        tcp_send_ack(tcp, seg);
        tcp_time_wait(tcp);
    }
    return NET_OK;
}

int tcp_listen_in(Tcp *tcp, Tcp_seg *seg) {
    Tcp_hdr *tcp_hdr = seg->hdr;
    if (tcp_hdr->f_rst) {
        dbg_warning(DBG_TCP, "%s: recieve a rst", tcp_state_name(tcp->state));
        return NET_OK;
    }
    if (tcp_hdr->f_ack) {
        dbg_warning(DBG_TCP, "%s: recieve a ack", tcp_state_name(tcp->state));
        tcp_send_reset(seg);
        return NET_OK;
    }
    if (tcp_hdr->f_syn) {
        if (tcp_backlog_count(tcp) >= tcp->conn.backlog) {
            dbg_warning(DBG_TCP, "backlog full");
            return -E_NET_FULL;
        }
        Tcp *child = tcp_create_child(tcp, seg);
        if (child == nullptr) {
            dbg_error(DBG_TCP, "error: no tcp for accept");
            return -E_NO_MEM;
        }
        tcp_send_syn(child);
        tcp_set_state(child, TCP_STATE_SYN_RECVD);
        return NET_OK;
    }
    return -E_NET;
}

int tcp_syn_recvd_in(Tcp *tcp, Tcp_seg *seg) {
    Tcp_hdr *tcp_hdr = seg->hdr;
    if (tcp_hdr->f_rst) {
        dbg_warning(DBG_TCP, "%s: recieve a rst", tcp_state_name(tcp->state));
        return tcp_abort(tcp, -E_NET_RESET);
    }
    if (tcp_hdr->f_syn) {
        dbg_warning(DBG_TCP, "%s: recieve a syn", tcp_state_name(tcp->state));
        tcp_send_reset(seg);
        return tcp_abort(tcp, -E_NET_RESET);
    }
    if (tcp_ack_process(tcp, seg) < 0) {
        dbg_warning(DBG_TCP, "%s:  ack process failed", tcp_state_name(tcp->state));
        return -E_NET;
    }
    if (tcp_hdr->f_fin) {
        dbg_warning(DBG_TCP, "%s: recieve a fin", tcp_state_name(tcp->state));
        return -E_NET;
    } else {
        tcp_set_state(tcp, TCP_STATE_ESTABLISHED);
        tcp_keepalive_start(tcp, tcp->flags.keep_enable);
        if (tcp->parent) { sock_wakeup(info2sk(tcp->parent, tcp), WT_SOCK_CONN, NET_OK); }
    }
    tcp_out_event(tcp, TCP_OEVENT_SEND);
    // tcp_transmit(tcp);
    return NET_OK;
}
