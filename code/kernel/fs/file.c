#include "file.h"

#include "assert.h"
#include "dev.h"
#include "error.h"
#include "inode.h"
#include "list.h"
#include "proc.h"
#include "stat.h"
#include "stdio.h"
#include "string.h"
#include "sysdef.h"
#include "vfs.h"
#include "slab.h"
#include "string.h"

#define testfd(fd)      ((fd) >= 0 && (fd) < FS_STRUCT_NENTRY)

static File *get_filemap(void){
    Fs_struct *fs_struct = myproc()->fs_struct;
    assert(fs_struct != nullptr && fs_count(fs_struct) > 0);
    return fs_struct->filemap;
}

void filemap_init(File *filemap){
    int fd;
    File *file = filemap;
    for(fd = 0; fd < FS_STRUCT_NENTRY; fd++, file++){
        atomic_set(&file->open_count, 0);
        file->status = FD_NONE, file->fd = fd;
        // sem_init(&file->file_sem, 1);
    }
}

static int filemap_alloc(int fd, File **file_store){
    File *file = get_filemap();
    if(fd == NO_FD){
        for(fd = 0; fd < FS_STRUCT_NENTRY; fd++, file++){
            if(file->status == FD_NONE){
                goto found;
            }
        }
        return -E_MAX_OPEN;
    }else{
        if(testfd(fd)){
            file += fd;
            if(file->status == FD_NONE){
                goto found;
            }
            return -E_BUSY;
        }
        return -E_INVAL;
    }
found:
    assert(fopen_count(file) == 0);
    file->status = FD_INIT, file->node = nullptr;
    *file_store = file;
    return 0;
}

static void filemap_free(File *file){
    assert(file->status == FD_INIT || file->status == FD_CLOSED);
    assert(fopen_count(file) == 0);
    if(file->status == FD_CLOSED){
        vfs_close(file->node);
    }
    file->status = FD_NONE;
}

void filemap_acquire(File *file){
    // down(&file->file_sem);
    assert(file->status == FD_OPENED);
    fopen_count_inc(file);
}

void filemap_release(File *file){
    assert(file->status == FD_OPENED || file->status == FD_CLOSED);
    assert(fopen_count(file) > 0);
    if(fopen_count_dec(file) == 0){
        filemap_free(file);
    }
    // up(&file->file_sem);
}

void filemap_open(File *file){
    assert(file->status == FD_INIT && file->node != nullptr);
    file->status = FD_OPENED;
    fopen_count_inc(file);
}

void filemap_close(File *file){
    assert(file->status == FD_OPENED);
    assert(fopen_count(file) > 0);
    file->status = FD_CLOSED;
    if(fopen_count_dec(file) == 0){
        filemap_free(file);
    }
}

void filemap_dup(File *to, File *from){
    assert(to->status == FD_INIT && from->status == FD_OPENED);
    to->pos = from->pos;
    to->readable = from->readable;
    to->writable = from->writable;
    Inode *node = from->node;
    vop_ref_inc(node), vop_open_inc(node);
    to->node = node;
    filemap_open(to);
}

int fd2file(int fd, File **file_store){
    if(testfd(fd)){
        File *file = get_filemap() + fd;
        if(file->status == FD_OPENED && file->fd == fd){
            *file_store = file;
            return 0;
        }
    }
    return -E_INVAL;
}

bool file_testfd(int fd, bool readable, bool writeable){
    int ret;
    File *file;
    if((ret = fd2file(fd, &file)) != 0){
        return 0;
    }
    if(readable && !file->readable){
        return 0;
    }
    if(writeable && !file->writable){
        return 0;
    }
    return 1;
}

int file_open(char *path, uint32_t open_flags){
    bool readable = 0, writable = 0;
    switch(open_flags & O_ACCMODE){
        case O_RDONLY:readable = true;break;
        case O_WRONLY:writable = true;break;
        case O_RDWR:
            readable = writable = true; break;
        default:
            return -E_INVAL;
    }
    int ret;
    File *file;
    if((ret = filemap_alloc(NO_FD, &file)) != 0){
        return ret;
    }
    Inode *node;
    if((ret = vfs_open(path, open_flags, &node)) != 0){
        filemap_free(file);
        return ret;
    }
    // sinode_dump_struct_lock();
    file->pos = 0;
    if(open_flags & O_APPEND){
        Stat __stat, *stat = &__stat;
        if((ret = vop_fstat(node, stat)) != 0){
            vfs_close(node);
            filemap_free(file);
            return ret;
        }
        file->pos = stat->st_size;
    }
    file->node = node;
    file->readable = readable;
    file->writable = writable;
    filemap_open(file);
    return file->fd;
}

int file_close(int fd){
    int ret;
    File *file;
    if((ret = fd2file(fd, &file)) != 0){
        return ret;
    }
    filemap_close(file);
    return 0;
}

int file_read(int fd, void *base, size_t len, size_t *copied_store){
    int ret;
    File *file;
    *copied_store = 0;
    if((ret = fd2file(fd, &file)) != 0){
        return ret;
    }
    if(!file->readable){
        return -E_INVAL;
    }
    filemap_acquire(file);
    Iobuf __iob, *iob = iobuf_init(&__iob, base, len, file->pos);
    ret = vop_read(file->node, iob);
    size_t copied = iobuf_used(iob);
    if(file->status == FD_OPENED){
        file->pos += copied;
    }
    *copied_store = copied;
    filemap_release(file);
    return ret;
}

int file_write(int fd, void *base, size_t len, size_t *copied_store) {
    int ret;
    File *file;
    *copied_store = 0;
    if ((ret = fd2file(fd, &file)) != 0) { return ret; }
    if (!file->writable) { return -E_INVAL; }
    if (file->status != FD_OPENED) { return -E_OPEN; }
    filemap_acquire(file);
    Iobuf __iob, *iob = iobuf_init(&__iob, base, len, file->pos);
    ret = vop_write(file->node, iob);
    size_t copied = iobuf_used(iob);
    if (file->status == FD_OPENED) { file->pos += copied; }
    *copied_store = copied;
    filemap_release(file);
    return ret;
}

int file_fstat(int fd, Stat *stat){
    int ret;
    File *file;
    if((ret = fd2file(fd, &file)) != 0){
        return ret;
    }
    filemap_acquire(file);
    ret = vop_fstat(file->node, stat);
    filemap_release(file);
    return ret;
}

int file_dup(int fd1, int fd2){
    int ret;
    File *file1, *file2;
    if((ret = fd2file(fd1, &file1)) != 0){
        return ret;
    }
    if((ret = filemap_alloc(fd2, &file2)) != 0){
        return ret;
    }
    filemap_dup(file2, file1);
    return file2->fd;
}

int file_pipe(int fd[]){
    int ret;
    File *file[2] = {nullptr, nullptr};
    if((ret = filemap_alloc(NO_FD, &file[0])) != 0){
        goto failed_cleanup;
    }
    if((ret = filemap_alloc(NO_FD, &file[1])) != 0){
        goto failed_cleanup;
    }
    if ((ret = pipe_open(&(file[0]->node), &(file[1]->node))) != 0) { goto failed_cleanup; }
    file[0]->pos = 0;
    file[0]->readable = 1, file[0]->writable = 0;
    filemap_open(file[0]);
    file[1]->pos = 0;
    file[1]->readable = 0, file[1]->writable = 1;
    filemap_open(file[1]);
    fd[0] = file[0]->fd, fd[1] = file[1]->fd;
    return 0;
failed_cleanup:
    if (file[0] != NULL) { filemap_free(file[0]); }
    if (file[1] != NULL) { filemap_free(file[1]); }
    return ret;
}

int file_mkfifo(const char *__name, uint32_t open_flags){
    bool readonly = 0;
    switch(open_flags & O_ACCMODE){
        case O_RDONLY:
            readonly = true;
        case O_WRONLY:
            break;
        default:
            return -E_INVAL;
    }
    int ret;
    File *file;
    if((ret = filemap_alloc(NO_FD, &file)) != 0){
        return ret;
    }
    char *name;
    const char *device = readonly? "pipe:r_" : "pipe:w_";
    if((name = stradd(device, __name)) == nullptr){
        ret = -E_NO_MEM;
        goto failed_cleanup_file;
    }
    if((ret = vfs_open(name, open_flags, &file->node)) != 0){
       goto failed_cleanup_name;
    }
    file->pos = 0;
    file->readable = readonly, file->writable = !readonly;
    filemap_open(file);
    kfree(name);
    return file->fd;
failed_cleanup_name:
    kfree(name);
failed_cleanup_file:
    filemap_free(file);
    return ret;
}

int file_seek(int fd, off_t pos, int whence){
    Stat __stat, *stat = &__stat;
    int ret;
    File *file;
    if((ret = fd2file(fd, &file)) != 0){
        return ret;
    }
    filemap_acquire(file);
    switch(whence){
        case LSEEK_SET:break;
        case LSEEK_CUR:pos += file->pos;break;
        case LSEEK_END:
            if((ret = vop_fstat(file->node, stat)) == 0){
                pos += stat->st_size;
            }
            break;
        default:ret = -E_INVAL;
    }
    if(ret == 0){
        if((ret = vop_tryseek(file->node, pos)) == 0){
            file->pos = pos;
        }
    }
    filemap_release(file);
    return ret;
}

int file_fsync(int fd){
    int ret;
    File *file;
    if((ret = fd2file(fd, &file)) != 0){
        return ret;
    }
    filemap_acquire(file);
    ret = vop_fsync(file->node);
    filemap_release(file);
    return ret;
}

int file_getdirentry(int fd, Dirent *dirent){
    int ret;
    File *file;
    if ((ret = fd2file(fd, &file)) != 0) { return ret; }
    filemap_acquire(file);
    Iobuf __iob, *iob;
try_again:
    iob = iobuf_init(&__iob, dirent->name, sizeof(dirent->name), dirent->offset);
    ret = vop_getdirentry(file->node, iob);
    if(ret == 0) {
        // dirent->offset += iobuf_used(iob);
        dirent->offset += sizeof(Sfs_dirent);
    }
    else if(ret == -E_NOENT){
        assert(iobuf_used(iob) == 0);
        dirent->offset += sizeof(Sfs_dirent);
        goto try_again;
    }
    filemap_release(file);
    return ret;
}