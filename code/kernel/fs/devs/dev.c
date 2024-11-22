#include "dev.h"

#include "assert.h"
#include "error.h"
#include "inode.h"
#include "stat.h"
#include "stdio.h"
#include "string.h"
#include "sysdef.h"

static int dev_open(Inode *node, uint32_t open_flags) {
    if (open_flags & (O_CREAT | O_TRUNC | O_EXCL | O_APPEND)) { return -E_INVAL; }
    Device *dev = vop_info(node, device);
    return dop_open(dev, open_flags);
}

static int dev_close(Inode *node) {
    Device *dev = vop_info(node, device);
    return dop_close(dev);
}

static int dev_read(Inode *node, Iobuf *iob) {
    struct device *dev = vop_info(node, device);
    return dop_io(dev, iob, 0);
}

static int dev_write(Inode *node, Iobuf *iob) {
    struct device *dev = vop_info(node, device);
    return dop_io(dev, iob, 1);
}

static int dev_ioctl(Inode *node, int op, void *data) {
    struct device *dev = vop_info(node, device);
    return dop_ioctl(dev, op, data);
}

static int dev_fstat(Inode *node, Stat *stat) {
    int ret;
    memset(stat, 0, sizeof(Stat));
    if ((ret = vop_gettype(node, &stat->st_mode)) != 0) { return ret; }
    Device *dev = vop_info(node, device);
    stat->st_nlinks = 1;
    stat->st_blocks = dev->d_blocks;
    stat->st_size = stat->st_blocks * dev->d_blocksize;
    return 0;
}

static int dev_gettype(Inode *node, uint32_t *type_store) {
    Device *dev = vop_info(node, device);
    *type_store = (dev->d_blocks > 0) ? S_IFBLK : S_IFCHR;
    return 0;
}

static int dev_tryseek(Inode *node, off_t pos) {
    Device *dev = vop_info(node, device);
    if (dev->d_blocks > 0) {
        if ((pos % dev->d_blocksize) == 0) {
            if (pos >= 0 && pos < dev->d_blocks * dev->d_blocksize) { return 0; }
        }
    }
    return -E_INVAL;
}

/*
 * Name lookup.
 *
 * One interesting feature of device:name pathname syntax is that you
 * can implement pathnames on arbitrary devices. For instance, if you
 * had a graphics device that supported multiple resolutions (which we
 * don't), you might arrange things so that you could open it with
 * pathnames like "video:800x600/24bpp" in order to select the operating
 * mode.
 *
 * However, we have no support for this in the base system.
 */
static int dev_lookup(Inode *node, char *path, Inode **node_store) {
    if (*path != '\0') { return -E_NOENT; }
    vop_ref_inc(node);
    *node_store = node;
    return 0;
}

static const Inode_ops dev_node_ops = {
    .vop_magic = VOP_MAGIC,
    .vop_open = dev_open,
    .vop_close = dev_close,
    .vop_read = dev_read,
    .vop_write = dev_write,
    .vop_fstat = dev_fstat,
    .vop_fsync = NULL_VOP_PASS,
    .vop_mkdir = NULL_VOP_NOTDIR,
    .vop_link = NULL_VOP_NOTDIR,
    .vop_rename = NULL_VOP_NOTDIR,
    .vop_readlink = NULL_VOP_INVAL,
    .vop_symlink = NULL_VOP_NOTDIR,
    .vop_namefile = NULL_VOP_PASS,
    .vop_getdirentry = NULL_VOP_INVAL,
    .vop_reclaim = NULL_VOP_PASS,
    .vop_ioctl = dev_ioctl,
    .vop_gettype = dev_gettype,
    .vop_tryseek = dev_tryseek,
    .vop_truncate = NULL_VOP_INVAL,
    .vop_create = NULL_VOP_NOTDIR,
    .vop_unlink = NULL_VOP_NOTDIR,
    .vop_lookup = dev_lookup,
    .vop_lookup_parent = NULL_VOP_NOTDIR,
};

#define init_device(x)                  \
    do {                                \
        extern void dev_init_##x(void); \
        dev_init_##x();                 \
    } while (0)

void dev_init(void){
    init_device(null);
    init_device(stdin);
    init_device(stdout);
    init_device(disk0);
}

Inode *dev_create_inode(void){
    Inode *node;
    if((node = alloc_inode(device)) != nullptr){
        vop_init(node, &dev_node_ops, nullptr);
    }
    return node;
}