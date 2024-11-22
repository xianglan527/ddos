#ifndef __FS_SFS_SFS_H__
#define __FS_SFS_SFS_H__

#include "atomic.h"
#include "config.h"
#include "dev.h"
#include "fs_img.h"
#include "hash.h"
#include "list.h"
#include "sem.h"
#include "types.h"
#include "virtio-blk.h"
#include "virtio_device.h"
#include "wait.h"
#include "sysdef.h"

typedef struct sfs_inode Sfs_inode;
struct sfs_inode {
    Dinode *din;
    uint32_t ino;           // Inode number
    Sem sem;                /* semaphore for din */
    bool valid;             // Inode has been read from disk?
    List_entry inode_link;  // entry for linked-list in sfs_fs
    List_entry hash_link;   // entry for hash linked-list in sfs_fs
};

#define le2sfs_inode(le, member) to_struct((le), Sfs_inode, member)

#define SFS_HASH_SHIFT 10
#define SFS_HASH_LIST_SIZE (1 << SFS_HASH_SHIFT)
#define inode_hashfn(x) (hash64(x, SFS_HASH_SHIFT))

typedef struct disk_buf Disk_buf;
struct disk_buf {
    bool valid;
    Device *dev;
    uint blockno;
    Sem sem;
    Atomic refcnt;
    uchar data[SFS_BSIZE];
    List_entry link;
};

#define le2disk_buf(le) to_struct((le), Disk_buf, link)

typedef struct sfs_fs Sfs_fs;
struct sfs_fs{
    Superblock sb;
    Device *dev;
    List_entry buffer_list;
    Spinlock buffer_list_lock;
    Sem inodes_sem;
    List_entry inode_list;
    List_entry *hash_list;
    Wait_queue wait_buf_queue;
};

#define MEM2DISK 1
#define DISK2MEM 0

#define min(a, b) ((a) < (b) ? (a) : (b))

#define sfs_dentry_size sizeof(((Sfs_dirent *)0)->name)

typedef struct fs Fs;
typedef struct inode Inode;

Disk_buf *buf_read(Sfs_fs *sfs, uint blockno);
void buf_write(Disk_buf *buf);
void buf_release(Sfs_fs *sfs, Disk_buf *buf);


void lock_sfs_inodes(Sfs_fs *sfs);
void unlock_sfs_inodes(Sfs_fs *sfs);
void sfs_init(void);

void sinode_dump(void);
void sinode_dump_struct_lock(void);
#endif