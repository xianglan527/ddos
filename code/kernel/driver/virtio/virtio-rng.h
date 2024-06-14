#ifndef __VIRTIO_VIRTIO_RNG_H_
#define __VIRTIO_VIRTIO_RNG_H_

#include "types.h"
#include "virtio-ring.h"

#define RNG_QSIZE       (128)   // rng queue0 size

struct virtio_rng {
    int idx;
    struct vring vr;
	uint32_t qsize;	// queue0 size
    uint16_t used_idx;
	uint16_t avail_idx;
    uint16_t flag;
};


int virtio_rng_init(uint32_t base, int idx);
int virtio_rng_read(uint8_t *buf, int len);
void virtio_rng_intr(void);

#endif /* VIRTIO_RNG_H_ */
