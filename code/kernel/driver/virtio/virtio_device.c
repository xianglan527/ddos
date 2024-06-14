#include "virtio-blk.h"
#include "virtio-rng.h"
#include "stdio.h"
#include "memlayout.h"
#include "assert.h"
#include "rand.h"
#include "virtio-mmio.h"
#include "config.h"
#include "rand.h"
#include "virtio_device.h"

static void virtio_mmio_rng_test(void) {
    uint32_t buf[4] = {0};
    for (int n = 0; n < 8; ++n) {
        cprintf("==== %d ====\n", n);
        int rlen = virtio_rng_read((uint8_t *)buf, sizeof(buf));

        for (int i = 0; i < sizeof(buf) / sizeof(buf[0]); i += 4) {
            cprintf("0x%08x 0x%08x 0x%08x 0x%08x\n", buf[i], buf[i + 1], buf[i + 2], buf[i + 3]);
        }

        for (int i = 0; i < sizeof(buf) / sizeof(buf[0]); ++i) { buf[i] = 0; }
    }
    cprintf("virtio-rng test passed!\n");
}

#define BLK_1K (2 * SECTOR_SZIE)
#define DATA_LEN (64 * BLK_1K)  // 64KB
uint32_t wdata[DATA_LEN / 4] = {0};
uint32_t rdata[DATA_LEN / 4] = {0};
#define TEST_CNT (64 * 1024 * 1024 / DATA_LEN)  // 64MB
static void virtio_mmio_blk_test(char *name) {
    int dlen = DATA_LEN;
    struct blk_buf req[1] = {0};

    for (int i = 0; i < dlen / 4; ++i) { wdata[i] = rand() & INT_MASK; }

    cprintf("blk test...");
    for (int n = 0; n < TEST_CNT; ++n) {
        req[0].addr = n * dlen;
        req[0].data = wdata;
        req[0].data_len = dlen;
        req[0].is_write = 1;
        virtio_blk_rw(&req[0],name);

        for (int j = 0; j < dlen / 4; ++j) { rdata[j] = 0; }

        req[0].addr = n * dlen;
        req[0].data = rdata;
        req[0].data_len = dlen;
        req[0].is_write = 0;
        virtio_blk_rw(&req[0], name);
        for (int j = 0; j < dlen / 4; ++j) {
            if (rdata[j] != wdata[j]) {
                cprintf("blk write or read failed\n");
                goto L1;
            }
        }
    }
    cprintf("%s test passed!\n",name);
    return;
L1:
    panic("failed!\n");
}



Device_type device_type_map[VIRTIO_DEVICE_NUM];

void virtio_device_init(void){
    for(uintptr_t i = VIRTIO_START_ADDR; i <= VIRTIO_END_ADDR; i += VIRTIO_STEP_SIZE){
        int ret = -1;
        uintptr_t ptr = i;
        uint32_t device_id = *(volatile uint32_t *)(i + VIRTIO_MMIO_DEVICE_ID);
        int idx = (i - VIRTIO_START_ADDR) / VIRTIO_STEP_SIZE + 1;
        switch (device_id)
        {
            case 0: {cprintf("Device not connected\n");} break;
            case 1: {cprintf("Network device\n");} break;
            case 2: {
                    cprintf("Block device\n");
                    device_type_map[idx] = DEVICE_BLOCK;
                    assert(virtio_blk_init(ptr, idx) == 0);
                }break;
                case 3: {
                    cprintf("Console device\n");
                } break;
                case 4:{
                    cprintf("Entropy device\n");
                    device_type_map[idx] = DEVICE_ENTROPY;
                    assert(virtio_rng_init(ptr, idx) == 0);
                }break;
                default: panic("Unrecognized device"); break;
        }
    }
#ifdef PRINT_VIRTIO_DEVICE_TEST
    virtio_mmio_rng_test();
    rand_test();
    virtio_mmio_blk_test("swap.img");
    virtio_mmio_blk_test("fs.img");
#endif
}


void virtio_device_intr_handler(int irq){
    switch (device_type_map[irq])
    {
    case DEVICE_BLOCK:{
        virtio_blk_intr(irq);
    }break;
    case DEVICE_ENTROPY:{
        virtio_rng_intr();
    }break;
    default: panic("virtio_device_intr_handler"); break;
    }
}