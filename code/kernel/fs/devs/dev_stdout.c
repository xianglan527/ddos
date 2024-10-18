#include "assert.h"
#include "dev.h"
#include "error.h"
#include "inode.h"
#include "iobuf.h"
#include "stdio.h"
#include "sysdef.h"
#include "vfs.h"
#include "uart.h"
#include "spinlock.h"

extern struct {
    Spinlock lock;
    int locking;
} pr;

static int stdout_open(Device *dev, uint32_t open_flags) {
    if (open_flags != O_WRONLY) { return -E_INVAL; }
    return 0;
}

static int stdout_close(Device *dev) { return 0; }

static int stdout_io(Device *dev, Iobuf *iob, bool write){
    if(write){
        char *data = iob->io_base;
        int locking = pr.locking;
        if (locking) acquire(&pr.lock);
        for(;iob->io_resid != 0; iob->io_resid--){
            uart_putc(*data++);
        }
        if (locking) release(&pr.lock);
        return 0;
    }
    return -E_INVAL;
}

static int stdout_ioctl(Device *dev, int op, void *data) { return -E_INVAL; }

static void stdout_device_init(Device *dev) {
    dev->d_blocks = 0;
    dev->d_blocksize = 1;
    dev->d_open = stdout_open;
    dev->d_close = stdout_close;
    dev->d_io = stdout_io;
    dev->d_ioctl = stdout_ioctl;
}

void dev_init_stdout(void) {
    Inode *node;
    if ((node = dev_create_inode()) == NULL) { panic("stdout: dev_create_node.\n"); }
    stdout_device_init(vop_info(node, device));

    int ret;
    if ((ret = vfs_add_dev("stdout", node, 0)) != 0) { panic("stdout: vfs_add_dev: %e.\n", ret); }
}