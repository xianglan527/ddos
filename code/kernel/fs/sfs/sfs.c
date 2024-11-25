#include "sfs.h"

#include "assert.h"
#include "bitmap.h"
#include "dev.h"
#include "error.h"
#include "fs.h"
#include "inode.h"
#include "iobuf.h"
#include "list.h"
#include "sfs.h"
#include "slab.h"
#include "stdio.h"
#include "string.h"
#include "types.h"
#include "vfs.h"

void lock_sfs_inodes(Sfs_fs *sfs) { down(&sfs->inodes_sem); }

void unlock_sfs_inodes(Sfs_fs *sfs) { up(&sfs->inodes_sem); }

static int sfs_sync(Fs *fs) {
    Sfs_fs *sfs = fsop_info(fs, sfs);
    lock_sfs_inodes(sfs);
    List_entry *le;
    list_for_each(le, &sfs->inode_list) {
        Sfs_inode *sin = le2sfs_inode(le, inode_link);
        vop_fsync(info2node(sin, sfs_inode));
    }
    unlock_sfs_inodes(sfs);
    return 0;
}

extern Spinlock print_struct_lock;
extern Inode *bootfs_node;

void sinode_dump(void) {
    static char *din_type[] = {
        [SFS_TYPE_DIR] = "dir",
        [SFS_TYPE_FILE] = "file",
        [SFS_TYPE_LINK] = "symlink",
        [SFS_TYPE_INVAL] = "inval",
    };
    Fs *fs;
    if (bootfs_node && bootfs_node->in_fs && bootfs_node->in_fs->fs_type == __fs_type(sfs)) {
        fs = bootfs_node->in_fs;
    } else {
        return;
    }
    fsop_sync(fs);
    Sfs_fs *sfs = fsop_info(fs, sfs);
    cprintf("\nsinode dump....................................\n\n");
    lock_sfs_inodes(sfs);
    List_entry *le;
    list_for_each(le, &sfs->inode_list) {
        Sfs_inode *sin = le2sfs_inode(le, inode_link);
        Inode *inode = info2node(sin, sfs_inode);
        Dinode *din = sin->din;
        cprintf(
            "ref_count : %lu open_count : %lu ino : %lu sem_value : %d type : %s nlink : "
            "%lu size : %lu",
            atomic_read(&inode->ref_count), atomic_read(&inode->open_count), sin->ino,
            inode->sem.value, din_type[din->type], din->nlink, din->size);
        cprintf("\n");
    }
    unlock_sfs_inodes(sfs);
    cprintf("\nend of sinode dump.............................\n");
}

void sinode_dump_struct_lock(void) {
    acquire(&print_struct_lock);
    sinode_dump();
    release(&print_struct_lock);
}

extern Inode *get_inode(Sfs_fs *sfs, uint ino);

static Inode *sfs_get_root(Fs *fs) {
    Sfs_fs *sfs = fsop_info(fs, sfs);
    Inode *root_inode = nullptr;
    root_inode = get_inode(sfs, SFS_ROOTINO);
    assert(root_inode != nullptr);
    return root_inode;
}

static void sfs_cleanup(Fs *fs) {
    for (int i = 0; i < 32; i++) { fsop_sync(fs); }
}

static int sfs_unmount(Fs *fs) {
    Sfs_fs *sfs = fsop_info(fs, sfs);
    if (!list_empty(&sfs->inode_list)) { return -E_BUSY; }
    kfree(sfs->hash_list);
    kfree(sfs);
    return 0;
}

// static int sfs_init_read(Device *dev, uint32_t blkno, void *blk_buffer){
//     Iobuf __iob, *iob = iobuf_init(&__iob, blk_buffer, SFS_BSIZE, blkno * SFS_BSIZE);
//     return dop_io(dev, iob, 0);
// }

static int check_superlock(Sfs_fs *sfs) {
    Disk_buf *buf = buf_read(sfs, SFS_BLKN_SUPER);
    sfs->sb = *(Superblock *)buf->data;
    if (sfs->sb.magic != SFS_MAGIC) {
        cprintf("sfs: wrong magic in superblock. (%08x should be %08x).\n", sfs->sb.magic, SFS_MAGIC);
        return -E_INVAL;
    }
    buf_release(sfs, buf);
    return 0;
}

static void buffer_init(Sfs_fs *sfs) {
    Disk_buf *buf;
    for (int i = 0; i < SFS_NBUF; i++) {
        assert((buf = kmalloc(sizeof(Disk_buf))) != nullptr);
        sem_init(&buf->sem, 1);
        atomic_set(&buf->refcnt, 0);
        buf->blockno = 0;
        buf->dev = sfs->dev;
        list_add(&sfs->buffer_list, &buf->link);
    }
    wait_queue_init(&sfs->wait_buf_queue);
}

static int sfs_do_mount(Device *dev, Fs **fs_store) {
    Fs *fs;
    if ((fs = alloc_fs(sfs)) == nullptr) { return -E_NO_MEM; }
    Sfs_fs *sfs = fsop_info(fs, sfs);
    sfs->dev = dev;
    list_init(&sfs->buffer_list);
    buffer_init(sfs);
    int ret;
    if ((ret = check_superlock(sfs)) != 0) { goto failed; }
    ret = -E_NO_MEM;
    List_entry *hash_list;
    if ((sfs->hash_list = hash_list = kmalloc(sizeof(List_entry) * SFS_HASH_LIST_SIZE)) == nullptr) {
        goto failed;
    }
    for (long i = 0; i < SFS_HASH_LIST_SIZE; i++) { list_init(hash_list + i); }
    initlock(&sfs->buffer_list_lock, "buffer_list_lock");
    sem_init(&sfs->inodes_sem, 1);
    list_init(&sfs->inode_list);
    fs->fs_sync = sfs_sync;
    fs->fs_get_root = sfs_get_root;
    fs->fs_unmount = sfs_unmount;
    fs->fs_cleanup = sfs_cleanup;
    *fs_store = fs;
    initlog(sfs);
    cprintf("sfs has mount successed.\n");
    return 0;
failed:
    kfree(fs);
    return ret;
}

static int sfs_mount(const char *devname) { return vfs_mount(devname, sfs_do_mount); }

void sfs_init(void) {
    int ret;
    if ((ret = sfs_mount("disk0")) != 0) { panic("failed: sfs: sfs_mount: %e.\n", ret); }
}
