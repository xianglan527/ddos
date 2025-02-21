#include "fs.h"

#include <sfs.h>

#include "assert.h"
#include "dev.h"
#include "error.h"
#include "inode.h"
#include "pipe.h"
#include "sem.h"
#include "slab.h"
#include "stdio.h"
#include "string.h"
#include "vfs.h"

void fs_init(void){
    vfs_init();
    dev_init();
    pipe_init();
    sfs_init();
}

void fs_cleanup(void){
    vfs_cleanup();
}

void lock_fs(Fs_struct *fs_struct){
    down(&fs_struct->fs_sem);
}

void unlock_fs(Fs_struct *fs_struct){
    up(&fs_struct->fs_sem);
}

Fs_struct *fs_create(void){
    assert(FS_STRUCT_NENTRY > 128);
    Fs_struct *fs_struct;
    if((fs_struct = kmalloc(sizeof(Fs_struct) + FS_STRUCT_BUFSIZE)) != nullptr){
        fs_struct->pwd = nullptr;
        fs_struct->filemap = (void*)(fs_struct + 1);
        atomic_set(&fs_struct->fs_count, 0);
        sem_init(&fs_struct->fs_sem, 1);
        filemap_init(fs_struct->filemap);
    }
    return fs_struct;
}

void fs_destroy(Fs_struct *fs_struct){
    assert(fs_struct != nullptr && fs_count(fs_struct) == 0);
    if(fs_struct->pwd != nullptr){
        vop_ref_dec(fs_struct->pwd);
    }
    int i;
    File *file = fs_struct->filemap;
    for(i = 0; i < FS_STRUCT_NENTRY;i++, file++){
        if(file->status == FD_OPENED){
            filemap_close(file);
        }
        assert(file->status == FD_NONE);
    }
    kfree(fs_struct);
}

int dup_fs(Fs_struct *to, Fs_struct *from){
    assert(to != nullptr && from != nullptr);
    assert(fs_count(to) == 0 && fs_count(from) > 0);
    if((to->pwd = from->pwd) != nullptr){
        vop_ref_inc(to->pwd);
    }
    int i;
    File *to_file = to->filemap, *from_file = from->filemap;
    for(i = 0; i < FS_STRUCT_NENTRY; i++, to_file++, from_file++){
        if(from_file->status == FD_OPENED){
            to_file->status = FD_INIT;
            filemap_dup(to_file, from_file);
        }
    }
    return 0;
}