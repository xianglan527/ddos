#include "assert.h"
#include "config.h"
#include "dev.h"
#include "error.h"
#include "inode.h"
#include "iobuf.h"
#include "proc.h"
#include "spinlock.h"
#include "stdio.h"
#include "string.h"
#include "sysdef.h"
#include "uart.h"
#include "vfs.h"
#include "wait.h"

static Wait_queue __wait_queue, *wait_queue = &__wait_queue;
#define BACKSPACE 0x100
#define C(x) ((x) - '@')  // Control-x

static struct {
    Spinlock lock;
    uint8_t buf[STDIN_BUFSIZE];
    size_t rpos;
    size_t wpos;
    size_t epos;
} stdin_str;

static void stdin_str_putc(int c) {
    if (c == BACKSPACE) {
        uart_putc('\b');
        uart_putc(' ');
        uart_putc('\b');
    } else
        uart_putc(c);
}

void dev_stdin_write(char c){
    acquire(&stdin_str.lock);
    if(c != 0 || c != -1){
        switch (c) {
            case C('X'): proc_dump(); break;
            case C('U'):
                while (stdin_str.epos != stdin_str.wpos && stdin_str.buf[(stdin_str.epos - 1) % STDIN_BUFSIZE] != '\n') {
                    stdin_str.epos--;
                    stdin_str_putc(BACKSPACE);
                }
                break;
            case C('H'):
            case '\x7f':
                if (stdin_str.epos != stdin_str.wpos) {
                    stdin_str.epos--;
                    stdin_str_putc(BACKSPACE);
                }
                break;
            default:
                if (c != 0 && stdin_str.epos - stdin_str.rpos < STDIN_BUFSIZE) {
                    c = (c == '\r' || c == C('D')) ? '\n' : c;
                    stdin_str_putc(c);
                    stdin_str.buf[stdin_str.epos++ % STDIN_BUFSIZE] = c;
                    if (c == '\n' || stdin_str.epos == stdin_str.rpos + STDIN_BUFSIZE) {
                        if (!wait_queue_empty(wait_queue)) { 
                            wakeup_queue(wait_queue, WT_KBD, 1); 
                        }
                        stdin_str.wpos = stdin_str.epos; 
                    }
                }
                break;
        }
    }
    release(&stdin_str.lock);
}

static int dev_stdin_read(char *buf, size_t len){
    long ret = 0;
    char c;
    acquire(&stdin_str.lock);
    for(; ret < len; ret++, stdin_str.rpos++){
        try_again:
            if (stdin_str.rpos < stdin_str.wpos) {
                c = stdin_str.buf[stdin_str.rpos % STDIN_BUFSIZE];
                if(c == '\n'){
                    if(ret == 0){
                        stdin_str.rpos++;
                    }
                    *buf = 0;
                    break;;
                }else{
                    *buf++ = stdin_str.buf[stdin_str.rpos % STDIN_BUFSIZE];
                }
            } else {
                Wait __wait, *wait = &__wait;
                wait_current_set(wait_queue, wait, WT_KBD);
                sleeping(myproc(), &stdin_str.lock);
                wait_current_del(wait_queue, wait);
                if(wait->wakeup_flags == WT_KBD){
                    goto try_again;
                }
                break;
            }
    }
    release(&stdin_str.lock);
    return ret;
}

int stdin_getchar(void) {
    char c;
    int ret;
    ret = dev_stdin_read(&c, 1);
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

    wait_queue_init(wait_queue);
    initlock(&stdin_str.lock, "stdin_str_lock");
}

void dev_init_stdin(void) {
    Inode *node;
    if ((node = dev_create_inode()) == nullptr) { panic("stdin: dev_create_node.\n"); }
    stdin_device_init(vop_info(node, device));

    int ret;
    if ((ret = vfs_add_dev("stdin", node, 0)) != 0) { panic("stdin: vfs_add_dev: %e.\n", ret); }
}
