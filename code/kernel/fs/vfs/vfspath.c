#include "assert.h"
#include "error.h"
#include "inode.h"
#include "stdio.h"
#include "vfs.h"
#include "proc.h"
#include "string.h"

static Inode *get_cwd_nolock(void){
    return myproc()->fs_struct->pwd;
}

static void set_cwd_nolock(Inode *pwd){
    myproc()->fs_struct->pwd = pwd;
}

static void lock_cfs(void){
    lock_fs(myproc()->fs_struct);
}

static void unlock_cfs(void){
    unlock_fs(myproc()->fs_struct);
}

int vfs_get_curdir(Inode **dir_store){
    Inode *node;
    if((node = get_cwd_nolock()) != nullptr){
        vop_ref_inc(node);
        *dir_store = node;
        return 0;
    }
    return -E_NOENT;
}

int vfs_set_curdir(Inode *dir){
    int ret = 0;
    lock_cfs();
    Inode *old_dir;
    if((old_dir = get_cwd_nolock()) != dir){
        if(dir != nullptr){
            uint32_t type;
            if((ret = vop_gettype(dir, &type)) != 0){
                goto out;
            }
            if(!S_ISDIR(type)){
                ret = -E_NOTDIR;
                goto out;
            }
            vop_ref_inc(dir);
        }
        set_cwd_nolock(dir);
        if(old_dir != nullptr){
            vop_ref_dec(old_dir);
        }
    }
out:
    unlock_cfs();
    return ret;
}

int vfs_chdir(char *path){
    int ret;
    Inode *node;
    if((ret = vfs_lookup(path, &node)) == 0){
        ret = vfs_set_curdir(node);
        vop_ref_dec(node);
    }
    return ret;
}

int vfs_getcwd(Iobuf *iob){
    int ret;
    Inode *node;
    if((ret = vfs_get_curdir(&node)) != 0){
        return ret;
    }
    assert(node->in_fs != nullptr);
    const char *devname = vfs_get_devname(node->in_fs);
    if((ret = iobuf_move(iob, (char *)devname, strlen(devname), 1, nullptr)) != 0){
        goto out;
    }
    char colon = ':';
    if((ret = iobuf_move(iob, &colon, sizeof(colon), 1, nullptr)) != 0){
        goto out;
    }
    ret = vop_namefile(node, iob);
out:
    vop_ref_dec(node);
    return ret;
}