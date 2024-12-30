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
#include "pktbuf.h"
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
        virtio_net_tx(buf, sizeof(buf), name);
        // assert(ret == sizeof(buf));
        // cprintf("ret: %d\n", ret);
    }
    cprintf("virtio_mmio_net_test done\n");
}

// void pktbuf_test(void) {
//     Pktbuf *buf = pktbuf_alloc(2000);
//     pktbuf_free(buf);
//     buf = pktbuf_alloc(2000);
//     for (int i = 0; i < 16; i++) {
//         pktbuf_add_header(buf, 66, true);  
//     }
//     for (int i = 0; i < 10; i++) {
//         pktbuf_remove_header(buf, 88); 
//     }
//     for (int i = 0; i < 16; i++) {
//         pktbuf_add_header(buf, 66, false);  
//     }
//     for (int i = 0; i < 10; i++) {
//         pktbuf_remove_header(buf, 88); 
//     }
//     pktbuf_free(buf);

//     buf = pktbuf_alloc(0);  
//     pktbuf_resize(buf, 32);
//     pktbuf_resize(buf, 527);
//     pktbuf_resize(buf, 4922);
//     pktbuf_resize(buf, 1921);
//     pktbuf_resize(buf, 288);
//     pktbuf_resize(buf, 32);
//     pktbuf_resize(buf, 0);
//     pktbuf_free(buf);

//     buf = pktbuf_alloc(689);
//     Pktbuf *sbuf = pktbuf_alloc(892);
//     pktbuf_join(buf, sbuf);
//     pktbuf_free(buf);

//     buf = pktbuf_alloc(32);
//     pktbuf_join(buf, pktbuf_alloc(4));
//     pktbuf_join(buf, pktbuf_alloc(16));
//     pktbuf_join(buf, pktbuf_alloc(54));
//     pktbuf_join(buf, pktbuf_alloc(32));
//     pktbuf_join(buf, pktbuf_alloc(38));
//     pktbuf_set_cont(buf, 44);  
//     pktbuf_set_cont(buf, 60);  
//     pktbuf_set_cont(buf, 64);   
//     pktbuf_set_cont(buf, 128);
//     pktbuf_set_cont(buf, 176);
//     // pktbuf_set_cont(buf, 513);  
//     pktbuf_free(buf);

//     buf = pktbuf_alloc(1024);
//     pktbuf_join(buf, pktbuf_alloc(4));
//     pktbuf_join(buf, pktbuf_alloc(16));
//     pktbuf_join(buf, pktbuf_alloc(54));
//     pktbuf_join(buf, pktbuf_alloc(32));
//     pktbuf_join(buf, pktbuf_alloc(38));
//     pktbuf_join(buf, pktbuf_alloc(512));
//     pktbuf_reset_acc(buf);
//     static uint16_t temp[1000];
//     for (int i = 0; i < 1000; i++) { temp[i] = i; }
//     pktbuf_write(buf, (uint8_t *)temp, buf->total_size);
//     static uint16_t read_temp[1000];
//     memset(read_temp, 0, sizeof(read_temp));
//     pktbuf_reset_acc(buf);
//     pktbuf_read(buf, (uint8_t *)read_temp, buf->total_size);
//     assert(memcmp(temp, read_temp, buf->total_size) == 0);

//     memset(read_temp, 0, sizeof(read_temp));
//     pktbuf_seek(buf, 85 * 2);
//     pktbuf_read(buf, (uint8_t *)read_temp, 512);
//     assert(memcmp(temp + 85, read_temp, 512) == 0);

//     Pktbuf *dest = pktbuf_alloc(1024);
//     pktbuf_seek(buf, 200);      
//     pktbuf_seek(dest, 300);       
//     pktbuf_copy(dest, buf, 522); 

//     memset(read_temp, 0, sizeof(read_temp));
//     pktbuf_seek(dest, 300);
//     pktbuf_read(dest, (uint8_t *)read_temp, 522);        
//     assert(memcmp(temp + 100, read_temp, 512) == 0);

//     pktbuf_seek(dest, 0);
//     pktbuf_fill(dest, 53, dest->total_size);
//     memset(read_temp, 0, sizeof(read_temp));
//     pktbuf_seek(dest, 0);
//     pktbuf_read(dest, (uint8_t *)read_temp, dest->total_size);
//     for (int i = 0; i < dest->total_size; i++) {
//         assert(((uint8_t *)read_temp)[i] == 53);
//     }

//     pktbuf_free(dest);
//     pktbuf_free(buf); 
// }

// void net_basic_test(void){
//     pktbuf_test();
// }

void kernel_test(){
#ifdef PRINT_VIRTIO_DEVICE_TEST
    virtio_mmio_rng_test();
    rand_test();
    // virtio_mmio_blk_test("swap.img");
    // virtio_mmio_net_test("net1");
    // while(1);
    // do_execve("user.elf", nullptr);
#endif
// #ifdef PRINT_NET_TEST
//     net_basic_test();
// #endif
}

void daemon_proc(void *arg) {
    Proc *p = myproc();
    kernel_test();
    // virtio_net_close("net0");
    // virtio_net_close("net1");
    while (1) {
        // cprintf("%s running %s ... all test passed\n", p->name, (const char *)arg);
        kswap_main();
        mbox_cleanup();
        updata_cpu_rq_load();
        load_balance(3);
        clean_kernel_proc();
        exec_timer_func();
        do_sleep(10);
        // task_delay(DELAY);
    }
}