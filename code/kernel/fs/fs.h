#ifndef __FS_FS_H__
#define __FS_FS_H__
#include "types.h"
#include "riscv.h"
#include "virtio-blk.h"
#include "inode.h"
#include "atomic.h"
#include "sem.h"
#include "file.h"
#include "config.h"

#define PAGE_NSECT (PGSIZE / SECTOR_SZIE)

typedef struct fs_struct Fs_struct;
struct fs_struct{
    Inode *pwd;
    File *filemap;
    Atomic fs_count;
    Sem fs_sem;
};

#define FS_STRUCT_BUFSIZE (FS_STRUCT_TOTAL_SIZE - sizeof(Fs_struct))
#define FS_STRUCT_NENTRY    (FS_STRUCT_BUFSIZE / sizeof(File))

void fs_init(void);

void lock_fs(Fs_struct *fs_struct);
void unlock_fs(Fs_struct *fs_struct);

Fs_struct *fs_create(void);
void fs_destroy(Fs_struct *fs_struct);
int dup_fs(Fs_struct *to, Fs_struct *from);

static inline long fs_count(Fs_struct *fs_struct) { return atomic_read(&(fs_struct->fs_count)); }

static inline long fs_count_inc(Fs_struct *fs_struct) {
    return atomic_add_return(&(fs_struct->fs_count), 1);
}

static inline long fs_count_dec(Fs_struct *fs_struct) {
    return atomic_sub_return(&(fs_struct->fs_count), 1);
}
#endif