#ifndef __NET_PKTBUF_H__
#define __NET_PKTBUF_H__

#include "list.h"
#include "net.h"
#include "spinlock.h"
#include "types.h"
#include "wait.h"
#include "atomic.h"

typedef struct pktblk Pktblk;
struct pktblk{
    List_entry pktblk_link;
    size_t size;
    uint8_t *data;
    uint8_t payload[PKTBUF_BLK_SIZE];
};

#define le2pktblk(le) to_struct((le), Pktblk, pktblk_link)

typedef struct pktbuf Pktbuf;
struct pktbuf{
    size_t total_size;
    List_entry pktblk_list;
    List_entry pktbuf_link;
    off_t pos;
    Atomic ref;
    Pktblk *cur_blk;  
    uint8_t *blk_offset;
    Spinlock pktblk_lock;  // read & write & seek lock
    List_entry pktbuf_wait_link;  //wait arp request pkt
};

#define le2pktbuf(le) to_struct((le), Pktbuf, pktbuf_link)

#define le2pktbuf_wait(le) to_struct((le), Pktbuf, pktbuf_wait_link)

typedef struct pktbuf_head Pktbuf_head;
struct pktbuf_head {
    size_t max_mem_size;
    volatile size_t mem_size;
    Spinlock pktbuf_lock;
    Wait_queue wait_queue;
    List_entry pktbuf_list;
};

static inline uint8_t *pktbuf_data(Pktbuf *buf) {
    if(list_empty(&buf->pktblk_list)){
        return nullptr;
    }
    return le2pktblk(list_next(&buf->pktblk_list))->data;
}

static inline void pktbuf_inc_ref(Pktbuf *buf){
    atomic_inc(&buf->ref);
}

int pktbuf_init(void);
Pktbuf *pktbuf_alloc(size_t size);
void pktbuf_free(Pktbuf *buf);
int pktbuf_add_header(Pktbuf *buf, size_t size, bool cont);
int pktbuf_remove_header(Pktbuf *buf, size_t size);
int pktbuf_resize(Pktbuf *buf, size_t to_size);
int pktbuf_join(Pktbuf *dest, Pktbuf *src);
int pktbuf_set_cont(Pktbuf *buf, size_t size);
void pktbuf_reset_acc(Pktbuf *buf);
int pktbuf_write(Pktbuf *buf, uint8_t *src, size_t size);
int pktbuf_read(Pktbuf *buf, uint8_t *dest, size_t size);
int pktbuf_seek(Pktbuf *buf, off_t offset);
int pktbuf_copy(Pktbuf *dest, Pktbuf *src, size_t size);
int pktbuf_fill(Pktbuf *buf, uint8_t v, int size);
#endif