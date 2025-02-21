#include "tcp_in.h"

#include "protocol.h"
#include "sock.h"
#include "stdio.h"
#include "tcp_out.h"
#include "tcp_state.h"

void tcp_seg_init(Tcp_seg *seg, Pktbuf *buf, Ipaddr *local, Ipaddr *remote) {
    seg->buf = buf;
    seg->hdr = (Tcp_hdr *)pktbuf_data(buf);
    ipaddr_copy(&seg->local_ip, local);
    ipaddr_copy(&seg->remote_ip, remote);
    seg->data_len = buf->total_size - tcp_hdr_size(seg->hdr);
    seg->seq = seg->hdr->seq;
    seg->seq_len = seg->data_len + seg->hdr->f_syn + seg->hdr->f_fin;
}

/* RFC 793:
 * Segment Receive  Test
 * Length  Window
 * ------- -------  -------------------------------------------
 *    0       0     SEG.SEQ = RCV.NXT
 *    0      >0     RCV.NXT =< SEG.SEQ < RCV.NXT+RCV.WND
 *   >0       0     not acceptable
 *   >0      >0     RCV.NXT =< SEG.SEQ < RCV.NXT+RCV.WND
 *                  or RCV.NXT =< SEG.SEQ+SEG.LEN-1 < RCV.NXT+RCV.WND
 *
 */
static bool tcp_seq_acceptable(Tcp *tcp, Tcp_seg *seg) {
    uint32_t rcv_win = tcp_rcv_window(tcp);
    if (seg->seq_len == 0) {
        if (rcv_win == 0) {
            // 0(len)   0(win)     SEG.SEQ = RCV.NXT
            return seg->seq == tcp->rcv.nxt;
        } else {
            // 0(len)   >0(win)     RCV.NXT =< SEG.SEQ < RCV.NXT+RCV.WND
            bool v = TCP_SEQ_LE(tcp->rcv.nxt, seg->seq) && TCP_SEQ_LE(seg->seq, tcp->rcv.nxt + rcv_win - 1);
            return v;
        }
    } else {
        // >0(len)   0(win)    not acceptable
        if (rcv_win == 0) {
            return false;
        } else {
            // RCV.NXT =< SEG.SEQ < RCV.NXT+RCV.WND
            bool v = TCP_SEQ_LE(tcp->rcv.nxt, seg->seq) && TCP_SEQ_LE(seg->seq, tcp->rcv.nxt + rcv_win - 1);
            // RCV.NXT =< SEG.SEQ+SEG.LEN-1 < RCV.NXT+RCV.WND
            uint32_t slast = seg->seq + seg->seq_len - 1;
            v |= TCP_SEQ_LE(tcp->rcv.nxt, slast) && TCP_SEQ_LE(slast, tcp->rcv.nxt + rcv_win - 1);
            return v;
        }
    }
}

int tcp_in(Pktbuf *buf, Ipaddr *src_ip, Ipaddr *dest_ip) {
    static Tcp_proc tcp_state_proc[] = {
        [TCP_STATE_CLOSED] = tcp_closed_in,           [TCP_STATE_SYN_SENT] = tcp_syn_sent_in,
        [TCP_STATE_ESTABLISHED] = tcp_established_in, [TCP_STATE_FIN_WAIT_1] = tcp_fin_wait_1_in,
        [TCP_STATE_FIN_WAIT_2] = tcp_fin_wait_2_in,   [TCP_STATE_CLOSING] = tcp_closing_in,
        [TCP_STATE_TIME_WAIT] = tcp_time_wait_in,     [TCP_STATE_CLOSE_WAIT] = tcp_close_wait_in,
        [TCP_STATE_LAST_ACK] = tcp_last_ack_in,       [TCP_STATE_LISTEN] = tcp_listen_in,
        [TCP_STATE_SYN_RECVD] = tcp_syn_recvd_in,
    };
    int ret = pktbuf_set_cont(buf, sizeof(Tcp_hdr));
    if (ret < 0) {
        dbg_error(DBG_TCP, "set tcp cont failed");
        return ret;
    }
    Tcp_hdr *tcp_hdr = (Tcp_hdr *)pktbuf_data(buf);
    if (tcp_hdr_size(tcp_hdr) > sizeof(sizeof(Tcp_hdr))) {
        // pktbuf_reset_acc(buf);
        ret = pktbuf_set_cont(buf, sizeof(Tcp_hdr));
        if (ret < 0) {
            dbg_error(DBG_TCP, "set tcp cont failed");
            return ret;
        }
    }
    if (tcp_hdr->checksum) {
        pktbuf_reset_acc(buf);
        if (checksum_peso(dest_ip->a_addr, src_ip->a_addr, NET_PROTOCOL_TCP, buf)) {
            dbg_warning(DBG_TCP, "tcp checksum incorrect");
            return -E_NET_CHECKSUM;
        }
    }
    if ((buf->total_size < sizeof(Tcp_hdr)) || (buf->total_size < tcp_hdr_size(tcp_hdr))) {
        dbg_warning(DBG_TCP, "tcp packet size incorrect: %d!", buf->total_size);
        return -E_NET_SIZE;
    }
    if (!tcp_hdr->sport || !tcp_hdr->dport) {
        dbg_warning(DBG_TCP, "port == 0");
        return -E_NET_UNREACH;
    }
    if (tcp_hdr->flags == 0) {
        dbg_warning(DBG_TCP, "flag == 0");
        return -E_NET;
    }
    tcp_hdr->sport = x_ntohs(tcp_hdr->sport);
    tcp_hdr->dport = x_ntohs(tcp_hdr->dport);
    tcp_hdr->seq = x_ntohl(tcp_hdr->seq);
    tcp_hdr->ack = x_ntohl(tcp_hdr->ack);
    tcp_hdr->win = x_ntohs(tcp_hdr->win);
    tcp_hdr->urgptr = x_ntohs(tcp_hdr->urgptr);
    tcp_display_pkt("tcp packet in!", tcp_hdr, buf);
    Tcp_seg seg;
    tcp_seg_init(&seg, buf, dest_ip, src_ip);
    Tcp *tcp = tcp_find(dest_ip, tcp_hdr->dport, src_ip, tcp_hdr->sport);
    if (!tcp || (tcp->state >= TCP_STATE_MAX)) {
        dbg_info(DBG_TCP, "no tcp found: port = %d", tcp_hdr->dport);
        // tcp_send_reset(&seg);
        tcp_closed_in(nullptr, &seg);
        pktbuf_free(buf);
        tcp_show_list();
        return NET_OK;
    }
    ret = pktbuf_seek(buf, tcp_hdr_size(tcp_hdr));
    if (ret < 0) {
        dbg_error(DBG_TCP, "seek failed.");
        return -E_NET_SIZE;
    }
    if ((tcp->state != TCP_STATE_CLOSED) && (tcp->state != TCP_STATE_SYN_SENT) &&
        (tcp->state != TCP_STATE_LISTEN)) {
        if (!tcp_seq_acceptable(tcp, &seg)) {
            dbg_info(DBG_TCP, "seq incorrect: %d < %d", seg.seq, tcp->rcv.nxt);
            goto seg_drop;
        }
    }
    tcp_keepalive_restart(tcp);
    tcp_state_proc[tcp->state](tcp, &seg);
    tcp_show_info("after tcp in", info2sk(tcp, tcp));
seg_drop:
    pktbuf_free(buf);
    return NET_OK;
}

static size_t copy_data_to_rcvbuf(Tcp *tcp, Tcp_seg *seg) {
    Tcp_hdr *tcp_hdr = seg->hdr;
    Pktbuf *buf = seg->buf;
    off_t doffset = seg->seq - tcp->rcv.nxt;
    if (seg->data_len && (doffset == 0)) {
        return tcp_buf_write_rcv(&tcp->rcv.buf, doffset, buf, seg->data_len);
    }
    return 0;
}

int tcp_data_in(Tcp *tcp, Tcp_seg *seg) {
    int wakeup = 0;
    size_t size = copy_data_to_rcvbuf(tcp, seg);
    if (size > 0) {
        tcp->rcv.nxt += size;
        wakeup++;
    }
    Tcp_hdr *tcp_hdr = seg->hdr;
    if (tcp_hdr->f_fin && (tcp->rcv.nxt == seg->seq)) {
        tcp->rcv.nxt++;
        tcp->flags.fin_in = 1;  
        wakeup++;
    }
    if (wakeup) {
        if (tcp->flags.fin_in) {
            sock_wakeup(info2sk(tcp, tcp), WT_SOCK_ALL, -E_NET_CLOSE);
        } else {
            sock_wakeup(info2sk(tcp, tcp), WT_SOCK_READ, NET_OK);
        }
        tcp_send_ack(tcp, seg);
    }
    return NET_OK;
}
