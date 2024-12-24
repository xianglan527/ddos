#ifndef __VIRTIO_VIRTIO_RING_H_
#define __VIRTIO_VIRTIO_RING_H_

#include "types.h"

/* Definitions for vring_desc.flags */
#define VRING_DESC_F_NEXT	    1	/* buffer continues via the next field */
#define VRING_DESC_F_WRITE	    2	/* buffer is write-only (otherwise read-only) */
#define VRING_DESC_F_INDIRECT	4	/* buffer contains a list of buffer descriptors */
/* Descriptor table entry - see Virtio Spec chapter 2.3.2 */
struct vring_desc {
	uint64_t addr;		/* Address (guest-physical) */
	uint32_t len;		/* Length */
	uint16_t flags;		/* The flags as indicated above */
	uint16_t next;		/* Next field if flags & NEXT */
};

/* Definitions for vring_avail.flags */
#define VRING_AVAIL_F_NO_INTERRUPT	1
/* Available ring - see Virtio Spec chapter 2.3.4 */
struct vring_avail {
	uint16_t flags;
	uint16_t idx;
	uint16_t ring[];
};

/* Definitions for vring_used.flags */
#define VRING_USED_F_NO_NOTIFY		1
struct vring_used_elem {
	uint32_t id;		    /* Index of start of used descriptor chain */
	uint32_t len;		/* Total length of the descriptor chain which was used */
};
struct vring_used {
	uint16_t flags;
    volatile uint16_t idx;
    struct vring_used_elem ring[];
};

struct vring {
	uint32_t                 size;   // power of 2
	struct vring_desc   *desc;
	struct vring_avail  *avail;
	struct vring_used   *used;
};

/*
 * We publish the used event index at the end of the available ring, and vice
 * versa. They are at the end for backwards compatibility.
 */
#define vring_used_event(vr)  ((vr)->avail->ring[(vr)->num])
#define vring_avail_event(vr) (*(uint16_t *)&(vr)->used->ring[(vr)->num])

uint32_t virtio_vring_size(uint32_t qsize);
int virtio_vring_init(struct vring *vr, uint8_t *buf, uint32_t buf_len, uint32_t qsize);
void virtio_vring_fill_desc(struct vring_desc *desc, uint64_t addr, uint32_t len, uint16_t flags, uint16_t next);
void virtio_vring_add_avail(struct vring_avail *avail, uint16_t idx, uint32_t qsize);

#endif /* VIRTIO_RING_H_ */
