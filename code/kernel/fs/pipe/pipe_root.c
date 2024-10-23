#include "assert.h"
#include "error.h"
#include "inode.h"
#include "list.h"
#include "pipe.h"
#include "pipe_state.h"
#include "types.h"
#include "vfs.h"
#include "string.h"

static void lookup_pipe_nolock(Pipe_fs *pipe, const char *name, Inode **rnode_store, Inode **wnode_store) {
    *rnode_store = *wnode_store = nullptr;
    List_entry *le;
    list_for_each(le, &pipe->pipe_list) {
        Pipe_inode *pin = le2pin(le);
        if (strcmp(pin->name, name) == 0) {
            Inode *node = info2node(pin, pipe_inode);
            switch (pin->pin_type) {
                case PIN_RDONLY:
                    assert(*rnode_store == nullptr);
                    *rnode_store = node;
                    break;
                case PIN_WRONLY:
                    assert(*wnode_store == nullptr);
                    *wnode_store = node;
                    break;
                default: panic("unknown pipe_inode type %d.\n", pin->pin_type);
            }
            if(vop_ref_inc(node) == 1){
                pin->reclaim_count++;
            }
        }
    }
}

static int pipe_root_create(Inode *__node, const char *name, bool excl, Inode **node_store){
    assert((name[0] == 'r' || name[0] == 'w') && name[1] == '_');
    int ret = 0;
    bool readonly = (name[0] == 'r');
    name += 2;
    Inode *node[2];
    Fs *fs = vop_fs(__node);
    Pipe_fs *pipe = fsop_info(fs, pipe);
    lock_pipe(pipe);
    lookup_pipe_nolock(pipe, name, &node[0], &node[1]);
    if(!readonly){
        Inode *tmp = node[0];
        node[0] = node[1], node[1] = tmp;
    }
    if(node[0] != nullptr){
        if(excl){
            ret = -E_EXISTS;
            vop_ref_dec(node[0]);
            goto out;
        }
        *node_store = node[0];
        goto out;
    }
    ret = -E_NO_MEM;
    Pipe_state *state;
    if(node[1] == nullptr){
        if((state = pipe_state_create()) == nullptr){
            goto out;
        }
    }else{
        state = vop_info(node[1], pipe_inode)->state;
        pipe_state_acquire(state);
    }
    Inode *new_node;
    if((new_node = pipe_create_inode(fs, name, state, readonly)) == nullptr){
        pipe_state_release(state);
        goto out;
    }
    list_add(&pipe->pipe_list, &(vop_info(new_node, pipe_inode)->pipe_link));
    ret = 0, *node_store = new_node;
out:
    unlock_pipe(pipe);
    if(node[1] != nullptr){
        vop_ref_dec(node[1]);
    }
    return ret;
}

static int pipe_root_lookup(Inode *__node, char *path, Inode **node_store){
    assert((path[0] == 'r' || path[0] == 'w') && path[1] == '_');
    Inode *node[2];
    Pipe_fs *pipe = fsop_info(vop_fs(__node), pipe);
    lock_pipe(pipe);
    lookup_pipe_nolock(pipe, path + 2, &node[0], &node[1]);
    unlock_pipe(pipe);
    if(path[0] != 'r'){
        Inode *tmp = node[0];
        node[0] = node[1], node[1] = tmp;
    }
    if(node[1] != nullptr){
        vop_ref_dec(node[1]);
    }
    if(node[0] == nullptr){
        return -E_NOENT;
    }
    *node_store = node[0];
    return 0;
}

static int pipe_root_lookup_parent(Inode *node, char *path, Inode **node_store, char **endp){
    assert((path[0] == 'r' || path[0] == 'w') && path[1] == '_');
    *node_store = node, *endp = path;
    vop_ref_inc(node);
    return 0;
}

static const struct inode_ops pipe_root_ops = {
    .vop_magic = VOP_MAGIC,
    .vop_open = NULL_VOP_INVAL,
    .vop_close = NULL_VOP_INVAL,
    .vop_read = NULL_VOP_INVAL,
    .vop_write = NULL_VOP_INVAL,
    .vop_fstat = NULL_VOP_INVAL,
    .vop_fsync = NULL_VOP_INVAL,
    .vop_mkdir = NULL_VOP_INVAL,
    .vop_link = NULL_VOP_INVAL,
    .vop_rename = NULL_VOP_INVAL,
    .vop_readlink = NULL_VOP_INVAL,
    .vop_symlink = NULL_VOP_INVAL,
    .vop_namefile = NULL_VOP_INVAL,
    .vop_getdirentry = NULL_VOP_INVAL,
    .vop_reclaim = NULL_VOP_INVAL,
    .vop_ioctl = NULL_VOP_INVAL,
    .vop_gettype = NULL_VOP_INVAL,
    .vop_tryseek = NULL_VOP_INVAL,
    .vop_truncate = NULL_VOP_INVAL,
    .vop_create = pipe_root_create,
    .vop_unlink = NULL_VOP_INVAL,
    .vop_lookup = pipe_root_lookup,
    .vop_lookup_parent = pipe_root_lookup_parent,
};

Inode *pipe_create_root(Fs *fs){
    Inode *node;
    if((node = alloc_inode(pipe_root)) != nullptr){
        vop_init(node, &pipe_root_ops, fs);
    }
    return node;
}