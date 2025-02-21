#ifndef __NET_TCP_BUF_H__
#define __NET_TCP_BUF_H__
#include "error.h"
#include "list.h"
#include "net_config.h"
#include "nettool.h"
#include "pktbuf.h"
#include "spinlock.h"
#include "types.h"
#include "assert.h"

typedef struct tcp_buf {
    size_t count;
    size_t in, out;
    size_t size;
    uint8_t *data; 
} Tcp_buf;

void tcp_buf_init(Tcp_buf *buf, uint8_t *data, size_t size);

static inline size_t tcp_buf_size(Tcp_buf *buf) { return buf->size; }

static inline size_t tcp_buf_free_cnt(Tcp_buf *buf) {
    assert(buf->size >= buf->count);
    return buf->size - buf->count;
}

static inline size_t tcp_buf_cnt(Tcp_buf *buf) { return buf->count; }

void tcp_buf_write_send(Tcp_buf *dest, uint8_t *buffer, size_t len);
void tcp_buf_read_send(Tcp_buf *buf, off_t offset, Pktbuf *dest, size_t count);
size_t tcp_buf_remove(Tcp_buf *buf, size_t cnt);
size_t tcp_buf_write_rcv(Tcp_buf *dest, off_t offset, Pktbuf *src, size_t total);
size_t tcp_buf_read_rcv(Tcp_buf *src, uint8_t *buf, size_t size);
#endif