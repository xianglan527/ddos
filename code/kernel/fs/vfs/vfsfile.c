#include "assert.h"
#include "error.h"
#include "inode.h"
#include "proc.h"
#include "stdio.h"
#include "string.h"
#include "sysdef.h"
#include "vfs.h"

extern int sfs_read_nolock(Inode *inode, void *dst, off_t off, size_t len, size_t *len_store);
extern void sinode_lock(Inode *inode);
extern void sinode_unlock(Inode *inode);
extern void sinode_unlock_put(Inode *inode);

int vfs_open(char *path, uint32_t open_flags, Inode **node_store) {
    bool can_write = 0;
    switch (open_flags & O_ACCMODE) {
        case O_RDONLY: break;
        case O_WRONLY:
        case O_RDWR: can_write = 1; break;
        default: return -E_INVAL;
    }
    if (open_flags & O_TRUNC) {
        if (!can_write) { return -E_INVAL; }
    }
    int ret;
    Inode *dir, *node;
    if (open_flags & O_CREAT) {
        char *name;
        bool excl = (open_flags & O_EXCL) != 0;
        if ((ret = vfs_lookup_parent(path, &dir, &name)) != 0) { return ret; }
        ret = vop_create(dir, name, excl, &node);
        vop_ref_dec(dir);
    } else {
        ret = vfs_lookup(path, &node);
    }
    if (ret != 0) { return ret; }
    assert(node != nullptr);
    if (node->in_type == __in_type(sfs_inode) && vop_info(node, sfs_inode)->din->type == SFS_TYPE_LINK) {
        Sfs_inode *sin = vop_info(node, sfs_inode);
        sinode_lock(node);
        if (!(open_flags & O_NOFOLLOW)) {
            int cycle = 0;
            char target[MAXPATH];
            while (sin->din->type == SFS_TYPE_LINK) {
                if (cycle == SFS_MAX_SYMLINK_CYCLE) {
                    sinode_unlock_put(node);
                    return -E_INVAL;
                }
                cycle++;
                memset(target, 0, sizeof(target));
                if ((ret = sfs_read_nolock(node, (void *)target, 0, MAXPATH, nullptr)) != 0) {
                    sinode_unlock_put(node);
                    return ret;
                }
                sinode_unlock_put(node);
                if ((ret = vfs_lookup(target, &node)) != 0) { return ret; }
                sinode_lock(node);
                sin = vop_info(node, sfs_inode);
            }
        }
        sinode_unlock(node);
    }
    if ((ret = vop_open(node, open_flags)) != 0) {
        vop_ref_dec(node);
        return ret;
    }
    vop_open_inc(node);
    if (open_flags & O_TRUNC) {
        if ((ret = vop_truncate(node, 0)) != 0) {
            vop_open_dec(node);
            vop_ref_dec(node);
            return ret;
        }
    }
    *node_store = node;
    return 0;
}

int vfs_close(Inode *node) {
    vop_open_dec(node);
    vop_ref_dec(node);
    return 0;
}

int vfs_unlink(char *path) {
    int ret;
    char *name;
    Inode *dir;
    if ((ret = vfs_lookup_parent(path, &dir, &name)) != 0) { return ret; }
    ret = vop_unlink(dir, name);
    vop_ref_dec(dir);
    return ret;
}

int vfs_rename(char *old_path, char *new_path) {
    int ret;
    char *old_name, *new_name;
    Inode *old_dir, *new_dir;
    if ((ret = vfs_lookup_parent(old_path, &old_dir, &old_name)) != 0) { return ret; }
    if ((ret = vfs_lookup_parent(new_path, &new_dir, &new_name)) != 0) {
        vop_ref_dec(old_dir);
        return ret;
    }
    if (old_dir->in_fs == nullptr || old_dir->in_fs != new_dir->in_fs) {
        ret = -E_XDEV;
    } else {
        ret = vop_rename(old_dir, old_name, new_dir, new_name);
    }
    vop_ref_dec(old_dir);
    vop_ref_dec(new_dir);
    return ret;
}

int vfs_link(char *old_path, char *new_path) {
    int ret;
    char *new_name;
    Inode *old_node, *new_dir;
    if ((ret = vfs_lookup(old_path, &old_node)) != 0) { return ret; }
    if ((ret = vfs_lookup_parent(new_path, &new_dir, &new_name)) != 0) {
        vop_ref_dec(old_node);
        return ret;
    }
    // cprintf("11111 current pid is %d old_path is %s, new_path is %s. old_node ref is %d\n", myproc()->pid,
    //         old_path, new_path, atomic_read(&old_node->ref_count));
    if (old_node->in_fs == nullptr || old_node->in_fs != new_dir->in_fs) {
        ret = -E_XDEV;
    } else {
        ret = vop_link(new_dir, new_name, old_node);
    }
    vop_ref_dec(old_node);
    vop_ref_dec(new_dir);
    return ret;
}

int vfs_symlink(char *old_path, char *new_path) {
    int ret;
    char *new_name;
    Inode *new_dir;
    if ((ret = vfs_lookup_parent(new_path, &new_dir, &new_name)) != 0) { return ret; }
    ret = vop_symlink(new_dir, new_name, old_path);
    vop_ref_dec(new_dir);
    return ret;
}

int vfs_readlink(char *path, Iobuf *iob) {
    int ret;
    Inode *node;
    if ((ret = vfs_lookup(path, &node)) != 0) { return ret; }
    ret = vop_readlink(node, iob);
    vop_ref_dec(node);
    return ret;
}

int vfs_mkdir(char *path) {
    int ret;
    char *name;
    Inode *dir;
    if ((ret = vfs_lookup_parent(path, &dir, &name)) != 0) { return ret; }
    ret = vop_mkdir(dir, name);
    vop_ref_dec(dir);
    return ret;
}
