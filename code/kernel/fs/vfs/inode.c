#include "inode.h"

#include "assert.h"
#include "error.h"
#include "slab.h"
#include "stdio.h"
#include "config.h"

Inode *__alloc_inode(int type) {
    Inode *node;
    if ((node = kmalloc(sizeof(Inode))) != nullptr) { node->in_type = type; }
    return node;
}

void inode_init(Inode *node, const Inode_ops *ops, Fs *fs) {
    atomic_set(&node->ref_count, 0);
    atomic_set(&node->open_count, 0);
    node->in_ops = ops, node->in_fs = fs;
    vop_ref_inc(node);
}

void inode_kill(Inode *node) {
    assert(inode_ref_count(node) == 0);
    assert(inode_open_count(node) == 0);
    kfree(node);
}

long inode_ref_inc(Inode *node) { return atomic_add_return(&node->ref_count, 1); }

long inode_ref_dec(Inode *node) {
    assert(inode_ref_count(node) > 0);
    long ref_count;
    int ret;
    if ((ref_count = atomic_sub_return(&node->ref_count, 1)) == 0) {
        if ((ret = vop_reclaim(node)) != 0 && ret != -E_BUSY) {
            cprintf("vfs: warning: vop_reclaim: %e.\n", ret);
        }
    }
    return ref_count;
}

long inode_open_inc(Inode *node) { return atomic_add_return(&node->open_count, 1); }

long inode_open_dec(Inode *node) {
    assert(inode_open_count(node) > 0);
    long open_count;
    int ret;
    if ((open_count = atomic_sub_return(&node->open_count, 1)) == 0) {
        if ((ret = vop_close(node)) != 0) { cprintf("vfs: warning: vop_close: %e.\n", ret); }
    }
    return open_count;
}

void inode_check(Inode *node) {
    assert(node != NULL && node->in_ops != NULL);
    assert(node->in_ops->vop_magic == VOP_MAGIC);
    int ref_count = inode_ref_count(node), open_count = inode_open_count(node);
    assert(ref_count >= open_count && open_count >= 0);
    assert(ref_count < MAX_INODE_COUNT && open_count < MAX_INODE_COUNT);
}

int null_vop_pass(void) { return 0; }

int null_vop_inval(void) { return -E_INVAL; }

int null_vop_unimp(void) { return -E_UNIMP; }

int null_vop_isdir(void) { return -E_ISDIR; }

int null_vop_notdir(void) { return -E_NOTDIR; }
