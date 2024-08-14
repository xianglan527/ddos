#ifndef __VIRTIO_VIRTIO_BLK_H_
#define __VIRTIO_VIRTIO_BLK_H_

#include "types.h"
#include "virtio-ring.h"
#include "list.h"
#include "spinlock.h"

#define BLK_QSIZE       (128)   // blk queue0 size
#define SECTOR_SZIE     (512)   // blk sector size

// block device feature bits
#define VIRTIO_BLK_F_RO              5	/* Disk is read-only */
#define VIRTIO_BLK_F_SCSI            7	/* Supports scsi command passthru */
#define VIRTIO_BLK_F_FLUSH           9  // Cache flush command support
#define VIRTIO_BLK_F_CONFIG_WCE     11	/* Writeback mode available in config */
#define VIRTIO_BLK_F_MQ             12	/* support more than one vq */

// As per VirtIO spec Version 1.1: 5.2.4 Device configuration layout
struct virtio_blk_cfg {
	uint64_t	capacity;
	uint32_t	size_max;
	uint32_t	seg_max;
	struct	virtio_blk_geometry {
		uint16_t	cylinders;
		uint8_t 	heads;
		uint8_t 	sectors;
	} geometry;
	uint32_t	blk_size;
	struct virtio_blk_topology {
		// # of logical blocks per physical block (log2)
		uint8_t physical_block_exp;
		// offset of first aligned logical block
		uint8_t alignment_offset;
		// suggested minimum I/O size in blocks
			uint16_t min_io_size;
		// optimal (suggested maximum) I/O size in blocks
		uint32_t opt_io_size;
	} topology;
	uint8_t writeback;
	uint8_t unused0[3];
	uint32_t max_discard_sectors;
	uint32_t max_discard_seg;
	uint32_t discard_sector_alignment;
	uint32_t max_write_zeroes_sectors;
	uint32_t max_write_zeroes_seg;
	uint8_t write_zeroes_may_unmap;
	uint8_t unused1[3];
} __attribute__((packed));

// these are specific to virtio block devices, e.g. disks,
// described in Section 5.2 of the spec.

#define VIRTIO_BLK_T_IN     0 // read the disk
#define VIRTIO_BLK_T_OUT    1 // write the disk
#define VIRTIO_BLK_T_FLUSH  4 // flush the disk

// the format of the first descriptor in a disk request.
// to be followed by two more descriptors containing
// the block, and a one-byte status.
struct virtio_blk_req {
  uint32_t type; // VIRTIO_BLK_T_IN or ..._OUT
  uint32_t reserved;
  uint64_t sector;
};

#define BSIZE 1024  // block size
// struct buf {
//     int valid;  // has data been read from disk?
//     int disk;   // does disk "own" buf?
//     uint dev;
//     uint blockno;
//     struct sleeplock lock;
//     uint refcnt;
//     struct buf *prev;  // LRU cache list
//     struct buf *next;
//     uchar data[BSIZE];
// };

struct blk_buf {
    uint64_t addr;  // bytes address
    void *data;
    uint64_t data_len;
    uint16_t is_write;
    uint16_t flag;
    bool syn;
};
struct virtio_blk {
	char blk_name[64];
	int idx;
    List_entry blk_list;
	Spinlock blk_lock;
    uint8_t  status[BLK_QSIZE];
    void *info[BLK_QSIZE];
    // disk command headers.
    // one-for-one with descriptors, for convenience.
    struct virtio_blk_req ops[BLK_QSIZE];
    struct vring vr;
	uint32_t capacity;
	uint32_t qsize;	// queue0 size
    uint16_t used_idx;
	uint16_t avail_idx;
	struct blk_buf *blk_buffer;
};


int virtio_blk_init(uint32_t base, int idx);
int virtio_blk_add(uint32_t base, char *name,int idx);
void virtio_blk_cfg(struct virtio_blk *blk);
void virtio_blk_rw(struct blk_buf *b, char *blk_name);
void virtio_blk_intr(int idx);
struct virtio_blk *find_blk_by_name(char *name);
struct virtio_blk *find_blk_by_index(int idx);
void virtio_blk_rw_syn(struct blk_buf *b, char *blk_name);
void virtio_blk_rw_asyn(struct blk_buf *b, char *blk_name);
#endif /* VIRTIO_BLK_H_ */
