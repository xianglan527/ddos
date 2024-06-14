#include "riscv.h"
#include "virtio-ring.h"
#include "stdio.h"

/* Parts of the virtqueue are aligned on a 4096 byte page boundary */
#define VQ_ALIGN(addr)	((((uint64_t)addr) + 0xfff) & ~0xfff)


/**
 * Calculate ring size according to queue size number
 */
uint32_t virtio_vring_size(uint32_t qsize)
{
	uint32_t size;

	size = VQ_ALIGN(qsize * sizeof(struct vring_desc)); // 4k aligned
	size += VQ_ALIGN(sizeof(struct vring_avail) + (qsize * sizeof(uint16_t))); // 4k aligned
	size += VQ_ALIGN(sizeof(struct vring_used)
						+ (qsize * sizeof(struct vring_used_elem))); // 4k aligned
	return size;
}

int virtio_vring_init(struct vring *vr, uint8_t *buf, uint32_t buf_len, uint32_t qsize)
{
    uint32_t vr_size = virtio_vring_size(qsize);
    if (vr_size > buf_len) { // buf is small
        return -1;
    }
	vr->size = qsize;
	vr->desc = (struct vring_desc *)VQ_ALIGN(buf);
	vr->avail = (struct vring_avail *)VQ_ALIGN(buf + qsize * sizeof(struct vring_desc));
	vr->used = (struct vring_used *)VQ_ALIGN(&vr->avail->ring[qsize]);

#ifdef PRINT_VIRTIO_DEVICE_INFO
    cprintf("buf: %p, len: %d, qsize: %d\n", buf, buf_len, qsize);
    cprintf("vr->desc: %p\n", vr->desc);
    cprintf("vr->avail: %p\n", vr->avail);
    cprintf("vr->used: %p\n", vr->used);
#endif
    uint8_t *pt = (uint8_t *)(&vr->used->ring[qsize].len) + 4;
    if (pt > (buf + buf_len)) { // buf overflow
        return -2;
    }

    return 0;
}

void virtio_vring_fill_desc(struct vring_desc *desc, uint64_t addr, uint32_t len, uint16_t flags, uint16_t next)
{
	volatile struct vring_desc *pt = (volatile struct vring_desc *)desc;
	pt->addr = addr;
	pt->len = len;
	pt->flags = flags;
	pt->next = next;
	dsb();
}

void virtio_vring_add_avail(struct vring_avail *avail, uint16_t idx, uint32_t qsize)
{
	volatile struct vring_avail *pt = (volatile struct vring_avail *)avail;
	pt->ring[pt->idx % qsize] = idx;
    dsb();
    // tell the device another avail ring entry is available.
    pt->idx += 1; // not % NUM ...
	dsb();
}
