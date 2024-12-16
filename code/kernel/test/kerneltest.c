#include "kerneltest.h"

#include "assert.h"
#include "atomic.h"
#include "config.h"
#include "error.h"
#include "hash.h"
#include "mbox.h"
#include "memlayout.h"
#include "pmm.h"
#include "proc.h"
#include "rand.h"
#include "sched.h"
#include "slab.h"
#include "stdio.h"
#include "string.h"
#include "swap.h"
#include "swapfs.h"
#include "virtio-blk.h"
#include "virtio-mmio.h"
#include "virtio-rng.h"
#include "virtio.h"
#include "virtio_device.h"
#include "vmm.h"
#include "virtio-net.h"
// #include "swapfs.h"

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
#define TEST_CNT (128 * 1024 * 1024 / DATA_LEN)  // 64MB
// #define TEST_CNT 10
static void virtio_mmio_blk_test(char *name) {
    int dlen = DATA_LEN;
    struct blk_buf req[1] = {0};

    cprintf("blk test...");
    for (int n = 0; n < TEST_CNT; ++n) {
        for (int i = 0; i < dlen / 4; ++i) { wdata[i] = simulate_rand() & INT_MASK; }
        req[0].addr = n * dlen;
        req[0].data = wdata;
        req[0].data_len = dlen;
        req[0].is_write = 1;
        virtio_blk_rw(&req[0], name);
        // virtio_blk_rw_syn(&req[0], name);
        for (int j = 0; j < dlen / 4; ++j) { rdata[j] = 0; }
        req[0].addr = n * dlen;
        req[0].data = rdata;
        req[0].data_len = dlen;
        req[0].is_write = 0;
        virtio_blk_rw(&req[0], name);
        // virtio_blk_rw_syn(&req[0], name);
        for (int j = 0; j < dlen / 4; ++j) {
            if (rdata[j] != wdata[j]) {
                cprintf("blk write or read failed\n");
                goto L1;
            }
        }
    }
    cprintf("%s test passed!\n", name);
    return;
L1:
    panic("failed!\n");
}

void virtio_mmio_net_test(char *name) {
    uint ret;
    uchar buf[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x12, 0x34, 0x56, 0x78, 0x90, 0xab, 0x08, 0x06,
                0x00, 0x01, 0x08, 0x00, 0x06, 0x04, 0x00, 0x01, 0x12, 0x34, 0x56, 0x78, 0x90, 0xab,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xa8, 0x01, 0x64};
    cprintf("buf: %p\n", buf);
    for (int i = 0; i < 200; ++i) {
        ret = virtio_net_tx(buf, sizeof(buf), name);
        // assert(ret == sizeof(buf));
        cprintf("ret: %d\n", ret);
    }
    cprintf("done\n");
}

void kernel_test(){
#ifdef PRINT_VIRTIO_DEVICE_TEST
    virtio_mmio_rng_test();
    rand_test();
    virtio_mmio_blk_test("swap.img");
    virtio_mmio_net_test("net1");
    // while(1);
    // do_execve("user.elf", nullptr);
#endif
}

void daemon_proc(void *arg) {
    Proc *p = myproc();
    kernel_test();
    virtio_net_close("net0");
    virtio_net_close("net1");
    while (1) {
        // cprintf("%s running %s ... all test passed\n", p->name, (const char *)arg);
        kswap_main();
        mbox_cleanup();
        updata_cpu_rq_load();
        load_balance(3);
        clean_kernel_proc();
        do_sleep(10);
        // task_delay(DELAY);
    }
}