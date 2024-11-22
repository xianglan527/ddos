#include "inode.h"

#include "assert.h"
#include "config.h"
#include "error.h"
#include "proc.h"
#include "slab.h"
#include "stdio.h"

Inode *__alloc_inode(int type) {
    Inode *node;
    if ((node = kmalloc(sizeof(Inode))) != nullptr) { node->in_type = type; }
    return node;
}

void inode_init(Inode *node, const Inode_ops *ops, Fs *fs) {
    node->in_ops = ops, node->in_fs = fs;
    atomic_set(&node->ref_count, 0);
    atomic_set(&node->open_count, 0);
    // sem_init(&node->count_sem, 1);
    sem_init(&node->sem, 1);
    // initlock(&node->count_sem, "a");
    // vop_ref_inc(node);
    // node->ref_cnt++;
    atomic_inc(&node->ref_count);
}

void nodes_lock(Inode *node) {
    if (node->in_type == __in_type(sfs_inode)) {
        Sfs_fs *sfs = fsop_info(vop_fs(node), sfs);
        lock_sfs_inodes(sfs);
    } else if (node->in_type == __in_type(pipe_inode)) {
        Pipe_fs *pipe = fsop_info(vop_fs(node), pipe);
        lock_pipe(pipe);
    } else {
        return;  // Other types of nodes do not experience contention, so there is no need to lock it.
    }
}

void nodes_unlock(Inode *node) {
    if (node->in_type == __in_type(sfs_inode)) {
        Sfs_fs *sfs = fsop_info(vop_fs(node), sfs);
        unlock_sfs_inodes(sfs);
    } else if (node->in_type == __in_type(pipe_inode)) {
        Pipe_fs *pipe = fsop_info(vop_fs(node), pipe);
        unlock_pipe(pipe);
    } else {
        return;
    }
}


void inode_kill(Inode *node) {
    assert(atomic_read(&node->ref_count) == 0);
    assert(atomic_read(&node->open_count) == 0);
    nodes_unlock(node);
    kfree(node);
}

long inode_ref_inc(Inode *node) {
    long count;
    nodes_lock(node);
    count = atomic_add_return(&node->ref_count, 1);
    nodes_unlock(node);
    return count;
}

long inode_ref_dec(Inode *node) {
    long count;
    nodes_lock(node);
    assert(atomic_read(&node->ref_count) > 0);
    count = atomic_sub_return(&node->ref_count, 1);
    if (count == 0) {
        assert(vop_reclaim(node) == 0);
        return 0;
    }
    nodes_unlock(node);
    return count;
}

long inode_open_inc(Inode *node) {
    long count;
    nodes_lock(node);
    count = atomic_add_return(&node->open_count, 1);
    nodes_unlock(node);
    return count;
}

long inode_open_dec(Inode *node) {
    long count;
    nodes_lock(node);
    assert(atomic_read(&node->open_count) > 0);
    count = atomic_sub_return(&node->open_count, 1);
    if (count == 0) { assert(vop_close(node) == 0); }
    nodes_unlock(node);
    return count;
}

void inode_check(Inode *node) {
    assert(node != NULL && node->in_ops != NULL);
    assert(node->in_ops->vop_magic == VOP_MAGIC);
    int ref_count = inode_ref_count(node);
    int open_count = inode_open_count(node);
    if (!(ref_count >= open_count && open_count >= 0)) { int pp = 3; }
    assert(ref_count >= open_count && open_count >= 0);
    assert(ref_count < MAX_INODE_COUNT && open_count < MAX_INODE_COUNT);
}

int null_vop_pass(void) { return 0; }

int null_vop_inval(void) { return -E_INVAL; }

int null_vop_unimp(void) { return -E_UNIMP; }

int null_vop_isdir(void) { return -E_ISDIR; }

int null_vop_notdir(void) { return -E_NOTDIR; }
