#include "tcp_buf.h"

void tcp_buf_init(Tcp_buf *buf, uint8_t *data, size_t size) {
    buf->in = buf->out = 0;
    buf->count = 0;
    buf->size = size;
    buf->data = data;
}

void tcp_buf_write_send(Tcp_buf *dest, uint8_t *buffer, size_t len) {
    while (len > 0) {
        dest->data[dest->in++] = *buffer++;
        if (dest->in >= dest->size) { dest->in = 0; }
        dest->count++;
        len--;
    }
}

void tcp_buf_read_send(Tcp_buf *buf, off_t offset, Pktbuf *dest, size_t count){
    size_t free_for_us = buf->count - offset;  
    if (count > free_for_us) {
        dbg_warning(DBG_TCP, "resize for send: %d -> %d", count, free_for_us);
        count = free_for_us;
    }
    size_t start = buf->out + offset;  
    if (start >= buf->size) { start -= buf->size; }
    while (count > 0) {
        size_t end = start + count;
        if (end >= buf->size) { end = buf->size; }
        size_t copy_size = (size_t)(end - start);
        int ret = pktbuf_write(dest, buf->data + start, (size_t)copy_size);
        assert(ret >= 0);
        start += copy_size;
        if (start >= buf->size) { start -= buf->size; }
        count -= copy_size;
    }
}

size_t tcp_buf_remove(Tcp_buf *buf, size_t cnt) {
    if (cnt > buf->count) { cnt = buf->count; }
    buf->out += cnt;
    if (buf->out >= buf->size) { buf->out -= buf->size; }
    buf->count -= cnt;
    return cnt;
}

size_t tcp_buf_write_rcv(Tcp_buf *dest, off_t offset, Pktbuf *src, size_t total){
    size_t start = dest->in + offset;
    if (start >= dest->size) { start = start - dest->size; }
    size_t free_size = tcp_buf_free_cnt(dest) - offset; 
    total = (total > free_size) ? free_size : total;
    size_t size = total;
    while (size > 0) {
        size_t free_to_end = dest->size - start;
        size_t curr_copy = size > free_to_end ? free_to_end : size;
        pktbuf_read(src, dest->data + start, curr_copy);
        start += curr_copy;
        if (start >= dest->size) { start = start - dest->size; }
        dest->count += curr_copy;
        size -= curr_copy;
    }
    dest->in = start;
    return total;
}

size_t tcp_buf_read_rcv(Tcp_buf *src, uint8_t *buf, size_t size) {
    size_t total = size > src->count ? src->count : size;
    size_t curr_size = 0;
    while (curr_size < total) {
        *buf++ = src->data[src->out++];
        if (src->out >= src->size) { src->out = 0; }
        src->count--;
        curr_size++;
    }
    return total;
}