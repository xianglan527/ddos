#include "vfs.h"

#include "assert.h"
#include "dev.h"
#include "error.h"
#include "inode.h"
#include "sem.h"
#include "stdio.h"
#include "string.h"
#include "slab.h"

static Sem bootfs_sem;
Inode *bootfs_node = nullptr;

extern void vfs_devlist_init(void);

Fs *__alloc_fs(int type){
    Fs *fs;
    if((fs = kmalloc(sizeof(Fs))) != nullptr){
        fs->fs_type = type;
    }
    return fs;
}

void vfs_init(void){
    sem_init(&bootfs_sem, 1);
    vfs_devlist_init();
}

static void lock_bootfs(void){
    down(&bootfs_sem);
}

static void unlock_bootfs(void){
    up(&bootfs_sem);
}

static void change_bootfs(Inode *node){
    Inode *old;
    lock_bootfs();
    {
        old = bootfs_node, bootfs_node = node;
    }
    unlock_bootfs();
    if(old != nullptr){
        vop_ref_dec(old);
    }
}

int vfs_set_bootfs(char *fsname){
    Inode *node = nullptr;
    if(fsname != nullptr){
        char *s;
        if((s = strchr(fsname, ':')) == nullptr || s[1] != '\0'){
            return -E_INVAL;
        }
        int ret;
        if((ret = vfs_chdir(fsname)) != 0){
            return ret;
        }
        if((ret = vfs_get_curdir(&node)) != 0){
            return ret;
        }
    }
    change_bootfs(node);
    return 0;
}

int vfs_get_bootfs(Inode **node_store){
    Inode *node = nullptr;
    if(bootfs_node != nullptr){
        lock_bootfs();
        {
            if((node = bootfs_node) != nullptr){
                vop_ref_inc(bootfs_node);
            }
        }
        unlock_bootfs();
    }
    if(node == nullptr){
        return -E_NOENT;
    }
    *node_store = node;
    return 0;
}