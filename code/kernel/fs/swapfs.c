#include "fs.h"
#include "swapfs.h"
#include "assert.h"
#include "swap.h"
#include "virtio_device.h"
#include "virtio-blk.h"
#include "pmm.h"
void swapfs_init(void){
    struct virtio_blk *blk = find_blk_by_name("swap.img");
    assert(blk != nullptr);
    if(!get_device_status_ok(blk->idx))
        panic("swap.img isn't available.\n");
    static_assert((PGSIZE % SECTOR_SZIE) == 0);
    max_swap_offset = blk->capacity / PAGE_NSECT;
}

void swapfs_read(swap_entry_t entry, Page *page){
    struct blk_buf req;
    req.addr = swap_offset(entry) * PGSIZE;
    req.data = (void *)page2kva(page);
    req.data_len = PGSIZE;
    req.is_write = 0;
    virtio_blk_rw(&req, "swap.img");
}

void swapfs_write(swap_entry_t entry, Page *page) {
    struct blk_buf req;
    req.addr = swap_offset(entry) * PGSIZE;
    req.data = (void *)page2kva(page);
    req.data_len = PGSIZE;
    req.is_write = 1;
    virtio_blk_rw(&req, "swap.img");
}

void swapfs_read_syn(swap_entry_t entry, Page *page) {
    struct blk_buf req;
    req.addr = swap_offset(entry) * PGSIZE;
    req.data = (void *)page2kva(page);
    req.data_len = PGSIZE;
    req.is_write = 0;
    virtio_blk_rw_syn(&req, "swap.img");
}

void swapfs_write_syn(swap_entry_t entry, Page *page) {
    struct blk_buf req;
    req.addr = swap_offset(entry) * PGSIZE;
    req.data = (void *)page2kva(page);
    req.data_len = PGSIZE;
    req.is_write = 1;
    virtio_blk_rw_syn(&req, "swap.img");
}
