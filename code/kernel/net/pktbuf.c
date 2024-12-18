#include "pktbuf.h"

#include "assert.h"
#include "proc.h"
#include "slab.h"
#include "stdio.h"
#include "string.h"

Pktbuf_head *pktbuf_head;
extern Spinlock print_struct_lock;

int pktbuf_init(void) {
    pktbuf_head = kmalloc(sizeof(*pktbuf_head));
    assert(pktbuf_head != nullptr);
    // atomic_set(&pktbuf_head->mem_size, 0);
    pktbuf_head->mem_size = 0;
    pktbuf_head->max_mem_size = PKTBUF_BLK_CNT * PKTBUF_BLK_SIZE;
    wait_queue_init(&pktbuf_head->wait_queue);
    initlock(&pktbuf_head->pktbuf_lock, "pktbuf_lock");
    list_init(&pktbuf_head->pktbuf_list);
    dbg_info(DBG_BUF, "pktbuf_init done.");
    return NET_OK;
}

static bool pkbuf_wait_nolock(void) {
    // acquire(&pktbuf_head->pktbuf_lock);
    Wait __wait, *wait = &__wait;
    wait_current_set(&pktbuf_head->wait_queue, wait, WT_PKTBUF);
    sleeping(myproc(), &pktbuf_head->pktbuf_lock);
    wait_current_del(&pktbuf_head->wait_queue, wait);
    // release(&pktbuf_head->pktbuf_lock);
    return wait->wakeup_flags == WT_PKTBUF;
}

static void pkbuf_wakeup_nolock(void) {
    // acquire(&pktbuf_head->pktbuf_lock);
    if (!wait_queue_empty(&pktbuf_head->wait_queue)) { wakeup_first(&pktbuf_head->wait_queue, WT_PKTBUF, 1); }
    // release(&pktbuf_head->pktbuf_lock);
}

static Pktblk *pktblock_alloc(void) {
    Pktblk *blk = kmalloc(sizeof(*blk));
    assert(blk != nullptr);
    blk->data = (uint8_t *)nullptr;
    blk->size = 0;
    list_init(&blk->pktblk_link);
    return blk;
}

static void print_check_blkbuf(Pktbuf *buf, bool print) {
    if (!buf) { panic("invalid buf. buf == nullptr"); }
    if (print) cprintf("check buf %p: size %d\n", buf, buf->total_size);
    size_t total_size = 0;
    int index = 0;
    List_entry *le;
    list_for_each(le, &buf->pktblk_list) {
        Pktblk *blk = le2pktblk(le);
        if ((blk->data < blk->payload) || (blk->data >= blk->payload + PKTBUF_BLK_SIZE)) {
            dbg_error(DBG_BUF, "net bad pktblk data.");
        }
        size_t pre_size = (size_t)(blk->data - blk->payload);
        size_t used_size = blk->size;
        size_t tail_size = PKTBUF_BLK_SIZE - ((size_t)(blk->data - blk->payload) + blk->size);
        if (print)
            cprintf("%d: Pre:%lu b, used:%lu b, Free:%lu b\n", index++, pre_size, used_size, tail_size);
        size_t blk_total_size = pre_size + used_size + tail_size;
        assert(blk_total_size == PKTBUF_BLK_SIZE);
        total_size += used_size;
    }

    assert(total_size == buf->total_size);
}

static void print_check_blkbuf_list(bool print) {
    acquire(&pktbuf_head->pktbuf_lock);
    acquire(&print_struct_lock);
    if (print) cprintf("\nblkbuf_list dump....................................\n\n");
    List_entry *le;
    size_t count = 0;
    list_for_each(le, &pktbuf_head->pktbuf_list) {
        Pktbuf *buf = le2pktbuf(le);
        count += buf->total_size;
        print_check_blkbuf(buf, print);
        if (print) cprintf("\n");
    }
    if (print) cprintf("\nend of blkbuf_list dump.............................\n");
    release(&print_struct_lock);
    assert(count == pktbuf_head->mem_size && pktbuf_head->mem_size <= pktbuf_head->max_mem_size);
    release(&pktbuf_head->pktbuf_lock);
}

static void pktblock_alloc_list(Pktbuf *buf, size_t size, int add_front) {
    while (size) {
        Pktblk *block = kmalloc(sizeof(*block));
        assert(block != nullptr);
        size_t cur_size = 0;
        if (add_front) {
            cur_size = size > PKTBUF_BLK_SIZE ? PKTBUF_BLK_SIZE : size;
            block->size = cur_size;
            block->data = block->payload + PKTBUF_BLK_SIZE - cur_size;
            list_add_after(&buf->pktblk_list, &block->pktblk_link);
        } else {
            cur_size = size > PKTBUF_BLK_SIZE ? PKTBUF_BLK_SIZE : size;
            block->size = cur_size;
            block->data = block->payload;
            list_add_after(buf->pktblk_list.prev, &block->pktblk_link);
        }
        size -= cur_size;
    }
}

Pktbuf *pktbuf_alloc(size_t size) {
    Pktbuf *buf = nullptr;
    acquire(&pktbuf_head->pktbuf_lock);
    while (pktbuf_head->mem_size + size > pktbuf_head->max_mem_size) {
        int ret = pkbuf_wait_nolock();
        assert(ret == true);
    }
    buf = kmalloc(sizeof(*buf));
    assert(buf != nullptr);
    atomic_set(&buf->ref, 1);
    buf->total_size = size;
    list_init(&buf->pktblk_list);
    initlock(&buf->pktblk_lock, "pktblk_lock");
    pktblock_alloc_list(buf, size, 1);
    list_add_after(pktbuf_head->pktbuf_list.prev, &buf->pktbuf_link);
    pktbuf_head->mem_size += size;
    release(&pktbuf_head->pktbuf_lock);
    pktbuf_reset_acc(buf);
    print_check_blkbuf_list(0);
    return buf;
}

static void __pktbuf_free(Pktbuf *buf) {
    acquire(&pktbuf_head->pktbuf_lock);
    // acquire(&buf->pktblk_lock);
    if (!list_empty(&buf->pktblk_list)) {
        List_entry *list = &buf->pktblk_list, *le;
        while ((le = list_next(list)) != list) {
            list_del(le);
            kfree(le2pktblk(le));
        }
    }
    // release(&buf->pktblk_lock);
    list_del(&buf->pktbuf_link);
    pktbuf_head->mem_size -= buf->total_size;
    pkbuf_wakeup_nolock();
    release(&pktbuf_head->pktbuf_lock);
    kfree(buf);
    print_check_blkbuf_list(0);
}

void pktbuf_free(Pktbuf *buf){
    if(atomic_dec_test_zero(&buf->ref)){
        __pktbuf_free(buf);
    }
}

int pktbuf_add_header(Pktbuf *buf, size_t size, bool cont) {
    assert(!list_empty(&buf->pktblk_list));
    acquire(&pktbuf_head->pktbuf_lock);
    while (pktbuf_head->mem_size + size > pktbuf_head->max_mem_size) {
        int ret = pkbuf_wait_nolock();
        assert(ret == true);
    }
    Pktblk *blk = le2pktblk(list_next(&buf->pktblk_list));
    size_t reserve_size = (size_t)(blk->data - blk->payload);
    pktbuf_head->mem_size += size;
    if (size <= reserve_size) {
        blk->size += size;
        blk->data -= size;
        buf->total_size += size;
    } else {
        if (cont) {
            if (size > PKTBUF_BLK_SIZE) {
                warn("size is too big %ul > %ul", size, PKTBUF_BLK_SIZE);
                return -E_NET_ERROR_SIZE;
            }
            pktblock_alloc_list(buf, size, 1);
            buf->total_size += size;
        } else {
            blk->data = blk->payload;
            blk->size += reserve_size;
            assert(blk->size == PKTBUF_BLK_SIZE);
            buf->total_size += size;
            size -= reserve_size;
            pktblock_alloc_list(buf, size, 1);
        }
    }
    release(&pktbuf_head->pktbuf_lock);
    print_check_blkbuf_list(0);
    return NET_OK;
}

int pktbuf_remove_header(Pktbuf *buf, size_t size) {
    assert(!list_empty(&buf->pktblk_list));
    assert(size <= buf->total_size);
    acquire(&pktbuf_head->pktbuf_lock);
    List_entry *list = &buf->pktblk_list, *le;
    Pktblk *blk;
    buf->total_size -= size;
    pktbuf_head->mem_size -= size;
    while (size) {
        le = list_next(list);
        blk = le2pktblk(le);
        if (size < blk->size) {
            blk->data += size;
            blk->size -= size;
            break;
        } else {
            list_del(le);
            size -= blk->size;
            kfree(blk);
            le = list_next(list);
            assert(le != list);
        }
    }
    pkbuf_wakeup_nolock();
    release(&pktbuf_head->pktbuf_lock);
    print_check_blkbuf_list(0);
    return NET_OK;
}

int pktbuf_resize(Pktbuf *buf, size_t to_size) {
    if (to_size == buf->total_size) { return NET_OK }
    // if (to_size == 0) {
    //     pktbuf_free(buf);
    //     return NET_OK
    // }
    acquire(&pktbuf_head->pktbuf_lock);
    if (to_size > buf->total_size) {
        size_t inc_size = to_size - buf->total_size;
        while (pktbuf_head->mem_size + inc_size > pktbuf_head->max_mem_size) {
            int ret = pkbuf_wait_nolock();
            assert(ret == true);
        }
        if (buf->total_size == 0) {
            pktblock_alloc_list(buf, to_size, 0);
        } else {
            Pktblk *tail_blk = le2pktblk(buf->pktblk_list.prev);
            size_t remain_size =
                PKTBUF_BLK_SIZE - ((size_t)(tail_blk->data - tail_blk->payload) + tail_blk->size);
            if (remain_size >= inc_size) {
                tail_blk->size += inc_size;
            } else {
                tail_blk->size += remain_size;
                pktblock_alloc_list(buf, inc_size - remain_size, 0);
            }
        }
        buf->total_size += inc_size;
        pktbuf_head->mem_size += inc_size;
    } else {
        size_t dec_size = buf->total_size - to_size;
        buf->total_size -= dec_size;
        pktbuf_head->mem_size -= dec_size;
        Pktblk *tail_blk = le2pktblk(buf->pktblk_list.prev);
        while (dec_size) {
            if (tail_blk->size > dec_size) {
                tail_blk->size -= dec_size;
                break;
            } else {
                list_del(&tail_blk->pktblk_link);
                dec_size -= tail_blk->size;
                kfree(tail_blk);
                if (list_empty(&buf->pktblk_list)) {
                    assert(to_size == 0);
                    break;
                }
                tail_blk = le2pktblk(buf->pktblk_list.prev);
            }
        }
        pkbuf_wakeup_nolock();
    }
    release(&pktbuf_head->pktbuf_lock);
    print_check_blkbuf_list(0);
    return NET_OK;
}

int pktbuf_join(Pktbuf *dest, Pktbuf *src) {
    print_check_blkbuf_list(0);
    acquire(&pktbuf_head->pktbuf_lock);
    list_join(&dest->pktblk_list, &src->pktblk_list);
    dest->total_size += src->total_size;
    list_del(&src->pktbuf_link);
    kfree(src);
    release(&pktbuf_head->pktbuf_lock);
    print_check_blkbuf_list(0);
    return NET_OK;
}

int pktbuf_set_cont(Pktbuf *buf, size_t size) {
    if (size > buf->total_size) {
        dbg_error(DBG_BUF, "size too big > %lu", buf->total_size);
        return -E_NET_ERROR_SIZE;
    }
    if (size > PKTBUF_BLK_SIZE) {
        dbg_error(DBG_BUF, "size too big > %lu", PKTBUF_BLK_SIZE);
        return -E_NET_ERROR_SIZE;
    }
    acquire(&pktbuf_head->pktbuf_lock);
    assert(!list_empty(&buf->pktblk_list));
    List_entry *le = list_next(&buf->pktblk_list);
    Pktblk *first_blk = le2pktblk(le);
    if (first_blk->size >= size) {
        release(&pktbuf_head->pktbuf_lock);
        return NET_OK;
    }
    Pktblk *current_blk = nullptr;
    memcpy(first_blk->payload, first_blk->data, first_blk->size);
    first_blk->data = first_blk->payload;
    size_t remaining = size - first_blk->size;
    le = list_next(le);
    while (remaining > 0) {
        assert(le != &buf->pktblk_list);
        current_blk = le2pktblk(le);
        size_t move_size = (current_blk->size > remaining) ? remaining : current_blk->size;
        memcpy(first_blk->data + first_blk->size, current_blk->data, move_size);
        first_blk->size += move_size;
        current_blk->data += move_size;
        current_blk->size -= move_size;
        if (current_blk->size == 0) {
            List_entry *next_le = list_next(le);
            list_del(le);
            kfree(current_blk);
            le = next_le;
        }
        remaining -= move_size;
    }
    assert(remaining == 0);
    release(&pktbuf_head->pktbuf_lock);
    print_check_blkbuf_list(0);
    return NET_OK;
}

void pktbuf_reset_acc(Pktbuf *buf) {
    acquire(&buf->pktblk_lock);
    if (buf) {
        buf->pos = 0;
        if (!list_empty(&buf->pktblk_list)) {
            buf->cur_blk= le2pktblk(list_next(&buf->pktblk_list));
            buf->blk_offset = buf->cur_blk->data;
        } else {
            buf->cur_blk= nullptr;
            buf->blk_offset = nullptr;
        }
    }
    release(&buf->pktblk_lock);
}

static size_t cur_blk_remain_nolock(Pktbuf *buf) {
    Pktblk *blk = buf->cur_blk;
    if (!blk) { return 0; }
    return (size_t)(buf->cur_blk->data + blk->size - buf->blk_offset);
}

static void move_forward_nolock(Pktbuf *buf, size_t size){
    Pktblk *cur = buf->cur_blk;
    buf->pos += size;
    buf->blk_offset += size;
    if(buf->blk_offset >= cur->data + cur->size){
        List_entry *le = list_next(&cur->pktblk_link);
        if(le != &buf->pktblk_list){
            buf->cur_blk = le2pktblk(le);
            buf->blk_offset = buf->cur_blk->data;
        }else{
            buf->cur_blk = nullptr;
            buf->blk_offset = nullptr;
        }
    }
}

int pktbuf_write(Pktbuf *buf, uint8_t *src, size_t size) {
    if (!src || !size) { return -E_NET_ERROR_PARAM; }
    acquire(&buf->pktblk_lock);
    size_t remain_size = buf->total_size - buf->pos;
    if (remain_size < size) {
        dbg_error(DBG_BUF, "size errorL %d < %d", remain_size, size);
        release(&buf->pktblk_lock);
        return -E_NET_ERROR_SIZE;
    }
    while(size > 0){
        size_t blk_size = cur_blk_remain_nolock(buf);
        size_t cur_copy_size = size > blk_size ? blk_size : size;
        memcpy(buf->blk_offset, src, cur_copy_size);
        move_forward_nolock(buf, cur_copy_size);
        src += cur_copy_size;
        size -= cur_copy_size;
    }
    release(&buf->pktblk_lock);
    return NET_OK;
}

int pktbuf_read(Pktbuf *buf, uint8_t *dest, size_t size) {
    if (!dest || !size) { return -NET_OK; }
    acquire(&buf->pktblk_lock);
    size_t remain_size = buf->total_size - buf->pos;
    if (remain_size < size) {
        dbg_error(DBG_BUF, "size errorL %d < %d", remain_size, size);
        return -E_NET_ERROR_SIZE;
        release(&buf->pktblk_lock);
    }
    while (size > 0) {
        size_t blk_size = cur_blk_remain_nolock(buf);
        size_t cur_copy_size = size > blk_size ? blk_size : size;
        memcpy(dest, buf->blk_offset, cur_copy_size);
        move_forward_nolock(buf, cur_copy_size);
        dest += cur_copy_size;
        size -= cur_copy_size;
    }
    release(&buf->pktblk_lock);
    return NET_OK;
}

int pktbuf_seek(Pktbuf *buf, off_t offset){
    acquire(&buf->pktblk_lock);
    if (buf->pos == offset) {
        release(&buf->pktblk_lock);
        return NET_OK;
    }
    if ((offset < 0) || (offset >= buf->total_size)) {
        release(&buf->pktblk_lock);
        return -E_NET_ERROR_SIZE;
    }
    assert(!list_empty(&buf->pktblk_list));
    size_t move_size;
    if(offset < buf->pos){
        buf->cur_blk = le2pktblk(list_next(&buf->pktblk_list));
        buf->blk_offset = buf->cur_blk->data;
        buf->pos = 0;
        move_size = offset;
    }else{
        move_size = offset - buf->pos;
    }
    while(move_size){
        size_t remain_size = cur_blk_remain_nolock(buf);
        size_t cur_move_size = move_size > remain_size ? remain_size : move_size;
        move_forward_nolock(buf, cur_move_size);
        move_size -= cur_move_size;
    }
    release(&buf->pktblk_lock);
    return NET_OK;
}

int pktbuf_copy(Pktbuf *dest, Pktbuf *src, size_t size){
    acquire(&dest->pktblk_lock);
    acquire(&src->pktblk_lock);
    if (dest->total_size - dest->pos < size || src->total_size - src->pos < size) {
        release(&src->pktblk_lock);
        release(&dest->pktblk_lock);
        return -E_NET_ERROR_SIZE;
    }
    while(size){
        size_t dest_remain = cur_blk_remain_nolock(dest);
        size_t src_remian = cur_blk_remain_nolock(src);
        size_t copy_size = dest_remain > src_remian ? src_remian : dest_remain;
        copy_size = copy_size > size ? size : copy_size;
        memcpy(dest->blk_offset, src->blk_offset, copy_size);
        move_forward_nolock(dest, copy_size);
        move_forward_nolock(src, copy_size);
        size -= copy_size;
    }
    release(&src->pktblk_lock);
    release(&dest->pktblk_lock);
    return NET_OK
}

int pktbuf_fill(Pktbuf *buf, uint8_t v, int size){
    if(!size){
        return -E_NET_ERROR_PARAM;
    }
    acquire(&buf->pktblk_lock);
    if (buf->total_size - buf->pos < size) {
        release(&buf->pktblk_lock);
        return -E_NET_ERROR_SIZE;
    }
    while(size){
          size_t blk_size = cur_blk_remain_nolock(buf);
          size_t cur_fill_size = size > blk_size ? blk_size : size;
          memset(buf->blk_offset, v, cur_fill_size);
          move_forward_nolock(buf, cur_fill_size);
          size -= cur_fill_size;
    }
    release(&buf->pktblk_lock);
    return NET_OK;
}