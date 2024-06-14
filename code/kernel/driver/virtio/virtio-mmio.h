#ifndef __VIRTIO_VIRTIO_MMIO_H_
#define __VIRTIO_VIRTIO_MMIO_H_

#include "types.h"
#include "virtio-ring.h"

#define	VIRTIO_MMIO_MAGIC_VALUE		    0x000   // 0x74726976
#define	VIRTIO_MMIO_VERSION		        0x004   // version; should be 2
#define	VIRTIO_MMIO_DEVICE_ID		    0x008   // device type; 1 is net, 2 is disk, 4 is rng
#define	VIRTIO_MMIO_VENDOR_ID		    0x00c   // 0x554d4551
#define	VIRTIO_MMIO_HOST_FEATURES	    0x010
#define	VIRTIO_MMIO_HOST_FEATURES_SEL	0x014
#define	VIRTIO_MMIO_GUEST_FEATURES	    0x020
#define	VIRTIO_MMIO_GUEST_FEATURES_SEL	0x024
#define	VIRTIO_MMIO_GUEST_PAGE_SIZE	    0x028	/* version 1 only */
#define	VIRTIO_MMIO_QUEUE_SEL		    0x030   // select queue, write-only
#define	VIRTIO_MMIO_QUEUE_NUM_MAX	    0x034   // max size of current queue, read-only
#define	VIRTIO_MMIO_QUEUE_NUM		    0x038   // size of current queue, write-only
#define	VIRTIO_MMIO_QUEUE_ALIGN		    0x03c	/* version 1 only */
#define	VIRTIO_MMIO_QUEUE_PFN		    0x040	/* version 1 only */
#define	VIRTIO_MMIO_QUEUE_READY		    0x044	/* ready bit, requires version 2 */
#define	VIRTIO_MMIO_QUEUE_NOTIFY	    0x050   // write-only
#define	VIRTIO_MMIO_INTERRUPT_STATUS	0x060   // read-only
#define	VIRTIO_MMIO_INTERRUPT_ACK	    0x064   // write-only
#define	VIRTIO_MMIO_STATUS		        0x070   // read/write
#define	VIRTIO_MMIO_QUEUE_DESC_LOW	    0x080	/* physical address for descriptor table, write-only, requires version 2 */
#define	VIRTIO_MMIO_QUEUE_DESC_HIGH	    0x084	/* requires version 2 */
#define	VIRTIO_MMIO_QUEUE_AVAIL_LOW	    0x090	/* physical address for available ring, write-only, requires version 2 */
#define	VIRTIO_MMIO_QUEUE_AVAIL_HIGH	0x094	/* requires version 2 */
#define	VIRTIO_MMIO_QUEUE_USED_LOW	    0x0a0	/* physical address for used ring, write-only, requires version 2 */
#define	VIRTIO_MMIO_QUEUE_USED_HIGH	    0x0a4	/* requires version 2 */
#define	VIRTIO_MMIO_CONFIG_GENERATION	0x0fc	/* requires version 2 */
#define	VIRTIO_MMIO_CONFIG	            0x100	/* requires version 2 */

#define INT_MASK  ((1UL << 32) - 1)

void virtio_mmio_device_info(int idx);
uint32_t virtio_mmio_read_reg(uint32_t addr, int idx);
uint8_t virtio_mmio_read_reg8(uint32_t addr, int idx);
uint32_t virtio_mmio_get_status(int idx);
void virtio_mmio_set_status(uint32_t status, int idx);
void virtio_mmio_reset_device(int idx);
uint64_t virtio_mmio_get_host_features(int idx);
void virtio_mmio_set_guest_features(uint64_t features, int idx);
int virtio_mmio_get_queue_ready(int qnum, int idx);
void virtio_mmio_set_queue_ready(int qnum, int idx);
int virtio_mmio_get_queue_size(int qnum, int idx);
void virtio_mmio_set_queue_size(int qnum, uint32_t qsize, int idx);
void virtio_mmio_set_queue_addr(int qnum, struct vring *vr, int idx);
void virtio_mmio_set_notify(int qnum, int idx);
void virtio_mmio_set_ack(int idx);

#endif /* VIRTIO_MMIO_H_ */
