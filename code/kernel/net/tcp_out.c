#include "tcp_out.h"
#include "protocol.h"
#include "ipv4.h"
#include "sock.h"
#include "stdio.h"

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
    Tcp_hdr *in = seg->hdr;
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

static void get_send_info(Tcp *tcp, bool rexmit, ssize_t *doff, ssize_t *dlen) {
    if (rexmit) {
        *doff = 0;
        *dlen = tcp_buf_cnt(&tcp->snd.buf) - *doff;
    }else{
        *doff = tcp->flags.syn_out ? 0 : tcp->snd.nxt - tcp->snd.una;
        *dlen = tcp_buf_cnt(&tcp->snd.buf) - *doff;
    }
    if (*dlen == 0) { return; }
    *dlen = (*dlen > tcp->mss) ? tcp->mss : *dlen;
}

static size_t copy_send_data(Tcp *tcp, Pktbuf *buf, off_t doff, size_t dlen) {
    if (dlen == 0) { return 0; }
    int ret = pktbuf_resize(buf, buf->total_size + dlen);
    if (ret < 0) {
        dbg_error(DBG_TCP, "pktbuf resize error");
        return ret;
    }
    size_t hdr_size = tcp_hdr_size((Tcp_hdr *)pktbuf_data(buf));
    pktbuf_reset_acc(buf);
    pktbuf_seek(buf, (off_t)hdr_size);
    tcp_buf_read_send(&tcp->snd.buf, doff, buf, dlen);
    return dlen;
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
    get_send_info(tcp, false, &doff, &dlen);
    if (dlen < 0) { return NET_OK; }
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
    // cprintf(".............nxt is %ld\n",  tcp->snd.nxt );
    return send_out(hdr, buf, &socket->remote_ip, &socket->local_ip);
}

int tcp_send_syn(Tcp *tcp) {
    // Socket *socket = info2sk(tcp, tcp);
    tcp->flags.syn_out = 1;
    // tcp_transmit(tcp);
    tcp_out_event(tcp, TCP_OEVENT_SEND);
    return NET_OK;
}

int tcp_ack_process(Tcp *tcp, Tcp_seg *seg) {
    Tcp_hdr *tcp_hdr = seg->hdr;
    // una < ack <= nxt
    if (TCP_SEQ_LE(tcp_hdr->ack, tcp->snd.una)) {
        return NET_OK;
    } else if (TCP_SEQ_LT(tcp->snd.nxt, tcp_hdr->ack)) {
        return -E_NET_UNREACH;
    }
    if (tcp->flags.syn_out) {
        tcp->snd.una++;
        tcp->flags.syn_out = 0;
    }
    size_t acked_cnt = tcp_hdr->ack - tcp->snd.una;    
    size_t unacked_cnt = tcp->snd.nxt - tcp->snd.una; 
    size_t curr_acked = (acked_cnt > unacked_cnt) ? unacked_cnt : acked_cnt;
    if (curr_acked > 0) {
        tcp->snd.una += curr_acked;
        curr_acked -= tcp_buf_remove(&tcp->snd.buf, curr_acked);  
        if (curr_acked && (tcp->flags.fin_out)) { tcp->flags.fin_out = 0; }
        // if(tcp_buf_cnt(&tcp->snd.buf) == 0){
        //     sock_wakeup(info2sk(tcp, tcp), WT_SOCK_WRITE, NET_OK);
        // }else{
        //     tcp_transmit(tcp);
        // }
        sock_wakeup(info2sk(tcp, tcp), WT_SOCK_WRITE, NET_OK);
    }
    // if (tcp->flags.fin_out && (tcp_hdr->ack > tcp->snd.una)) { tcp->flags.fin_out = 0; }
    return NET_OK;
}

int tcp_send_ack(Tcp *tcp, Tcp_seg *seg) {
    if (seg->hdr->f_rst) { return NET_OK; }
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

size_t tcp_write_sndbuf(Tcp *tcp, uint8_t *buf, size_t len){
    int free_cnt = tcp_buf_free_cnt(&tcp->snd.buf);
    if (free_cnt == 0) { return 0; }

    // 计算实际能写入的大小
    int wr_len = (len > free_cnt) ? free_cnt : len;
    tcp_buf_write_send(&tcp->snd.buf, buf, wr_len);
    return wr_len;
}

int tcp_send_keepalive(Tcp *tcp){
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

int tcp_send_reset_for_tcp(Tcp *tcp){
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

char *tcp_ostate_name(Tcp *tcp){
    static char *state_name[] = {
        [TCP_OSTATE_IDLE] = "idle",
        [TCP_OSTATE_SENDING] = "sending",
        [TCP_OSTATE_REXMIT] = "resending",
        [TCP_OSTATE_MAX] = "unknow",
    };
    return state_name[tcp->snd.ostate >= TCP_OSTATE_MAX ? TCP_OSTATE_MAX : tcp->snd.ostate];
}

int tcp_retransmit(Tcp *tcp) {
    ssize_t dlen, doff;
    get_send_info(tcp, true, &doff, &dlen);
    if (dlen < 0) { return NET_OK; }
    int seq_len = dlen;
    if (tcp->flags.syn_out) { seq_len++; }
    if (tcp->flags.fin_out) { seq_len++; }
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
    if (hdr->f_syn) {
        write_sync_option(tcp, buf);
    }
    hdr->f_ack = tcp->flags.irs_valid;
    hdr->f_fin = (tcp_buf_cnt(&tcp->snd.buf) == 0) ? tcp->flags.fin_out : 0;
    hdr->win = (uint16_t)tcp_rcv_window(tcp);
    hdr->urgptr = 0;
    tcp_set_hdr_size(hdr, buf->total_size);
    copy_send_data(tcp, buf, doff, dlen);
    // int diff = tcp->snd.una + dlen - tcp->snd.nxt;
    // tcp->snd.nxt += diff > 0 ? diff : 0;
    tcp->snd.nxt = dlen + hdr->f_syn + hdr->f_fin + tcp->snd.una;
    dbg_info(DBG_TCP, "tcp send: seq %u, ack %u, dlen %d, seqlen: %d, %s", hdr->seq, hdr->ack, 0, seq_len,
             tcp_ostate_name(tcp));
    return send_out(hdr, buf, &socket->remote_ip, &socket->local_ip);
}

void tcp_out_timer_tmo(Timer *timer) {
    Tcp *tcp = (Tcp *)timer->arg;
    dbg_warning(DBG_TCP, "timer tmo: %s", tcp_ostate_name(tcp));
    switch (tcp->snd.ostate) {
        case TCP_OSTATE_SENDING: {
            // int ret = 0;
            int ret = tcp_retransmit(tcp);
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
            int ret = tcp_retransmit(tcp);
            if (ret < 0) {
                dbg_error(DBG_TCP, "rexmit failed.");
                return;
            }
            tcp->snd.rto <<= 1;
            if (tcp->snd.rto >= TCP_RTO_MAX) { tcp->snd.rto = TCP_RTO_MAX; }
            tcp->snd.snd_timer = timer_func_add(tcp_out_timer_tmo, tcp, tcp->snd.rto, 0);
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
            tcp->snd.rto = TCP_INIT_RTO;
            if(tcp->snd.snd_timer != nullptr){
                del_func_timer(tcp->snd.snd_timer);
                tcp->snd.snd_timer = nullptr;
            }
            return;
        case TCP_OSTATE_SENDING: tmo = tcp->snd.rto; break;
        case TCP_OSTATE_REXMIT:
            tmo = tcp->snd.rto;  // 仍然使用RTO
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
        case TCP_OEVENT_SEND:
            tcp_transmit(tcp);
            tcp_set_ostate(tcp, TCP_OSTATE_SENDING);
            break;
        default: break;
    }
}

static void tcp_ostate_sending_in(Tcp *tcp, Tcp_oevent event) {
    switch (event) {
        case TCP_OEVENT_SEND:
            if ((tcp->snd.una == tcp->snd.nxt) || tcp->flags.fin_out) {
                if (tcp_buf_cnt(&tcp->snd.buf) || tcp->flags.fin_out) {
                    tcp_transmit(tcp);
                    tcp_set_ostate(tcp, TCP_OSTATE_SENDING);
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
        case TCP_OEVENT_SEND: {
            if ((tcp->snd.una == tcp->snd.nxt) || tcp->flags.fin_out) {
                if (tcp_buf_cnt(&tcp->snd.buf) || tcp->flags.fin_out) {
                    tcp_transmit(tcp);
                    tcp_set_ostate(tcp, TCP_OSTATE_SENDING);
                } else {
                    tcp_set_ostate(tcp, TCP_OSTATE_IDLE);
                }
            } else {
                tcp_set_ostate(tcp, TCP_OSTATE_REXMIT);
                tcp_retransmit(tcp);
            }
            break;
        }
        default: break;
    }
}

void tcp_out_event(Tcp *tcp, Tcp_oevent event){
    static void (*state_fun[])(Tcp *tcp, Tcp_oevent event) = {
        [TCP_OSTATE_IDLE] = tcp_ostate_idle_in,
        [TCP_OSTATE_SENDING] = tcp_ostate_sending_in,
        [TCP_OSTATE_REXMIT] = tcp_ostate_rexmit_in,
    };
    if (tcp->snd.ostate >= TCP_OSTATE_MAX) {
        dbg_error(DBG_TCP, "tcp ostate unknown: %d", tcp->snd.ostate);
        return;
    }
    state_fun[tcp->snd.ostate](tcp, event);
}
