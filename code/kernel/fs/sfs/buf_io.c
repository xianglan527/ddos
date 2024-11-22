#ifndef __FS_SFS_BUF_IO_H__
#define __FS_SFS_BUF_IO_H__

#include "sfs.h"
#include "proc.h"

static void lock_disk_buf(Disk_buf *buf){
    down(&buf->sem);
}

static void unlock_disk_buf(Disk_buf *buf){
    up(&buf->sem);
}

static bool disk_buf_wait(Sfs_fs *sfs){
    Wait __wait, *wait = &__wait;
    wait_current_set(&sfs->wait_buf_queue, wait, WT_DISK_BUF);
    sleeping(myproc(), &sfs->buffer_list_lock);
    wait_current_del(&sfs->wait_buf_queue, wait);
    return wait->wakeup_flags == WT_DISK_BUF;
}

static void disk_buf_wakeup(Sfs_fs *sfs){
    if(!wait_queue_empty(&sfs->wait_buf_queue)){
        acquire(&sfs->buffer_list_lock);
        wakeup_queue(&sfs->wait_buf_queue, WT_DISK_BUF, 1);
        release(&sfs->buffer_list_lock);
    }
}

static Disk_buf *buf_get(Sfs_fs *sfs, uint blockno) {
try_again:
    acquire(&sfs->buffer_list_lock);
    List_entry *le;
    list_for_each(le, &sfs->buffer_list) {
        Disk_buf *buf = le2disk_buf(le);
        if(buf->dev == sfs->dev && buf->blockno == blockno){
            atomic_inc(&buf->refcnt);
            release(&sfs->buffer_list_lock);
            lock_disk_buf(buf);
            return buf;
        }
    }
    list_for_each_prev(le, &sfs->buffer_list) {
        Disk_buf *buf = le2disk_buf(le);
        if(atomic_read(&buf->refcnt) == 0){
            buf->dev = sfs->dev;
            buf->blockno = blockno;
            buf->valid = 0;
            atomic_set(&buf->refcnt, 1);
            release(&sfs->buffer_list_lock);
            lock_disk_buf(buf);
            return buf;
        }
    }
    assert(disk_buf_wait(sfs) == true);
    release(&sfs->buffer_list_lock);
    goto try_again;
}

Disk_buf *buf_read(Sfs_fs *sfs, uint blockno){
    Disk_buf *buf = buf_get(sfs, blockno);
    if(!buf->valid){
        Iobuf __iob, *iob = iobuf_init(&__iob, buf->data, SFS_BSIZE, buf->blockno * SFS_BSIZE);
        dop_io(sfs->dev, iob, 0);
        buf->valid = true;
    }
    return buf;
}

void buf_write(Disk_buf *buf){
    assert(buf->sem.value == 0);  //must be locked
    Iobuf __iob, *iob = iobuf_init(&__iob, buf->data, SFS_BSIZE, buf->blockno * SFS_BSIZE);
    dop_io(buf->dev, iob, 1);
    buf->valid = true;
}

void buf_release(Sfs_fs *sfs, Disk_buf *buf) {
    assert(buf->sem.value == 0);  // must be locked
    unlock_disk_buf(buf);
    acquire(&sfs->buffer_list_lock);
    if(atomic_sub_return(&buf->refcnt, 1) == 0){
        list_del(&buf->link);
        list_add_before(&sfs->buffer_list, &buf->link);
    }
    release(&sfs->buffer_list_lock);
    disk_buf_wakeup(sfs);
}


#endif