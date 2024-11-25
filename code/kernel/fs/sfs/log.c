#include "assert.h"
#include "error.h"
#include "fs.h"
#include "inode.h"
#include "iobuf.h"
#include "proc.h"
#include "sfs.h"
#include "spinlock.h"
#include "stdio.h"
#include "string.h"
#include "types.h"

// Simple logging that allows concurrent FS system calls.
//
// A log transaction contains the updates of multiple FS system
// calls. The logging system only commits when there are
// no FS system calls active. Thus there is never
// any reasoning required about whether a commit might
// write an uncommitted system call's updates to disk.
//
// A system call should call begin_op()/end_op() to mark
// its start and end. Usually begin_op() just increments
// the count of in-progress FS system calls and returns.
// But if it thinks the log is close to running out, it
// sleeps until the last outstanding end_op() commits.
//
// The log is a physical re-do log containing disk blocks.
// The on-disk log format:
//   header block, containing block #s for block A, B, C, ...
//   block A
//   block B
//   block C
//   ...
// Log appends are synchronous.

// Contents of the header block, used for both the on-disk header block
// and to keep track in memory of logged block# before commit.




// How to test log system:

// First, it must be clarified that any code module functionality that has not undergone effective testing
// should be considered as having bugs. As for the logging functionality of the file system, I do not plan to
// test it at this stage. This can be marked as a TODO for future completion. The initial testing approach is
// recorded as follows:

// When simulating RISC-V using QEMU, writing 0x5555 to the physical address 0x10000 will cause the system to
// crash. This effectively simulates a power failure crash in a physical machine. Calling crash() within
// buf_write() will lead to the system crashing after a certain number of write operations (via buf_write()).
// This setup allows us to create a potentially corrupted disk file. Subsequently, a test program can be
// written to check the robustness of the file system within the disk file to determine whether the logging
// functionality operates as expected.

// kvmmap(0x100000, 0x100000, PGSIZE, PTE_R | PTE_W);

// void crash(){
//     static int count = 100;
//     if(--count == 0){
//         *(uint *)0x100000 = 0x5555;
//     }
// }

// void buf_write(Disk_buf *buf) {
//     assert(buf->sem.value == 0);  // must be locked
//     Iobuf __iob, *iob = iobuf_init(&__iob, buf->data, SFS_BSIZE, buf->blockno * SFS_BSIZE);
//     dop_io(buf->dev, iob, 1);
//     buf->valid = true;
//     crash();
// }

Log log;

#define log_start (log.sfs->sb.logstart)
#define log_size (log.sfs->sb.nlog)

static void recover_from_log(void);
static void commit();

void initlog(Sfs_fs *sfs) {
    assert(sizeof(Log_header) < SFS_BSIZE);
    initlock(&log.log_lock, "log_lock");
    log.sfs = sfs;
    log.committing = 0;
    recover_from_log();
    wait_queue_init(&log.log_wait_queue);
}

static void install_trans(bool recovering) {
    for (int tail = 0; tail < log.lh.log_num; tail++) {
        Disk_buf *lbuf = buf_read(log.sfs, log_start + tail + 1);
        Disk_buf *dbuf = buf_read(log.sfs, log.lh.block[tail]);
        memmove(dbuf->data, lbuf->data, SFS_BSIZE);
        buf_write(dbuf);
        if (recovering == false) { atomic_dec(&dbuf->refcnt); }
        buf_release(log.sfs, lbuf);
        buf_release(log.sfs, dbuf);
    }
}

static void read_head(void) {
    Disk_buf *buf = buf_read(log.sfs, log_start);
    Log_header *lh = (Log_header *)(buf->data);
    log.lh.log_num = lh->log_num;
    for (int i = 0; i < log.lh.log_num; i++) { log.lh.block[i] = lh->block[i]; }
    buf_release(log.sfs, buf);
}

static void write_head(void) {
    Disk_buf *buf = buf_read(log.sfs, log_start);
    Log_header *lh = (Log_header *)(buf->data);
    lh->log_num = log.lh.log_num;
    for (int i = 0; i < log.lh.log_num; i++) { lh->block[i] = log.lh.block[i]; }
    buf_write(buf);
    buf_release(log.sfs, buf);
}

static void recover_from_log(void) {
    read_head();
    install_trans(true);
    log.lh.log_num = 0;
    write_head();
}

static void log_wait_nolock() {
    // acquire(&log.log_lock);
    Wait __wait, *wait = &__wait;
    wait_current_set(&log.log_wait_queue, wait, WT_LOG);
    sleeping(myproc(), &log.log_lock);
    wait_current_del(&log.log_wait_queue, wait);
    // release(&log.log_lock);
    // assert(wait->wakeup_flags == WT_LOG);
}

static void log_wakeup_nolock() {
    if (!wait_queue_empty(&log.log_wait_queue)) { wakeup_first(&log.log_wait_queue, WT_PIPE, 1); }
}

void begin_op(void) {
    acquire(&log.log_lock);
    while (1) {
        if (log.committing) {
            log_wait_nolock();
        } else if (log.lh.log_num + (log.outstanding + 1) * SFS_MAXOPBLOCKS > SFS_LOGSIZE) {
            log_wait_nolock();
        } else {
            log.outstanding++;
            release(&log.log_lock);
            break;
        }
    }
}

void end_op(void) {
    bool do_commit = false;
    acquire(&log.log_lock);
    log.outstanding--;
    assert(log.committing == false);
    if (log.outstanding == 0) {
        do_commit = true;
        log.committing = true;
    } else {
        log_wakeup_nolock();
    }
    release(&log.log_lock);
    if (do_commit) {
        commit();
        acquire(&log.log_lock);
        log.committing = false;
        log_wakeup_nolock();
        release(&log.log_lock);
    }
}

static void write_log(void) {
    for (int tail = 0; tail < log.lh.log_num; tail++) {
        Disk_buf *to = buf_read(log.sfs, log_start + tail + 1);
        Disk_buf *from = buf_read(log.sfs, log.lh.block[tail]);
        memmove(to->data, from->data, SFS_BSIZE);
        buf_write(to);
        buf_release(log.sfs, from);
        buf_release(log.sfs, to);
    }
}

static void commit(void) {
    if (log.lh.log_num > 0) {
        write_log();
        write_head();
        install_trans(0);
        log.lh.log_num = 0;
        write_head();
    }
}

void log_write(Disk_buf *buf) {
    if (log.lh.log_num >= log_size - 1) { panic("too big a transaction"); }
    if (!(log.outstanding >= 1)) { int pp = 3; }
    assert(log.outstanding >= 1);
    acquire(&log.log_lock);
    int i;
    for (i = 0; i < log.lh.log_num; i++) {
        if (log.lh.block[i] == buf->blockno) { break; }
    }
    log.lh.block[i] = buf->blockno;
    if (i == log.lh.log_num) {
        atomic_inc(&buf->refcnt);
        log.lh.log_num++;
    }
    release(&log.log_lock);
}