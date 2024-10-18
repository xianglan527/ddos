#include "assert.h"
#include "dev.h"
#include "error.h"
#include "inode.h"
#include "iobuf.h"
#include "stdio.h"
#include "vfs.h"
#include "config.h"
#include "wait.h"
#include "spinlock.h"
#include "proc.h"
#include "sysdef.h"

Spinlock stdin_lock;
static char stdin_buffer[STDIN_BUFSIZE];
static off_t p_rpos, p_wpos;

static Wait_queue __wait_queue, *wait_queue = &__wait_queue;

void dev_stdin_write(char c){
    if(c != '\0'){
        acquire(&stdin_lock); 
        stdin_buffer[p_wpos % STDIN_BUFSIZE] = c;
        if(p_wpos - p_rpos < STDIN_BUFSIZE){
            p_wpos++;
        }
        if(!wait_queue_empty(wait_queue)){
            wakeup_queue(wait_queue, WT_KBD, 1);
        }
        release(&stdin_lock);
    }
}

static int dev_stdin_read(char *buf, size_t len){
    long ret = 0;
    acquire(&stdin_lock);
    for(; ret < len; ret++, p_rpos++){
        try_again:
            if(p_rpos < p_wpos){
                *buf++ = stdin_buffer[p_rpos % STDIN_BUFSIZE];
            }else{
                Wait __wait, *wait = &__wait;
                wait_current_set(wait_queue, wait, WT_KBD);
                sleeping(myproc(), &stdin_lock);
                wait_current_del(wait_queue, wait);
                if(wait->wakeup_flags == WT_KBD){
                    goto try_again;
                }
                break;
            }
    }
    release(&stdin_lock);
    return ret;
}

static int stdin_open(Device *dev, uint32_t open_flags){
    if(open_flags != O_RDONLY){
        return -E_INVAL;
    }
    return 0;
}

static int
stdin_close(Device *dev) {
    return 0;
}

static int stdin_io(Device *dev, Iobuf *iob, bool write){
    if(!write){
        long ret;
        if((ret = dev_stdin_read(iob->io_base, iob->io_resid)) > 0){
            iob->io_resid -= ret;
        }
        return 0;
    }
    return -E_INVAL;
}

static int stdin_ioctl(Device *dev, int op, void *data){
    return -E_INVAL;
}

static void stdin_device_init(Device *dev){
    dev->d_blocks = 0;
    dev->d_blocks = 0;
    dev->d_blocksize = 1;
    dev->d_open = stdin_open;
    dev->d_close = stdin_close;
    dev->d_io = stdin_io;
    dev->d_ioctl = stdin_ioctl;

    p_rpos = p_wpos = 0;
    wait_queue_init(wait_queue);
    initlock(&stdin_lock, "stdin_lock");
}

void dev_init_stdin(void) {
    Inode *node;
    if ((node = dev_create_inode()) == nullptr) { panic("stdin: dev_create_node.\n"); }
    stdin_device_init(vop_info(node, device));

    int ret;
    if ((ret = vfs_add_dev("stdin", node, 0)) != 0) { panic("stdin: vfs_add_dev: %e.\n", ret); }
}
