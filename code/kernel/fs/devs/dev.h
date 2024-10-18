#ifndef __FS_DEVS_DEV_H__
#define __FS_DEVS_DEV_H__

#include "types.h"
#include "iobuf.h"

typedef struct inode Inode;

typedef struct device Device;
struct device{
    size_t d_blocks;
    size_t d_blocksize;
    int (*d_open)(Device *dev, uint32_t open_flags);
    int (*d_close)(Device *dev);
    int (*d_io)(Device *dev, Iobuf *iob, bool write);
    int (*d_ioctl)(Device *dev, int op, void *data); 
};

#define dop_open(dev, open_flags)   ((dev)->d_open(dev, open_flags))
#define dop_close(dev) ((dev)->d_close(dev))
#define dop_io(dev, iob, write) ((dev)->d_io(dev, iob, write))
#define dop_ioctl(dev, op, data) ((dev)->d_ioctl(dev, op, data))

void dev_init(void);
Inode *dev_create_inode(void);
#endif