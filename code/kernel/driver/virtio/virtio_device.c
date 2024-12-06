#include "virtio_device.h"

#include "assert.h"
#include "config.h"
#include "memlayout.h"
#include "rand.h"
#include "stdio.h"
#include "virtio-blk.h"
#include "virtio-mmio.h"
#include "virtio-net.h"
#include "virtio-rng.h"
#include "virtio.h"

Device_type device_type_map[VIRTIO_DEVICE_NUM];

void virtio_device_init(void) {
    for (uintptr_t i = VIRTIO_START_ADDR; i <= VIRTIO_END_ADDR; i += VIRTIO_STEP_SIZE) {
        int ret = -1;
        uintptr_t ptr = i;
        uint32_t device_id = *(volatile uint32_t *)(i + VIRTIO_MMIO_DEVICE_ID);
        int idx = (i - VIRTIO_START_ADDR) / VIRTIO_STEP_SIZE + 1;
        switch (device_id) {
            case 0: {
                cprintf("Device not connected\n");
                break;
                case 1: {
                    cprintf("Network device\n");
                    device_type_map[idx] = DEVICE_NETWORK;
                    assert(virtio_net_init(ptr, idx) == 0);
                } break;
                case 2: {
                    cprintf("Block device\n");
                    device_type_map[idx] = DEVICE_BLOCK;
                    assert(virtio_blk_init(ptr, idx) == 0);
                } break;
                case 3: {
                    cprintf("Console device\n");
                } break;
                case 4: {
                    cprintf("Entropy device\n");
                    device_type_map[idx] = DEVICE_ENTROPY;
                    assert(virtio_rng_init(ptr, idx) == 0);
                } break;
                default: panic("Unrecognized device"); break;
            }
        }
    }
}

void virtio_device_intr_handler(int irq) {
    switch (device_type_map[irq]) {
        case DEVICE_BLOCK: {
            virtio_blk_intr(irq);
        } break;
        case DEVICE_ENTROPY: {
            virtio_rng_intr();
        } break;
        case DEVICE_NETWORK: {
            virtio_net_intr(irq);
        } break;
        default: panic("virtio_device_intr_handler"); break;
    }
}

int get_device_status_ok(int idx) { return virtio_mmio_get_status(idx) | VIRTIO_STAT_DRIVER_OK; }