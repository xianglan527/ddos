#include "tcp_in.h"

#include "protocol.h"
#include "slab.h"
#include "sock.h"
#include "stdio.h"
#include "tcp_out.h"
#include "tcp_state.h"

void tcp_seg_init(Tcp_seg *seg, Pktbuf *buf, Ipaddr *local, Ipaddr *remote) {
    seg->buf = buf;
    seg->hdr = *(Tcp_hdr *)pktbuf_data(buf);
    ipaddr_copy(&seg->local_ip, local);
    ipaddr_copy(&seg->remote_ip, remote);
    seg->data_len = buf->total_size - tcp_hdr_size(&seg->hdr);
    seg->seq = seg->hdr.seq;
    seg->seq_len = seg->data_len + seg->hdr.f_syn + seg->hdr.f_fin;
    seg->tcp_seg_list_type = TCP_SEG_LIST_TYPE_NONE;
    list_init(&seg->tcp_ofo_link);
    pktbuf_remove_header(seg->buf, tcp_hdr_size(&seg->hdr));
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
    Tcp_seg *seg = kmalloc(sizeof(Tcp_seg));
    tcp_seg_init(seg, buf, dest_ip, src_ip);
    Tcp *tcp = tcp_find(dest_ip, tcp_hdr->dport, src_ip, tcp_hdr->sport);
    if (!tcp || (tcp->state >= TCP_STATE_MAX)) {
        dbg_info(DBG_TCP, "no tcp found: port = %d", tcp_hdr->dport);
        // tcp_send_reset(&seg);
        tcp_closed_in(nullptr, seg);
        pktbuf_free(buf);
        tcp_show_list();
        return NET_OK;
    }
    // ret = pktbuf_seek(buf, tcp_hdr_size(tcp_hdr));
    // if (ret < 0) {
    //     dbg_error(DBG_TCP, "seek failed.");
    //     return -E_NET_SIZE;
    // }
    if ((tcp->state != TCP_STATE_CLOSED) && (tcp->state != TCP_STATE_SYN_SENT) &&
        (tcp->state != TCP_STATE_LISTEN) && (tcp->state != TCP_STATE_SYN_RECVD)) {
        if (!tcp_seq_acceptable(tcp, seg)) {
            dbg_info(DBG_TCP, "seq incorrect: %d < %d", seg->seq, tcp->rcv.nxt);
            if (!tcp_hdr->f_rst) { tcp_send_ack(tcp, seg); }
            if (seg->seq == tcp->rcv.nxt - 1) {
                dbg_info(DBG_TCP, "rcv keep alive");
            } else {
                dbg_info(DBG_TCP, "seq incorrect: %u < %u", seg->seq, tcp->rcv.nxt);
            }
            tcp_read_options(tcp, tcp_hdr);
            goto seg_drop;
        }
    }
    tcp_keepalive_restart(tcp);
    tcp_state_proc[tcp->state](tcp, seg);
    tcp_show_info("after tcp in", info2sk(tcp, tcp));
seg_drop:
    // pktbuf_free(buf);
    if (seg && list_empty(&seg->tcp_ofo_link)) {
        if (seg->buf) { pktbuf_free(seg->buf); }
        kfree(seg);
    }
    return NET_OK;
}

static void tcp_ofo_seq_insert(Tcp *tcp, Tcp_seg *seg) {
    tcp_show_ofo_list(tcp, "insert before");
    pktbuf_reset_acc(seg->buf);
    List_entry *head = &tcp->rcv.ofo_seq_list;
    List_entry *le, *prev = head;
    for (le = list_next(head); le != head; le = list_next(le)) {
        Tcp_seg *s = le2seq(le);
        if (TCP_SEQ_LE(seg->seq, s->seq)) {
            list_add_after(prev, &seg->tcp_ofo_link);
            break;
        }
        prev = le;
    }
    if (le == head) { list_add_after(prev, &seg->tcp_ofo_link); }
    seg->tcp_seg_list_type = TCP_SEG_LIST_TYPE_OFO;
    uint32_t seg_end = seg->seq + seg->data_len;
    List_entry *next_le = list_next(&seg->tcp_ofo_link);
    while (next_le != head) {
        Tcp_seg *next_s = le2seq(next_le);
        next_le = list_next(next_le);
        uint32_t next_start = next_s->seq;
        uint32_t next_end = next_s->seq + next_s->data_len;
        if (TCP_SEQ_LT(seg_end, next_start)) {
            break;
        } else if (TCP_SEQ_LE(next_end, seg_end)) {
            list_del_init(&next_s->tcp_ofo_link);
            pktbuf_free(next_s->buf);
            kfree(next_s);
        } else {
            uint32_t tail_add_count = next_end - seg_end;
            uint32_t off_seek = next_s->data_len - tail_add_count;
            pktbuf_remove_header(next_s->buf, off_seek);
            pktbuf_join(seg->buf, next_s->buf);  // In pktbuf_join, next_s->buf has already been kfree
            list_del_init(&next_s->tcp_ofo_link);
            kfree(next_s);
            seg->seq_len += tail_add_count, seg->data_len += tail_add_count;
            break;
        }
    }
    List_entry *prev_le = list_prev(&seg->tcp_ofo_link);
    if (prev_le != head) {
        Tcp_seg *prev_s = le2seq(prev_le);
        uint32_t prev_end = prev_s->seq + prev_s->data_len;
        if (TCP_SEQ_LE(seg->seq, prev_end)) {
            uint32_t head_cut_count = prev_end - seg->seq;
            uint32_t prev_add_count = seg->data_len - head_cut_count;
            pktbuf_remove_header(seg->buf, head_cut_count);
            pktbuf_join(prev_s->buf, seg->buf);
            // I don’t intend to free `seg` here, because all segments are freed in one place at the
            // `seg_drop` label in `tcp_in` function.
            // Also note that `pktbuf_join` will free `seg->buf`, so should explicitly set `seg->buf` to
            // NULL to distinguish it from segments that don’t enter the out-of-order queue (whose `seg->buf`
            // isn’t freed). When handling in the `seg_drop` section, be extra careful and check whether
            // `seg->buf` is NULL before freeing it, to avoid serious double-free issues.
            seg->buf = nullptr;
            list_del_init(&seg->tcp_ofo_link);
            prev_s->seq_len += prev_add_count, prev_s->data_len += prev_add_count;
        }
    }
    tcp_show_ofo_list(tcp, "insert after");
}

static size_t check_tcp_ofo_seq(Tcp *tcp, uint32_t check_seq) {
    tcp_show_ofo_list(tcp, "check before");
    List_entry *head = &tcp->rcv.ofo_seq_list;
    List_entry *next_le = list_next(head);
    while (next_le != head) {
        Tcp_seg *next_s = le2seq(next_le);
        next_le = list_next(next_le);
        uint32_t next_start = next_s->seq;
        uint32_t next_end = next_s->seq + next_s->data_len;
        if (TCP_SEQ_LT(check_seq, next_start)) {
            break;
        } 
        else if (TCP_SEQ_LE(next_end, check_seq)) {
            list_del_init(&next_s->tcp_ofo_link);
            pktbuf_free(next_s->buf);
            kfree(next_s);
        } else {
            uint32_t tail_count = next_end - check_seq;
            uint32_t off_seek = next_s->data_len - tail_count;
            pktbuf_seek(next_s->buf, off_seek); 
            size_t recv_write_size = tcp_buf_write_rcv(&tcp->rcv.buf, 0, next_s->buf, tail_count);
            size_t remain_recv = tail_count - recv_write_size;
            if (remain_recv) {
                uint32_t add_count = off_seek + recv_write_size;
                pktbuf_remove_header(next_s->buf, add_count);
                next_s->seq += add_count;
                next_s->seq_len -= add_count, next_s->data_len -= add_count;
            } else {
                list_del_init(&next_s->tcp_ofo_link);
                pktbuf_free(next_s->buf);
                kfree(next_s);
            }
            tcp_show_ofo_list(tcp, "check after");
            return recv_write_size;
        }
    }
    tcp_show_ofo_list(tcp, "check after");
    return 0;
}

static size_t copy_data_to_rcvbuf(Tcp *tcp, Tcp_seg *seg) {
    Tcp_hdr *tcp_hdr = &seg->hdr;
    Pktbuf *buf = seg->buf;

    uint32_t rlast = seg->seq + seg->data_len - 1;
    uint32_t wlast = tcp->rcv.nxt + tcp_buf_free_cnt(&tcp->rcv.buf) - 1;
    if (TCP_SEQ_LT(wlast, rlast)) {
        dbg_warning(DBG_TCP, "tcp incoming cut head");
        uint32_t tail_cut = rlast - wlast;  // if wlast = 0xFFFFFFF0，rlast = 0x00000010 ,0x00000010 -
                                            // 0xFFFFFFF0 == 0x00000020 ,the result is still corrected
        if (tcp_hdr->f_fin) {
            tcp_hdr->f_fin = 0;
            seg->seq_len--;
        }
        if (buf->total_size < tail_cut) {
            dbg_error(DBG_TCP, "tcp incoming cut head, buf size is not enough");
            return -1;
        }
        pktbuf_resize(buf, buf->total_size - tail_cut);
        seg->seq_len -= tail_cut;
        seg->data_len -= tail_cut;
    }
    pktbuf_reset_acc(buf);
    uint32_t head_cut = 0;
    if (TCP_SEQ_LT(seg->seq, tcp->rcv.nxt)) {
        dbg_warning(DBG_TCP, "tcp incoming cut tail");
        head_cut = tcp->rcv.nxt - seg->seq;
        tcp_hdr->seq += head_cut;
        seg->seq += head_cut;
        seg->seq_len -= head_cut;
        seg->data_len -= head_cut;
        pktbuf_seek(buf, head_cut);
    }
    off_t doffset = seg->seq - tcp->rcv.nxt;
    if (doffset == 0) {
        size_t rcv_count = tcp_buf_write_rcv(&tcp->rcv.buf, doffset, buf, seg->data_len);
        return rcv_count + check_tcp_ofo_seq(tcp, tcp_hdr->seq + rcv_count + tcp_hdr->f_syn + tcp_hdr->f_fin);
    } else {
        tcp_send_ack(tcp, seg);
        if (doffset && seg->data_len && !((seg->hdr.f_syn) || (seg->hdr.f_fin))) {
            // We do not consider segments with SYN and FIN flags here, which makes the processing simpler.
            tcp_ofo_seq_insert(tcp, seg);
        }
        return 0;
    }
}

int tcp_data_in(Tcp *tcp, Tcp_seg *seg) {
    int wakeup = 0;
    if ((seg->seq == tcp->rcv.nxt) && (seg->data_len == 1) && (tcp_rcv_window(tcp) == 0)) {
        tcp_send_ack(tcp, seg);
        return NET_OK;
    }
    size_t size = copy_data_to_rcvbuf(tcp, seg);
    if (size < 0) {
        dbg_error(DBG_TCP, "copy data to tcp rcvbuf failed.");
        return -E_NET_SIZE;
    }
    if (size > 0) {
        tcp->rcv.nxt += size;
        wakeup++;
    }
    // if(seg->hdr == nullptr){
    //     assert(size == 0);
    //     return NET_OK;
    // }
    if (seg->tcp_seg_list_type == TCP_SEG_LIST_TYPE_OFO) {
        assert(size == 0);
        return NET_OK;
    }
    Tcp_hdr *tcp_hdr = &seg->hdr;
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
