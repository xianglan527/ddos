#include "assert.h"
#include "config.h"
#include "dev.h"
#include "error.h"
#include "fs.h"
#include "fs_img.h"
#include "inode.h"
#include "iobuf.h"
#include "list.h"
#include "sem.h"
#include "spinlock.h"
#include "stdio.h"
#include "types.h"
#include "vfs.h"
#include "virtio-blk.h"
#include "virtio_device.h"
#include "wait.h"

#define DISK0_BLKSIZE SFS_BSIZE
#define DISK0_BLK_NSECT (DISK0_BLKSIZE / SECTOR_SZIE)
static Sem disk0_sem;

static void disk0fs_read(Iobuf *iob) {
    struct blk_buf req;
    req.addr = iob->io_offset;
    req.data = iob->io_base;
    req.data_len = iob->io_len;
    req.is_write = 0;
    virtio_blk_rw(&req, "disk0.img");
}

static void disk0fs_write(Iobuf *iob) {
    struct blk_buf req;
    req.addr = iob->io_offset;
    req.data = iob->io_base;
    req.data_len = iob->io_len;
    req.is_write = 1;
    virtio_blk_rw(&req, "disk0.img");
}

static void disk0fs_read_syn(Iobuf *iob) {
    struct blk_buf req;
    req.addr = iob->io_offset;
    req.data = iob->io_base;
    req.data_len = iob->io_len;
    req.is_write = 0;
    virtio_blk_rw_syn(&req, "disk0.img");
}

static void disk0fs_write_syn(Iobuf *iob) {
    struct blk_buf req;
    req.addr = iob->io_offset;
    req.data = iob->io_base;
    req.data_len = iob->io_len;
    req.is_write = 1;
    virtio_blk_rw_syn(&req, "disk0.img");
}

static void lock_disk0(void) { down(&disk0_sem); }

static void unlock_disk0(void) { up(&disk0_sem); }

static int disk0_open(Device *dev, uint32_t open_flags) { return 0; }

static int disk0_close(Device *dev) { return 0; }

static int disk0_io(Device *dev, Iobuf *iob, bool write) {
    off_t offset = iob->io_offset;
    size_t resid = iob->io_resid;
    uint32_t blkno = offset / DISK0_BLKSIZE;
    uint32_t nblks = resid / DISK0_BLKSIZE;
    if ((offset % DISK0_BLKSIZE) != 0 || (resid % DISK0_BLKSIZE) != 0) { return -E_INVAL; }
    if (blkno + nblks > dev->d_blocks) { return -E_INVAL; }
    if (nblks == 0) { return 0; }
    assert(iob->io_resid == iob->io_len);
    lock_disk0();
    if (write) {
        disk0fs_write(iob);
    } else {
        disk0fs_read(iob);
    }
    unlock_disk0();
    return 0;
}

static int disk0_ioctl(Device *dev, int op, void *data) { return -E_UNIMP; }

static void disk0_check(void) {
    static_assert(DISK0_BLKSIZE % SECTOR_SZIE == 0);
    struct virtio_blk *blk = find_blk_by_name("disk0.img");
    assert(blk != nullptr);
    if (!get_device_status_ok(blk->idx)) panic("disk0.img isn't available.\n");
    static_assert((SFS_BSIZE % SECTOR_SZIE) == 0);
    assert(blk->capacity * SECTOR_SZIE == SFS_DISKSIZE);
}

static void disk0_device_init(Device *dev) {
    disk0_check();
    dev->d_blocks = find_blk_by_name("disk0.img")->capacity / DISK0_BLK_NSECT;
    dev->d_blocksize = DISK0_BLKSIZE;
    dev->d_open = disk0_open;
    dev->d_close = disk0_close;
    dev->d_io = disk0_io;
    dev->d_ioctl = disk0_ioctl;
    sem_init(&disk0_sem, 1);
}

void dev_init_disk0(void) {
    Inode *node;
    if ((node = dev_create_inode()) == nullptr) { panic("disk0: dev_create_node.\n"); }
    disk0_device_init(vop_info(node, device));
    int ret;
    if ((ret = vfs_add_dev("disk0", node, 1)) != 0) { panic("disk0: vfs_add_dev: %e.\n", ret); }
}