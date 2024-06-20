#include "virtio-blk.h"

#include "assert.h"
#include "config.h"
#include "memlayout.h"
#include "plic.h"
#include "printf.h"
#include "riscv.h"
#include "slab.h"
#include "stdio.h"
#include "string.h"
#include "virtio-mmio.h"
#include "virtio-ring.h"
#include "virtio.h"

List_entry blks_list;
#define le2blk(le, member) to_struct((le), struct virtio_blk, member)
static int blk_device_num = 0;

int virtio_blk_init(uint32_t base, int idx) {
    int ret = -1;
    if (blk_device_num == 0) {
        list_init(&blks_list);
        ret = virtio_blk_add(base, "swap.img", idx);
        blk_device_num++;
        return ret;
    }
    else if (blk_device_num == 1) {
        ret = virtio_blk_add(base, "fs.img", idx);
        blk_device_num++;
        return ret;
    }
    panic("There are too many block devices");
    return ret;
}

int virtio_blk_add(uint32_t base, char *name,int idx) {
    assert(base == (VIRTIO_START_ADDR + ((idx) - 1) * VIRTIO_STEP_SIZE));
#ifdef PRINT_VIRTIO_DEVICE_INFO
    virtio_mmio_device_info(idx);
#endif
    if (virtio_mmio_read_reg(VIRTIO_MMIO_MAGIC_VALUE, idx) != 0x74726976 ||
        virtio_mmio_read_reg(VIRTIO_MMIO_VERSION, idx) != 2 || virtio_mmio_read_reg(VIRTIO_MMIO_DEVICE_ID, idx) != 2 ||
        virtio_mmio_read_reg(VIRTIO_MMIO_VENDOR_ID, idx) != 0x554d4551) {
        cprintf("could not find virtio blk\n");
        return -1;
    }

    // reset device
    virtio_mmio_reset_device(idx);

    uint32_t status = 0;
    // set ACKNOWLEDGE status bit
    status |= VIRTIO_STAT_ACKNOWLEDGE;
    virtio_mmio_set_status(status, idx);

    // set DRIVER status bit
    status |= VIRTIO_STAT_DRIVER;
    virtio_mmio_set_status(status, idx);

    // negotiate features
    uint64_t features = virtio_mmio_get_host_features(idx);
    cprintf("device features: 0x%016llx\n", features);
    features &= ~(1 << VIRTIO_BLK_F_RO);
    features &= ~(1 << VIRTIO_BLK_F_SCSI);
    features &= ~(1 << VIRTIO_BLK_F_FLUSH);
    features &= ~(1 << VIRTIO_BLK_F_CONFIG_WCE);
    features &= ~(1 << VIRTIO_BLK_F_MQ);
    features &= ~(1 << VIRTIO_F_ANY_LAYOUT);
    features &= ~(1 << VIRTIO_F_EVENT_IDX);
    features &= ~(1 << VIRTIO_F_INDIRECT_DESC);
    cprintf("after set driver features: 0x%016llx\n", features);
    virtio_mmio_set_guest_features(features, idx);

    // tell device that feature negotiation is complete.
    status |= VIRTIO_STAT_FEATURES_OK;
    virtio_mmio_set_status(status, idx);

    // re-read status to ensure FEATURES_OK is set.
    status = virtio_mmio_get_status(idx);
    if (!(status & VIRTIO_STAT_FEATURES_OK)) {
        cprintf("virtio disk FEATURES_OK unset");
        return -2;
    }

    // initialize queue 0.
    int qnum = 0;
    int qsize = BLK_QSIZE;
    // ensure queue 0 is not in use.
    if (virtio_mmio_get_queue_ready(qnum, idx)) {
        cprintf("virtio disk should not be ready");
        return -3;
    }

    // check maximum queue size.
    uint32_t max = virtio_mmio_get_queue_size(qnum, idx);
    if (max == 0) {
        cprintf("virtio disk has no queue 0");
        return -4;
    }
    if (max < qsize) {
        cprintf("virtio disk max queue too short");
        return -5;
    }
    cprintf("queue_0 max size: %d\n", max);
    struct virtio_blk *blk = kmalloc(sizeof(struct virtio_blk));

    blk->qsize = max;
    uint8_t *blk_buf = aligned_kmalloc(3 * PGSIZE, PGSIZE);
    memset(blk_buf, 0, 3 * PGSIZE);
    int r = virtio_vring_init(&blk->vr, blk_buf, 3 * PGSIZE, qsize);
    if (r) {
        cprintf("virtio_vring_init failed: %d\n", r);
        return r;
    }

    // set queue size.
    virtio_mmio_set_queue_size(qnum, qsize, idx);
    // write physical addresses.
    virtio_mmio_set_queue_addr(qnum, &blk->vr, idx);
    // queue is ready.
    virtio_mmio_set_queue_ready(qnum, idx);

    // tell device we're completely ready.
    status |= VIRTIO_STAT_DRIVER_OK;
    virtio_mmio_set_status(status, idx);
    dsb();

    cprintf("block device status:0x%02x\n", virtio_mmio_get_status(idx));
    blk->idx = idx;
    virtio_blk_cfg(blk);
    snprintf(blk->blk_name, sizeof(blk->blk_name), "%s", name);
    initlock(&blk->blk_lock, name);
    list_add(&blks_list, &blk->blk_list);
    return 0;
}

void virtio_blk_cfg(struct virtio_blk *blk) {
    uint32_t pt[(sizeof(struct virtio_blk_cfg) + 3) / 4] = {0};
    struct virtio_blk_cfg *cfg = (struct virtio_blk_cfg *)pt;

    for (int i = 0; i < sizeof(cfg) / 4; ++i) { pt[i] = virtio_mmio_read_reg(VIRTIO_MMIO_CONFIG + 4 * i, blk->idx); }
    blk->capacity = cfg->capacity;
#ifdef PRINT_VIRTIO_DEVICE_INFO
    cprintf("capacity: %lld\n", cfg->capacity);
    cprintf("size_max: %d\n", cfg->size_max);
    cprintf("seg_max: %d\n", cfg->seg_max);
    cprintf("geometry.cylinders: %d\n", cfg->geometry.cylinders);
    cprintf("geometry.heads: %d\n", cfg->geometry.heads);
    cprintf("geometry.sectors: %d\n", cfg->geometry.sectors);
    cprintf("blk_size: %d\n", cfg->blk_size);
#endif
}

struct virtio_blk *find_blk_by_name(char *name) {
    List_entry *list, *le;
    list = le = &blks_list;
    while ((le = list_next(le)) != list) {
        struct virtio_blk *blk = le2blk(le, blk_list);
        if (strcmp(blk->blk_name, name) == 0) { return blk; }
    }
    return nullptr;  
}

struct virtio_blk *find_blk_by_index(int idx) {
    List_entry *list, *le;
    list = le = &blks_list;
    while ((le = list_next(le)) != list) {
        struct virtio_blk *blk = le2blk(le, blk_list);
        if (blk->idx == idx) { return blk; }
    }
    return nullptr;  
}

void virtio_blk_rw(struct blk_buf *b, char *blk_name) {
    int index[3];
    int qnum = 0;
    uint64_t sector = b->addr / SECTOR_SZIE;
    struct virtio_blk *blk = find_blk_by_name(blk_name);
    // acquire(&blk->blk_lock);
    if (blk == nullptr) panic("the %s does not exist\n", blk_name);
    int idx = blk->idx;
    uint64_t sector_end = (b->addr + b->data_len) / SECTOR_SZIE;

    if (sector_end > blk->capacity) {
        cprintf("virtio_blk_rw: invalid data length!\n");
        return;
    }

    for (int i = 0; i < 3; ++i) { index[i] = blk->avail_idx++ % BLK_QSIZE; }

    struct virtio_blk_req *req = &blk->ops[index[0]];
    req->type = b->is_write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
    req->reserved = 0;
    req->sector = sector;

    // fill descriptor: blk request header
    virtio_vring_fill_desc(blk->vr.desc + index[0], (uintptr_t)req, sizeof(struct virtio_blk_req),
                           VRING_DESC_F_NEXT, index[1]);

    // fill descriptor: blk data
    virtio_vring_fill_desc(blk->vr.desc + index[1], (uintptr_t)b->data, b->data_len,
                           (b->is_write ? 0 : VRING_DESC_F_WRITE) | VRING_DESC_F_NEXT, index[2]);

    // fill descriptor: blk request status
    blk->status[index[0]] = 0xff;  // device writes 0 on success
    virtio_vring_fill_desc(blk->vr.desc + index[2], (uintptr_t)&blk->status[index[0]], 1, VRING_DESC_F_WRITE,
                           0);

    // set blk flag
    b->flag = 1;
    blk->info[index[0]] = b;

    virtio_vring_add_avail(blk->vr.avail, index[0], BLK_QSIZE);
    virtio_mmio_set_notify(qnum, idx);


    volatile uint16_t *pflag = &b->flag;
    uint64_t pp = intr_get();
    intr_on();
    while (*pflag == 1);
    blk->info[index[0]] = NULL;
    // acquire(&blk->blk_lock);
}

void virtio_blk_intr(int idx) {
    struct virtio_blk *blk = find_blk_by_index(idx);
    assert(blk != nullptr);
    virtio_mmio_set_ack(idx);

    // the device increments disk.used->idx when it
    // adds an entry to the used ring.
    dsb();
    while (blk->used_idx != blk->vr.used->idx) {
        int id = blk->vr.used->ring[blk->used_idx % BLK_QSIZE].id;
        if (blk->status[id] != 0) { cprintf("virtio_blk_intr status: %d\n", blk->status[id]); }

        struct blk_buf *b = blk->info[id];
        b->flag = 0;  // blk is done
        blk->used_idx += 1;
        dsb();
        // cprintf("virtio_blk_intr b->flag: %d\n", b->flag);
    }
}