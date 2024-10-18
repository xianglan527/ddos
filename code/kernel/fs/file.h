#ifndef __FS_FILE_H__
#define __FS_FILE_H__
#include "inode.h"
#include "types.h"
#include "atomic.h"
#include "stat.h"
#include "sem.h"

typedef struct file File;
struct file{
    enum{
        FD_NONE, FD_INIT, FD_OPENED, FD_CLOSED,
    }status;
    bool readable;
    bool writable;
    int fd;
    off_t pos;
    Inode *node;
    Atomic open_count;
    // Sem file_sem;
};

void filemap_init(File *filemap);
void filemap_open(File *file);
void filemap_close(File *file);
void filemap_dup(File *to, File *from);
bool file_testfd(int fd, bool readable, bool writable);

int file_open(char *path, uint32_t open_flags);
int file_close(int fd);
int file_read(int fd, void *base, size_t len, size_t *copied_store);
int file_write(int fd, void *base, size_t len, size_t *copied_store);
int file_fstat(int fd, struct stat *stat);
int file_dup(int fd1, int fd2);
int fd2file(int fd, File **file_store);
void filemap_acquire(File *file);
void filemap_release(File *file);

static inline long fopen_count(File *file){
    return atomic_read(&file->open_count);
}

static inline long fopen_count_inc(File *file){
    return atomic_add_return(&file->open_count, 1);
}

static inline long fopen_count_dec(File *file){
    return atomic_sub_return(&file->open_count, 1);
}
#endif