#include "tcp_out.h"

#include "ipv4.h"
#include "protocol.h"
#include "sock.h"
#include "stdio.h"
extern uint64_t sys_gettime(void);
extern uint64_t sys_time_goes(uint64_t pre);

static int send_out(Tcp_hdr *out, Pktbuf *buf, Ipaddr *dest, Ipaddr *src) {
    tcp_display_pkt("tcp out", out, buf);
    out->sport = x_htons(out->sport);
    out->dport = x_htons(out->dport);
    out->seq = x_htonl(out->seq);
    out->ack = x_htonl(out->ack);
    out->win = x_htons(out->win);
    out->urgptr = x_htons(out->urgptr);
    out->checksum = 0;
    out->checksum = checksum_peso(dest->a_addr, src->a_addr, NET_PROTOCOL_TCP, buf);
    int ret = ipv4_out(NET_PROTOCOL_TCP, dest, src, buf);
    if (ret < 0) {
        dbg_info(DBG_TCP, "send tcp buf error");
        pktbuf_free(buf);
    }
    return ret;
}

int tcp_send_reset(Tcp_seg *seg) {
    Tcp_hdr *in = &seg->hdr;
    // If the incoming segment already has the RST flag set, ignore it.
    if (in->f_rst) {
        dbg_info(DBG_TCP, "reset, ignore");
        return NET_OK;
    }
    // Allocate a packet buffer for the TCP header.
    Pktbuf *buf = pktbuf_alloc(sizeof(Tcp_hdr));
    if (!buf) {
        dbg_warning(DBG_TCP, "no pktbuf");
        return -E_NO_MEM;
    }
    Tcp_hdr *out = (Tcp_hdr *)pktbuf_data(buf);
    // Swap source and destination ports for the reset packet.
    out->sport = in->dport;
    out->dport = in->sport;
    // Initialize flags to 0 and set the RST flag.
    out->flags = 0;
    out->f_rst = 1;
    /*
     * SEQ: Use SEQ to inform the peer that this packet has been received and is being responded to.
     * This ensures the packet is correctly received by the peer.
     * ACK: Inform the peer that all data up to a certain sequence number has been received, and to send data
     * beyond that.
     */
    if (in->f_ack) {
        // If an ACK has been received from the peer, use the ACK number as the starting SEQ.
        // This ensures the SEQ is within the acceptable range for the peer.
        out->seq = in->ack;
        // Since setting the SEQ above ensures the peer will correctly receive it,
        // there's no need to set the ACK field. It can remain unset.
        out->ack = 0;
        out->f_ack = 0;  // Reset the ACK flag along with the RST flag.
    } else {
        /*
         * If an ACK has not been received from the peer (e.g., only a SYN packet was received),
         * meaning the peer has not specified an expected SEQ number, the starting SEQ is unknown.
         * Therefore, set SEQ to 0.
         *
         * However, to ensure the peer can accept the RST packet, the peer does not simply accept any RST
         * packet. In this case, the peer checks for an ACK packet (as seen in the handling within the
         * SYN_SENT state). It verifies whether the ACK is within the appropriate range to process it. Thus,
         * an ACK must be sent back to the peer.
         */
        out->seq = 0;
        // Since the expected data SEQ from the peer is unknown, set ACK to SEQ + seg->seq_len.
        out->ack = in->seq + seg->seq_len;
        out->f_ack = 1;  // Set the ACK flag along with the RST flag.
    }

    // Set the header size for the TCP packet.
    tcp_set_hdr_size(out, sizeof(Tcp_hdr));
    // Window size and urgent pointer are not used in the reset packet.
    out->win = out->urgptr = 0;
    // Send out the reset packet.
    return send_out(out, buf, &seg->remote_ip, &seg->local_ip);
}

static void get_send_info(Tcp *tcp, bool rexmit, bool no_newdata, ssize_t *doff, ssize_t *dlen) {
    if (TCP_SEQ_LT(tcp->snd.nxt, tcp->snd.una)) {
        dbg_error(DBG_TCP, "tcp->snd.nxt < tcp->snd.una");
        return;
    }
    if (rexmit) {
        *doff = 0;
        if (no_newdata) {
            *dlen = tcp->snd.nxt - tcp->snd.una;  // 仅未确认的数据
        } else {
            *dlen = tcp_buf_cnt(&tcp->snd.buf) - *doff;
        }
    } else {
        *doff = tcp->flags.syn_out ? 0 : tcp->snd.nxt - tcp->snd.una;
        *dlen = (size_t)tcp_buf_cnt(&tcp->snd.buf) - *doff;
    }
    if (tcp->snd.win == 0) {
        *dlen = 1;
    } else {
        *dlen = (*dlen > tcp->mss) ? tcp->mss : *dlen;
        size_t win = tcp->snd.cwin < tcp->snd.win ? tcp->snd.cwin : tcp->snd.win;
        if (*dlen + *doff > win) { *dlen = win - *doff; }
    }
}

static size_t copy_send_data(Tcp *tcp, Pktbuf *buf, off_t doff, size_t dlen) {
    if (dlen == 0) { return 0; }
    size_t real_dlen = dlen <= tcp->snd.buf.count - doff ? dlen : tcp->snd.buf.count;
    if (real_dlen == 0) { return 0; }
    int ret = pktbuf_resize(buf, buf->total_size + real_dlen);
    if (ret < 0) {
        dbg_error(DBG_TCP, "pktbuf resize error");
        return ret;
    }
    size_t hdr_size = tcp_hdr_size((Tcp_hdr *)pktbuf_data(buf));
    pktbuf_reset_acc(buf);
    pktbuf_seek(buf, (off_t)hdr_size);
    tcp_buf_read_send(&tcp->snd.buf, doff, buf, real_dlen);
    return real_dlen;
}

static void do_nagle_ack_timer_tmo(Timer *timer) {
    Tcp *tcp = (Tcp *)timer->arg;
    dbg_warning(DBG_TCP, "nagle timer tmo: %d", tcp->flags.ACK_delay);
    assert(tcp);
    if (tcp->flags.ACK_delay == false)
        return;
    else {
        tcp->flags.ACK_delay = false;
        Pktbuf *buf = pktbuf_alloc(sizeof(Tcp_hdr));
        if (!buf) {
            dbg_error(DBG_TCP, "no buffer");
            return;
        }
        Socket *socket = info2sk(tcp, tcp);
        Tcp_hdr *out = (Tcp_hdr *)pktbuf_data(buf);
        out->sport = socket->local_port;
        out->dport = socket->remote_port;
        out->seq = tcp->snd.nxt;
        out->ack = tcp->rcv.nxt;
        out->flags = 0;
        out->f_ack = 1;  // ACK标志
        out->win = (uint16_t)tcp_rcv_window(tcp);
        out->urgptr = 0;
        tcp_set_hdr_size(out, sizeof(Tcp_hdr));
        send_out(out, buf, &socket->remote_ip, &socket->local_ip);
    }
}

int tcp_send_win_update(Tcp *tcp) {
    if (tcp->flags.ACK_delay == false && !tcp_do_input_nagle(tcp)) {
        tcp->flags.ACK_delay = true;
        tcp->conn.nagle_timer = timer_func_add(do_nagle_ack_timer_tmo, tcp, TCP_NAGLE_TMO, 0);
        return NET_OK;
    }
    tcp->flags.ACK_delay = false;
    Pktbuf *buf = pktbuf_alloc(sizeof(Tcp_hdr));
    if (!buf) {
        dbg_error(DBG_TCP, "no buffer");
        return -E_NO_MEM;
    }
    Socket *socket = info2sk(tcp, tcp);
    Tcp_hdr *out = (Tcp_hdr *)pktbuf_data(buf);
    out->sport = socket->local_port;
    out->dport = socket->remote_port;
    out->seq = tcp->snd.nxt;
    out->ack = tcp->rcv.nxt;
    out->flags = 0;
    out->f_ack = 1;  // ACK标志
    out->win = (uint16_t)tcp_rcv_window(tcp);
    out->urgptr = 0;
    tcp_set_hdr_size(out, sizeof(Tcp_hdr));
    return send_out(out, buf, &socket->remote_ip, &socket->local_ip);
}

static void write_sync_option(Tcp *tcp, Pktbuf *buf) {
    size_t opt_len = sizeof(Tcp_opt_mss);
    int ret = pktbuf_resize(buf, buf->total_size + opt_len);
    if (ret < 0) {
        dbg_error(DBG_TCP, "resize error");
        return;
    }
    Tcp_opt_mss mss;
    mss.kind = TCP_OPT_MSS;
    mss.length = sizeof(Tcp_opt_mss);
    mss.mss = x_ntohs(tcp->mss);
    pktbuf_reset_acc(buf);
    pktbuf_seek(buf, sizeof(Tcp_hdr));
    pktbuf_write(buf, (uint8_t *)&mss, sizeof(mss));
}

int tcp_transmit(Tcp *tcp) {
    ssize_t dlen, doff;
    get_send_info(tcp, false, false, &doff, &dlen);
    if (dlen < 0 || !tcp_do_output_nagle(tcp, dlen)) { return NET_OK; }
    // if (!tcp_do_input_nagle(tcp)) {
    //     // tcp->conn.nagle_timer = timer_func_add(do_nagle_ack_timer_tmo, tcp, TCP_NAGLE_TMO, 0);
    //     cprintf("xxx get get here %ld\n", tcp_rcv_window(tcp));
    //     return NET_OK;
    // }
    // tcp->flags.ACK_delay = false;
    ssize_t seq_len = dlen;
    if (tcp->flags.syn_out) { seq_len++; }
    if ((tcp_buf_cnt(&tcp->snd.buf) == 0) && tcp->flags.fin_out) { seq_len++; }
    if (seq_len == 0) { return NET_OK; }
    Socket *socket = info2sk(tcp, tcp);
    Pktbuf *buf = pktbuf_alloc(sizeof(Tcp_hdr));
    if (!buf) {
        dbg_error(DBG_TCP, "no buffer");
        return -E_NO_MEM;
    }
    Tcp_hdr *hdr = (Tcp_hdr *)pktbuf_data(buf);
    memset(hdr, 0, sizeof(Tcp_hdr));
    hdr->sport = socket->local_port;
    hdr->dport = socket->remote_port;
    hdr->seq = tcp->snd.nxt;
    hdr->ack = tcp->rcv.nxt;
    hdr->flags = 0;
    hdr->f_syn = tcp->flags.syn_out;
    if (hdr->f_syn) { write_sync_option(tcp, buf); }
    hdr->f_ack = tcp->flags.irs_valid;
    hdr->f_fin = (tcp_buf_cnt(&tcp->snd.buf) == 0) ? tcp->flags.fin_out : 0;
    hdr->win = (uint16_t)tcp_rcv_window(tcp);
    hdr->urgptr = 0;
    tcp_set_hdr_size(hdr, buf->total_size);
    copy_send_data(tcp, buf, doff, (size_t)dlen);
    tcp->snd.nxt += dlen + hdr->f_syn + hdr->f_fin;
    dbg_info(DBG_TCP, "tcp send: dlen %d, seqlen: %d, %s", dlen, seq_len, tcp_ostate_name(tcp));
    return send_out(hdr, buf, &socket->remote_ip, &socket->local_ip);
}

int tcp_send_syn(Tcp *tcp) {
    // Socket *socket = info2sk(tcp, tcp);
    tcp->flags.syn_out = 1;
    // tcp_transmit(tcp);
    tcp_out_event(tcp, TCP_OEVENT_SEND);
    return NET_OK;
}

static void tcp_cal_snd_wnd(Tcp *tcp, Tcp_seg *seg) {
    Tcp_hdr *tcp_hdr = &seg->hdr;
    if (TCP_SEQ_LT(tcp->snd.wl1_seq, seg->seq) ||
        ((tcp->snd.wl1_seq == seg->seq) && (TCP_SEQ_LE(tcp->snd.wl2_ack, tcp_hdr->ack)))) {
        size_t pre_win = tcp->snd.win;
        tcp->snd.win = tcp_hdr->win;
        tcp->snd.wl1_seq = seg->seq;
        tcp->snd.wl2_ack = tcp_hdr->ack;
    }
}

/**
 * The formulas given in TCP/IP Illustrated:
 *      Err = RTT - srtt;
 *      srtt = srtt + g * Err,    where g = 1/8
 *      rttvar = rttvar + h * (|Err| - rttvar),    where h = 1/4
 *      RTO = srtt + 4 * rttvar
 *
 * These can be rearranged to avoid data loss when shifting:
 *      Err = RTT - srtt
 *      8 * srtt    = 8 * srtt    + Err
 *      4 * rttvar  = 4 * rttvar  + |Err| - rttvar
 *      RTO = srtt + 4 * rttvar
 *
 * Before measuring round-trip time (RTT) for segments exchanged
 * between sender and receiver, the sender should set RTO < 1 second.
 *
 * When RTT is measured for the first time with value R, the host must initialize:
 *      SRTT   = R
 *      RTTVAR = R / 2
 *      RTO    = SRTT + max{G, K * RTTVAR}
 *      where K = 4
 */
void tcp_cal_rto(Tcp *tcp) {
    if (tcp->flags.rto_going == 0) { return; }
    int64_t rtt = sys_time_goes(tcp->snd.rtttime);
    if (rtt == 0) { rtt = 1; }
    if (tcp->snd.srtt != 0) {
        int64_t delta = rtt - (tcp->snd.srtt >> 3);  // Err = RTT - srtt
        tcp->snd.srtt += delta;                      // 8*srtt = 8*srtt + Err
        // 4*rttvar = 4*rttvar + |Err| - rttvar
        tcp->snd.rttvar += ((delta < 0 ? -delta : delta)) - (tcp->snd.rttvar >> 2);
    } else {
        tcp->snd.srtt = rtt << 3;        // 8*srtt <- M
        tcp->snd.rttvar = rtt << 1;      // 4*rttvar <- M/2
        tcp->snd.rttseq = tcp->snd.nxt;  // current seq
    }
    // RTO = srtt + 4*rttvar
    tcp->snd.rto = (tcp->snd.srtt >> 3) + tcp->snd.rttvar;
    if (tcp->snd.rto < TCP_RTO_MIN) { tcp->snd.rto = TCP_RTO_MIN; }
    tcp->flags.rto_going = 0;
    dbg_warning(DBG_TCP, "rttseq: %u, rto: %ld, rtt: %ld, srtt: %ld, rttvar: %ld", tcp->snd.rttseq,
                tcp->snd.rto, rtt, tcp->snd.srtt >> 3, tcp->snd.rttvar >> 2);
    dbg_warning(DBG_TCP, "rtttime %lu", tcp->snd.rtttime);
}

int tcp_ack_process(Tcp *tcp, Tcp_seg *seg) {
    tcp_cal_snd_wnd(tcp, seg);
    Tcp_hdr *tcp_hdr = &seg->hdr;
    // una < ack <= nxt
    if (TCP_SEQ_LE(tcp_hdr->ack, tcp->snd.una) &&
        ((tcp->snd.ostate == TCP_OSTATE_SENDING) && (++tcp->snd.dup_ack >= TCP_DUPTHRESH))) {
        if(!tcp->flags.fast_recovery){
            tcp_retransmit(tcp, false);
            tcp_set_ostate(tcp, TCP_OSTATE_REXMIT);
            ssize_t wnd = tcp->snd.cwin < tcp->snd.win ? tcp->snd.cwin : tcp->snd.win;
            tcp->snd.sthresh = wnd >> 1;
            if (tcp->snd.sthresh < tcp->mss * 2) { tcp->snd.sthresh = tcp->mss * 2; }
            tcp->snd.cwin = tcp->snd.sthresh + 3 * tcp->mss;
            tcp->flags.fast_recovery = true;
        }else{
            tcp->snd.cwin +=
                (tcp->mss * tcp->mss / tcp->snd.cwin) < 1 ? 1 : (tcp->mss * tcp->mss / tcp->snd.cwin);
        }
        return NET_OK;

    } else if (TCP_SEQ_LT(tcp->snd.nxt, tcp_hdr->ack)) {
        if (tcp->state == TCP_STATE_SYN_RECVD) {
            tcp_send_reset(seg);
        } else {
            tcp_send_ack(tcp, seg);
        }
        return -E_NET_UNREACH;
    }
    tcp->snd.dup_ack = 0;
    if(tcp->flags.fast_recovery){
        tcp->flags.fast_recovery = false;
        tcp->snd.cwin = tcp->snd.sthresh;
    }
    if (tcp->flags.syn_out) {
        tcp->snd.una++;
        tcp->flags.syn_out = 0;
    }
    if(tcp->snd.cwin < tcp->snd.sthresh){
        tcp->snd.cwin += tcp->snd.cwin;
        if(tcp->snd.cwin > tcp->snd.sthresh){
            tcp->snd.cwin = tcp->snd.sthresh;
        }
    }else{
        ssize_t add_cwin =
            (tcp->mss * tcp->mss / tcp->snd.cwin) < 1 ? 1 : (tcp->mss * tcp->mss / tcp->snd.cwin);
        tcp->snd.cwin += add_cwin; 
    }
    size_t acked_cnt = tcp_hdr->ack - tcp->snd.una;
    size_t unacked_cnt = tcp->snd.nxt - tcp->snd.una;
    size_t curr_acked = (acked_cnt > unacked_cnt) ? unacked_cnt : acked_cnt;
    if (curr_acked > 0) {
        tcp->snd.una += curr_acked;
        curr_acked -= tcp_buf_remove(&tcp->snd.buf, curr_acked);
        if (tcp_buf_cnt(&tcp->snd.buf) == 0 && curr_acked && (tcp->flags.fin_out)) { tcp->flags.fin_out = 0; }
        sock_wakeup(info2sk(tcp, tcp), WT_SOCK_WRITE, NET_OK);
    }
    // if (tcp->flags.fin_out && (tcp_hdr->ack > tcp->snd.una)) { tcp->flags.fin_out = 0; }
    if (TCP_SEQ_LE(tcp->snd.rttseq, tcp->snd.una)) { tcp_cal_rto(tcp); }
    return NET_OK;
}

int tcp_send_ack(Tcp *tcp, Tcp_seg *seg) {
    if (seg->hdr.f_rst) { return NET_OK; }
    if (tcp->flags.ACK_delay == false && !tcp_do_input_nagle(tcp)) {
        tcp->flags.ACK_delay = true;
        tcp->conn.nagle_timer = timer_func_add(do_nagle_ack_timer_tmo, tcp, TCP_NAGLE_TMO, 0);
        return NET_OK;
    }
    tcp->flags.ACK_delay = false;
    Socket *socket = info2sk(tcp, tcp);
    Pktbuf *buf = pktbuf_alloc(sizeof(Tcp_hdr));
    if (!buf) {
        dbg_error(DBG_TCP, "no buffer");
        return -E_NO_MEM;
    }
    Tcp_hdr *out = (Tcp_hdr *)pktbuf_data(buf);
    out->sport = socket->local_port;
    out->dport = socket->remote_port;
    out->seq = tcp->snd.nxt;
    out->ack = tcp->rcv.nxt;
    out->flags = 0;
    out->f_ack = 1;
    out->win = (uint16_t)tcp_rcv_window(tcp);
    out->urgptr = 0;
    tcp_set_hdr_size(out, sizeof(Tcp_hdr));
    return send_out(out, buf, &seg->remote_ip, &seg->local_ip);
}

int tcp_send_fin(Tcp *tcp) {
    tcp->flags.fin_out = 1;
    // tcp_transmit(tcp);
    tcp_out_event(tcp, TCP_OEVENT_SEND);
    return NET_OK;
}

size_t tcp_write_sndbuf(Tcp *tcp, uint8_t *buf, size_t len) {
    int free_cnt = tcp_buf_free_cnt(&tcp->snd.buf);
    if (free_cnt == 0) { return 0; }

    // 计算实际能写入的大小
    int wr_len = (len > free_cnt) ? free_cnt : len;
    tcp_buf_write_send(&tcp->snd.buf, buf, wr_len);
    return wr_len;
}

int tcp_send_keepalive(Tcp *tcp) {
    Pktbuf *buf = pktbuf_alloc(sizeof(Tcp_hdr));
    if (!buf) {
        dbg_warning(DBG_TCP, "no pktbuf");
        return -E_NO_MEM;
    }
    Tcp_hdr *out = (Tcp_hdr *)pktbuf_data(buf);
    Socket *socket = info2sk(tcp, tcp);
    out->sport = socket->local_port;
    out->dport = socket->remote_port;
    out->seq = tcp->snd.nxt - 1;
    out->ack = tcp->rcv.nxt;
    out->flags = 0;
    out->f_ack = 1;
    out->win = (uint16_t)tcp_rcv_window(tcp);
    out->urgptr = 0;
    tcp_set_hdr_size(out, sizeof(Tcp_hdr));
    return send_out(out, buf, &socket->remote_ip, &socket->local_ip);
}

int tcp_send_reset_for_tcp(Tcp *tcp) {
    Pktbuf *buf = pktbuf_alloc(sizeof(Tcp_hdr));
    if (!buf) {
        dbg_warning(DBG_TCP, "no pktbuf");
        return -E_NO_MEM;
    }
    Tcp_hdr *out = (Tcp_hdr *)pktbuf_data(buf);
    Socket *socket = info2sk(tcp, tcp);
    out->sport = socket->local_port;
    out->dport = socket->remote_port;
    out->seq = tcp->snd.nxt;
    out->ack = tcp->rcv.nxt;
    out->flags = 0;
    out->f_ack = 1;
    out->f_rst = 1;
    out->win = (uint16_t)tcp_rcv_window(tcp);
    out->urgptr = 0;
    tcp_set_hdr_size(out, sizeof(Tcp_hdr));
    return send_out(out, buf, &socket->remote_ip, &socket->local_ip);
}

char *tcp_oevent_name(Tcp_oevent event) {
    static char *oevent_name[] = {
        [TCP_OEVENT_SEND] = "oevent_send",
        [TCP_OEVENT_XMIT] = "oevent_xmit",
    };
    if (event != TCP_OEVENT_SEND && event != TCP_OEVENT_XMIT) { dbg_error(DBG_TCP, "tcp out event error."); }
    return oevent_name[event];
}

char *tcp_ostate_name(Tcp *tcp) {
    static char *state_name[] = {
        [TCP_OSTATE_IDLE] = "idle",       [TCP_OSTATE_SENDING] = "sending", [TCP_OSTATE_REXMIT] = "resending",
        [TCP_OSTATE_PERSIST] = "persist", [TCP_OSTATE_MAX] = "unknow",
    };
    return state_name[tcp->snd.ostate >= TCP_OSTATE_MAX ? TCP_OSTATE_MAX : tcp->snd.ostate];
}

// sudo tc qdisc add dev br0 root netem loss 40%
// sudo tc qdisc show dev br0
// sudo tc qdisc del dev br0 root
// sudo tc qdisc replace dev br0 root netem delay 200ms reorder 100% gap 2 limit 2000 loss 40%
int tcp_retransmit(Tcp *tcp, bool no_newdata) {
    ssize_t dlen, doff;
    get_send_info(tcp, true, no_newdata, &doff, &dlen);
    if (dlen < 0 || (!tcp_do_output_nagle(tcp, dlen) && !no_newdata)) {
        // cprintf("get here 111\n");
        return NET_OK;
    }
    // if (!tcp_do_input_nagle(tcp)) {
    //     cprintf("yyy get get here %ld\n", tcp_rcv_window(tcp));
    //     return NET_OK;
    // }
    // tcp->flags.ACK_delay = false;
    // int seq_len = dlen;
    // if (tcp->flags.syn_out) { seq_len++; }
    // if (tcp->flags.fin_out) { seq_len++; }
    Socket *socket = info2sk(tcp, tcp);
    Pktbuf *buf = pktbuf_alloc(sizeof(Tcp_hdr));
    if (!buf) {
        dbg_error(DBG_TCP, "no buffer");
        return -E_NO_MEM;
    }
    Tcp_hdr *hdr = (Tcp_hdr *)pktbuf_data(buf);
    memset(hdr, 0, sizeof(Tcp_hdr));
    hdr->sport = socket->local_port;
    hdr->dport = socket->remote_port;
    hdr->seq = tcp->snd.una;
    hdr->ack = tcp->rcv.nxt;
    hdr->flags = 0;
    hdr->f_syn = tcp->flags.syn_out;
    if (hdr->f_syn) { write_sync_option(tcp, buf); }
    hdr->f_ack = tcp->flags.irs_valid;
    hdr->f_fin = (tcp_buf_cnt(&tcp->snd.buf) == 0) ? tcp->flags.fin_out : 0;
    hdr->win = (uint16_t)tcp_rcv_window(tcp);
    hdr->urgptr = 0;
    tcp_set_hdr_size(hdr, buf->total_size);
    size_t send_size = 0;
    if (dlen - tcp->flags.syn_out - tcp->flags.fin_out > 0) {
        send_size = copy_send_data(tcp, buf, doff, dlen - tcp->flags.syn_out - tcp->flags.fin_out);
    }
    if (send_size == 0 && !tcp->flags.syn_out && !tcp->flags.fin_out) {
        pktbuf_free(buf);
        return NET_OK;
    }
    size_t diff = 0;
    uint32_t una_rlimit = tcp->snd.una + dlen;
    if (TCP_SEQ_LT(tcp->snd.nxt, una_rlimit)) {
        diff = una_rlimit - tcp->snd.nxt - tcp->flags.syn_out - tcp->flags.fin_out;
        tcp->snd.nxt += diff;
    }
    dbg_info(DBG_TCP, "tcp send: seq %u, ack %u, dlen %ld, diff %ld, %s", hdr->seq, hdr->ack, dlen, diff,
             tcp_ostate_name(tcp));
    return send_out(hdr, buf, &socket->remote_ip, &socket->local_ip);
}

static void tcp_begin_rto(Tcp *tcp) {
    if (tcp->flags.rto_going == 0) {
        tcp->snd.rttseq = tcp->snd.nxt;
        tcp->snd.rtttime = sys_gettime();
        dbg_warning(DBG_TCP, "rttseq %u", tcp->snd.rttseq);
        dbg_warning(DBG_TCP, "rtttime %llu", tcp->snd.rtttime);
        tcp->flags.rto_going = 1;
    }
}

static inline void tcp_end_rto(Tcp *tcp) { tcp->flags.rto_going = 0; }

void tcp_out_timer_tmo(Timer *timer) {
    Tcp *tcp = (Tcp *)timer->arg;
    dbg_warning(DBG_TCP, "timer tmo: %s", tcp_ostate_name(tcp));
    // timer_func_dump();
    ssize_t wnd = tcp->snd.cwin < tcp->snd.win ? tcp->snd.cwin : tcp->snd.win;
    tcp->snd.sthresh = wnd >> 1;
    if(tcp->snd.sthresh < tcp->mss * 2){
        tcp->snd.sthresh = tcp->mss * 2;
    }
    tcp->snd.cwin = tcp->mss;
    switch (tcp->snd.ostate) {
        case TCP_OSTATE_SENDING: {
            // int ret = 0;
            tcp_end_rto(tcp);
            int ret = tcp_retransmit(tcp, true);
            if (ret < 0) {
                dbg_error(DBG_TCP, "rexmit failed.");
                return;
            }
            tcp->snd.rexmit_cnt = 1;
            tcp->snd.rto <<= 1;
            tcp->snd.ostate = TCP_OSTATE_REXMIT;
            tcp->snd.snd_timer = timer_func_add(tcp_out_timer_tmo, tcp, tcp->snd.rto, 0);
            break;
        }
        case TCP_OSTATE_REXMIT: {
            if ((++tcp->snd.rexmit_cnt > tcp->snd.rexmit_max)) {
                dbg_error(DBG_TCP, "rexmit tmo err");
                tcp_abort(tcp, -E_NET_TMO);
                return;
            }
            // int ret = 0;
            int ret = tcp_retransmit(tcp, true);
            if (ret < 0) {
                dbg_error(DBG_TCP, "rexmit failed.");
                return;
            }
            // if(ret == NET_OK_NO_TRANSMIT_DATA){
            //     cprintf("get here xx \n");
            //     tcp_set_ostate(tcp, TCP_OSTATE_IDLE);
            //     break;
            // }
            tcp->snd.rto <<= 1;
            if (tcp->snd.rto >= TCP_RTO_MAX) { tcp->snd.rto = TCP_RTO_MAX; }
            tcp->snd.snd_timer = timer_func_add(tcp_out_timer_tmo, tcp, tcp->snd.rto, 0);
            break;
        }
        case TCP_OSTATE_PERSIST: {
            if (TCP_PERSIST_RETRIES && (++tcp->snd.persist_cnt > TCP_PERSIST_RETRIES)) {
                dbg_error(DBG_TCP, "persist tmo err");
                tcp_abort(tcp, -E_NET_TMO);
                return;
            }
            int ret = tcp_retransmit(tcp, true);
            if (ret < 0) {
                dbg_error(DBG_TCP, "send win query failed.");
                return;
            }
            int64_t tmo = tcp->snd.rto << tcp->snd.persist_cnt;
            if (tmo >= TCP_RTO_MAX) { tmo = TCP_RTO_MAX; }
            tcp->snd.snd_timer = timer_func_add(tcp_out_timer_tmo, tcp, tmo, 0);
            break;
        }
        default: dbg_error(DBG_TCP, "tcp state error: %d", tcp->state); return;
    }
}

void tcp_set_ostate(Tcp *tcp, Tcp_ostate state) {
    if (state >= TCP_OSTATE_MAX) {
        dbg_error(DBG_TCP, "unknown state: %d", tcp->snd.ostate);
        return;
    }
    int tmo = 0;
    switch (state) {
        case TCP_OSTATE_IDLE:
            tcp->snd.ostate = state;
            if (tcp->snd.snd_timer != nullptr) {
                del_func_timer(tcp->snd.snd_timer);
                tcp->snd.snd_timer = nullptr;
            }
            return;
        case TCP_OSTATE_SENDING:
            tmo = tcp->snd.rto;
            tcp->snd.rexmit_cnt = 0;
            break;
        case TCP_OSTATE_REXMIT:
            tmo = tcp->snd.rto;
            tcp->snd.rexmit_cnt = 0;
            break;
        case TCP_OSTATE_PERSIST:
            tmo = TCP_PERSIST_TMO;
            tcp->snd.persist_cnt = 0;
            break;
        default: break;
    }
    tcp->snd.ostate = state;
    if (tcp->snd.snd_timer != nullptr) {
        del_func_timer(tcp->snd.snd_timer);
        tcp->snd.snd_timer = nullptr;
    }
    tcp->snd.snd_timer = timer_func_add(tcp_out_timer_tmo, tcp, tmo, 0);
}

static void tcp_ostate_idle_in(Tcp *tcp, Tcp_oevent event) {
    switch (event) {
        case TCP_OEVENT_SEND:;
            if (tcp->snd.win) {
                tcp_transmit(tcp);
                tcp_set_ostate(tcp, TCP_OSTATE_SENDING);
                tcp_begin_rto(tcp);
            } else {
                // tcp_transmit(tcp);
                tcp_set_ostate(tcp, TCP_OSTATE_PERSIST);
            }
            break;
        default: break;
    }
}

static void tcp_ostate_sending_in(Tcp *tcp, Tcp_oevent event) {
    switch (event) {
        case TCP_OEVENT_SEND:
            if (tcp->snd.win) {
                tcp_transmit(tcp);
                tcp_set_ostate(tcp, TCP_OSTATE_SENDING);
                tcp_begin_rto(tcp);
            } else {
                // tcp_transmit(tcp);
                tcp_set_ostate(tcp, TCP_OSTATE_PERSIST);
            }
            break;
        case TCP_OEVENT_XMIT:
            if ((tcp->snd.una == tcp->snd.nxt) || tcp->flags.fin_out) {
                if (tcp_buf_cnt(&tcp->snd.buf) || tcp->flags.fin_out) {
                    if (tcp->snd.win) {
                        tcp_transmit(tcp);
                        tcp_set_ostate(tcp, TCP_OSTATE_SENDING);
                        tcp_begin_rto(tcp);
                    } else {
                        tcp_set_ostate(tcp, TCP_OSTATE_PERSIST);
                        // tcp_transmit(tcp);
                    }
                } else {
                    tcp_set_ostate(tcp, TCP_OSTATE_IDLE);
                }
            }
            break;
        default: break;
    }
}

static void tcp_ostate_rexmit_in(Tcp *tcp, Tcp_oevent event) {
    switch (event) {
        case TCP_OEVENT_XMIT: {
            if ((tcp->snd.una == tcp->snd.nxt) || tcp->flags.fin_out) {
                if (tcp_buf_cnt(&tcp->snd.buf) || tcp->flags.fin_out) {
                    if (tcp->snd.win) {
                        tcp_transmit(tcp);
                        tcp_set_ostate(tcp, TCP_OSTATE_SENDING);
                    } else {
                        tcp_set_ostate(tcp, TCP_OSTATE_PERSIST);
                    }
                } else {
                    tcp_set_ostate(tcp, TCP_OSTATE_IDLE);
                }
            } else {
                if (tcp->snd.win) {
                    dbg_info(DBG_TCP, "rxmit ack in, retransmit, seq: %lu, ack: %lu", tcp->snd.una,
                             tcp->rcv.nxt);
                    tcp_set_ostate(tcp, TCP_OSTATE_REXMIT);
                } else {
                    tcp_set_ostate(tcp, TCP_OSTATE_PERSIST);
                }
                // tcp_retransmit(tcp, false);
            }
            break;
        }
        default: break;
    }
}

static void tcp_ostate_persist_in(Tcp *tcp, Tcp_oevent event) {
    switch (event) {
        case TCP_OEVENT_XMIT: {
            if (tcp->snd.win) {
                if ((tcp->snd.una == tcp->snd.nxt) || tcp->flags.fin_out) {
                    tcp_transmit(tcp);
                    tcp_set_ostate(tcp, TCP_OSTATE_SENDING);
                } else {
                    tcp_set_ostate(tcp, TCP_OSTATE_REXMIT);
                    tcp_retransmit(tcp, true);
                }
            }
            break;
        }
        default: break;
    }
}

void tcp_out_event(Tcp *tcp, Tcp_oevent event) {
    static void (*state_fun[])(Tcp *tcp, Tcp_oevent event) = {
        [TCP_OSTATE_IDLE] = tcp_ostate_idle_in,
        [TCP_OSTATE_SENDING] = tcp_ostate_sending_in,
        [TCP_OSTATE_REXMIT] = tcp_ostate_rexmit_in,
        [TCP_OSTATE_PERSIST] = tcp_ostate_persist_in,
    };
    if (tcp->snd.ostate >= TCP_OSTATE_MAX) {
        dbg_error(DBG_TCP, "tcp ostate unknown: %d", tcp->snd.ostate);
        return;
    }
    dbg_info(DBG_TCP, "current ostate: %s oevent : %s", tcp_ostate_name(tcp), tcp_oevent_name(event));
    state_fun[tcp->snd.ostate](tcp, event);
}
