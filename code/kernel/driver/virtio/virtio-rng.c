#include "riscv.h"
#include "virtio.h"
#include "virtio-ring.h"
#include "virtio-mmio.h"
#include "virtio-rng.h"
#include "stdio.h"
#include "proc.h"

static uint8_t gs_blk_buf[3*4096] __attribute__((aligned(4096))) = { 0 };
static struct virtio_rng gs_virtio_rng = { 0 };

int virtio_rng_init(uint32_t base, int idx)
{
    assert(base == (VIRTIO_START_ADDR + ((idx)-1) * VIRTIO_STEP_SIZE));
#ifdef PRINT_VIRTIO_DEVICE_INFO
    virtio_mmio_device_info(idx);
#endif

    if (virtio_mmio_read_reg(VIRTIO_MMIO_MAGIC_VALUE, idx) != 0x74726976 ||
        virtio_mmio_read_reg(VIRTIO_MMIO_VERSION, idx) != 2 ||
        virtio_mmio_read_reg(VIRTIO_MMIO_DEVICE_ID, idx) != 4 ||
        virtio_mmio_read_reg(VIRTIO_MMIO_VENDOR_ID, idx) != 0x554d4551) {
        cprintf("could not find virtio entropy\n");
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

    // get features
    uint64_t features = virtio_mmio_get_host_features(idx);
    cprintf("device features: 0x%016llx\n", features);
    // negotiate features
    features &= ~(1 << VIRTIO_F_EVENT_IDX);
    features &= ~(1 << VIRTIO_F_INDIRECT_DESC);
    cprintf("after set driver features: 0x%016llx\n", features);
    virtio_mmio_set_guest_features(features, idx);

    // tell device that feature negotiation is complete.
    status |= VIRTIO_STAT_FEATURES_OK;
    virtio_mmio_set_status(status, idx);

    // re-read status to ensure FEATURES_OK is set.
    status = virtio_mmio_get_status(idx);
    if(!(status & VIRTIO_STAT_FEATURES_OK)) {
        cprintf("virtio entropy FEATURES_OK unset");
        return -2;
    }

    // initialize queue 0.
    int qnum = 0;
    int qsize = RNG_QSIZE;
    // ensure queue 0 is not in use.
    if (virtio_mmio_get_queue_ready(qnum, idx)) {
        cprintf("virtio entropy should not be ready");
        return -3;
    }

    // check maximum queue size.
    uint32_t max = virtio_mmio_get_queue_size(qnum, idx);
    if(max == 0){
        cprintf("virtio entropy has no queue 0");
        return -4;
    }
    if(max < qsize){
        cprintf("virtio entropy max queue too short");
        return -5;
    }
    cprintf("queue_0 max size: %d\n", max);
    gs_virtio_rng.qsize = max;

    int r = virtio_vring_init(&gs_virtio_rng.vr, gs_blk_buf, sizeof(gs_blk_buf), qsize);
    if (r) {
        cprintf("virtio_vring_init failed: %d\n", r);
        return r;
    }

    // set queue size.
    virtio_mmio_set_queue_size(qnum, qsize, idx);
    // write physical addresses.
    virtio_mmio_set_queue_addr(qnum, &gs_virtio_rng.vr, idx);
    // queue is ready.
    virtio_mmio_set_queue_ready(qnum, idx);

    // tell device we're completely ready.
    status |= VIRTIO_STAT_DRIVER_OK;
    virtio_mmio_set_status(status, idx);
    dsb();

    cprintf("status:0x%02x\n", virtio_mmio_get_status(idx));
    gs_virtio_rng.idx = idx;
    return 0;
}

int virtio_rng_read(uint8_t *buf, int len)
{
    int qnum = 0;
    volatile struct virtio_rng *rng = &gs_virtio_rng;
    int avail_idx = rng->avail_idx++ % RNG_QSIZE;
    int idx = gs_virtio_rng.idx;
    virtio_vring_fill_desc(rng->vr.desc + avail_idx, (uint64_t)buf, len,
            VRING_DESC_F_WRITE, 0);

    rng->flag = 1;
    virtio_vring_add_avail(rng->vr.avail, avail_idx, RNG_QSIZE);
    virtio_mmio_set_notify(qnum, idx);
    dsb();

    while(rng->flag == 1)
        ;

    int rlen = rng->vr.used->ring[rng->used_idx % RNG_QSIZE].len;

    return rlen;
}

void virtio_rng_intr(void){
    volatile struct virtio_rng *rng = &gs_virtio_rng;
    int idx = gs_virtio_rng.idx;
    virtio_mmio_set_ack(idx);
    dsb();

    while (rng->used_idx != rng->vr.used->idx){
        rng->flag = 0;
        rng->used_idx += 1;
        dsb();
    } 
}
