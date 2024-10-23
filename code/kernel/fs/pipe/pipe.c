#include "config.h"
#include "sem.h"
#include "assert.h"
#include "types.h"
#include "wait.h"
#include "pipe.h"
#include "vfs.h"
#include "inode.h"
#include "error.h"
#include "list.h"

void lock_pipe(Pipe_fs *pipe){
    down(&pipe->pipe_sem);
}

void unlock_pipe(Pipe_fs *pipe){
    up(&pipe->pipe_sem);
}

static int pipe_sync(Fs *fs){
    return 0;
}

static Inode *pipe_get_root(Fs *fs){
    Pipe_fs *pipe = fsop_info(fs, pipe);
    vop_ref_inc(pipe->root);
    return pipe->root;
}

static int pipe_unmount(Fs *fs){
    return -E_INVAL;
}

static void pipe_cleanup(Fs *fs){
    /*do nothing*/
}

static void pipe_fs_init(Fs *fs){
    Pipe_fs *pipe = fsop_info(fs, pipe);
    if((pipe->root = pipe_create_root(fs)) == nullptr){
     panic("pipe: create root inode failed.\n");
    }
    sem_init(&pipe->pipe_sem, 1);
    list_init(&pipe->pipe_list);
    fs->fs_sync = pipe_sync;
    fs->fs_get_root = pipe_get_root;
    fs->fs_unmount = pipe_unmount;
    fs->fs_cleanup = pipe_cleanup;
}

void pipe_init(void){
    Fs *fs;
    if((fs = alloc_fs(pipe)) == nullptr){
         panic("pipe: create pipe_fs failed.\n");
    }
    pipe_fs_init(fs);
    int ret;
    if((ret = vfs_add_fs("pipe", fs)) != 0){
         panic("pipe: vfs_add_fs: %e.\n", ret);
    }
}