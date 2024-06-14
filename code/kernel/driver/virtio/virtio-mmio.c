#include "riscv.h"
#include "virtio.h"
#include "virtio-mmio.h"
#include "stdio.h"
#include "config.h"
#include "memlayout.h"


#define VTMO_ADDR(addr, idx) (VIRTIO_START_ADDR + ((idx) - 1) * VIRTIO_STEP_SIZE + (addr))
#define VTMO_REG(reg, idx)       (*(volatile uint32_t *)(uintptr_t)VTMO_ADDR(reg, idx))
#define VTMO_REG8(reg, idx)       (*(volatile uint8_t *)(uintptr_t)VTMO_ADDR(reg, idx))

void virtio_mmio_device_info(int idx)
{
    cprintf("0x%08x\n", virtio_mmio_read_reg(VIRTIO_MMIO_MAGIC_VALUE, idx));
    cprintf("0x%08x\n", virtio_mmio_read_reg(VIRTIO_MMIO_VERSION, idx));
    cprintf("0x%08x\n", virtio_mmio_read_reg(VIRTIO_MMIO_DEVICE_ID, idx));
    cprintf("0x%08x\n", virtio_mmio_read_reg(VIRTIO_MMIO_VENDOR_ID, idx));
}

uint32_t virtio_mmio_read_reg(uint32_t addr, int idx)
{
    return VTMO_REG(addr, idx);
}

uint8_t virtio_mmio_read_reg8(uint32_t addr, int idx)
{
    return VTMO_REG8(addr, idx);
}

uint32_t virtio_mmio_get_status(int idx)
{
    return VTMO_REG(VIRTIO_MMIO_STATUS, idx);
}

void virtio_mmio_set_status(uint32_t status, int idx)
{
    VTMO_REG(VIRTIO_MMIO_STATUS, idx) = status;
}

void virtio_mmio_reset_device(int idx)
{
	virtio_mmio_set_status(0, idx);
}

uint64_t virtio_mmio_get_host_features(int idx)
{
    // Read the full 64-bit device features field
	uint64_t features = 0;
	VTMO_REG(VIRTIO_MMIO_HOST_FEATURES_SEL, idx) = 0;
	dsb();
	features = VTMO_REG(VIRTIO_MMIO_HOST_FEATURES, idx);

	VTMO_REG(VIRTIO_MMIO_HOST_FEATURES_SEL, idx) = 1;
	dsb();
	features |= ((uint64_t)VTMO_REG(VIRTIO_MMIO_HOST_FEATURES, idx) << 32);

	return features;
}

void virtio_mmio_set_guest_features(uint64_t features, int idx)
{
    uint32_t f0 = features & 0xFFFFFFFF;
    uint32_t f1 = (features >> 32) & 0xFFFFFFFF;

    VTMO_REG(VIRTIO_MMIO_HOST_FEATURES_SEL, idx) = 0;
	dsb();
    VTMO_REG(VIRTIO_MMIO_GUEST_FEATURES, idx) = f0;

    VTMO_REG(VIRTIO_MMIO_HOST_FEATURES_SEL, idx) = 1;
	dsb();
    VTMO_REG(VIRTIO_MMIO_GUEST_FEATURES, idx) = f1;
}

int virtio_mmio_get_queue_ready(int qnum, int idx)
{
    VTMO_REG(VIRTIO_MMIO_QUEUE_SEL, idx) = qnum;
	dsb();

    return VTMO_REG(VIRTIO_MMIO_QUEUE_READY, idx);
}

void virtio_mmio_set_queue_ready(int qnum, int idx)
{
    VTMO_REG(VIRTIO_MMIO_QUEUE_SEL, idx) = qnum;
	dsb();

    VTMO_REG(VIRTIO_MMIO_QUEUE_READY, idx) = 1;
}

int virtio_mmio_get_queue_size(int qnum, int idx)
{
    VTMO_REG(VIRTIO_MMIO_QUEUE_SEL, idx) = qnum;
	dsb();
    return VTMO_REG(VIRTIO_MMIO_QUEUE_NUM_MAX, idx);

    // uint64_t features = virtio_mmio_get_host_features();
    // if (features & VIRTIO_F_VERSION_1) {
    //     return VTMO_REG(VIRTIO_MMIO_QUEUE_NUM);
    // } else {
    //     return VTMO_REG(VIRTIO_MMIO_QUEUE_NUM_MAX);
    // }
}

void virtio_mmio_set_queue_size(int qnum, uint32_t qsize, int idx)
{
    VTMO_REG(VIRTIO_MMIO_QUEUE_SEL, idx) = qnum;
	dsb();

    VTMO_REG(VIRTIO_MMIO_QUEUE_NUM, idx) = qsize;
}

void virtio_mmio_set_queue_addr(int qnum, struct vring *vr, int idx)
{
    VTMO_REG(VIRTIO_MMIO_QUEUE_SEL, idx) = qnum;
	dsb();

    VTMO_REG(VIRTIO_MMIO_QUEUE_DESC_LOW, idx) = ((uint64_t)vr->desc) & INT_MASK;
    VTMO_REG(VIRTIO_MMIO_QUEUE_DESC_HIGH, idx) = 0;
    //VTMO_REG(VIRTIO_MMIO_QUEUE_DESC_HIGH) = (uint64_t)vr->desc >> 32;
    VTMO_REG(VIRTIO_MMIO_QUEUE_AVAIL_LOW, idx) = ((uint64_t)vr->avail) & INT_MASK;
    VTMO_REG(VIRTIO_MMIO_QUEUE_AVAIL_HIGH, idx) = 0;
    //VTMO_REG(VIRTIO_MMIO_QUEUE_AVAIL_HIGH) = (uint64_t)vr->avail >> 32;
    VTMO_REG(VIRTIO_MMIO_QUEUE_USED_LOW, idx) = ((uint64_t)vr->used) & INT_MASK;
    VTMO_REG(VIRTIO_MMIO_QUEUE_USED_HIGH, idx) = 0;
    //VTMO_REG(VIRTIO_MMIO_QUEUE_USED_HIGH) = (uint64_t)vr->used >> 32;
}

void virtio_mmio_set_notify(int qnum, int idx)
{
    VTMO_REG(VIRTIO_MMIO_QUEUE_NOTIFY, idx) = qnum;
	dsb();
}

void virtio_mmio_set_ack(int idx)
{
    uint32_t status = VTMO_REG(VIRTIO_MMIO_INTERRUPT_STATUS, idx);
    VTMO_REG(VIRTIO_MMIO_INTERRUPT_ACK, idx) = status & 0x3;
	dsb();
}