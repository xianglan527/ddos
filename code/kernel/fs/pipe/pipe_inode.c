#include "assert.h"
#include "error.h"
#include "inode.h"
#include "list.h"
#include "pipe.h"
#include "pipe_state.h"
#include "types.h"
#include "vfs.h"
#include "slab.h"
#include "sysdef.h"
#include "iobuf.h"
#include "string.h"

static int pipe_inode_open(Inode *node, uint32_t open_flags){
    if(open_flags & (O_TRUNC | O_APPEND)){
        return -E_INVAL;
    }
    Pipe_inode *pin = vop_info(node, pipe_inode);
    switch(open_flags & O_ACCMODE){
        case O_RDONLY:
            return (pin->pin_type == PIN_RDONLY)? 0 : -E_INVAL;
        case O_WRONLY:
            return (pin->pin_type == PIN_WRONLY)? 0 : -E_INVAL;
        default:
            return -E_INVAL;
    }
}

static int pipe_inode_close(Inode *node){
    Pipe_inode *pin = vop_info(node, pipe_inode);
    pipe_state_close(pin->state);
    return 0;
}

static int pipe_inode_read(Inode *node, Iobuf *iob){
    Pipe_inode *pin = vop_info(node, pipe_inode);
    if(pin->pin_type != PIN_RDONLY){
        return -E_INVAL;
    }
    size_t ret;
    if((ret = pipe_state_read(pin->state, iob->io_base, iob->io_resid)) != 0){
        iobuf_skip(iob, ret);
    }
    return 0;
}

static int pipe_inode_write(Inode *node, Iobuf *iob){
    Pipe_inode *pin = vop_info(node, pipe_inode);
    if(pin->pin_type != PIN_WRONLY){
        return -E_INVAL;
    }
    size_t ret;
    if((ret = pipe_state_write(pin->state, iob->io_base, iob->io_resid)) != 0){
        iobuf_skip(iob, ret);
    }
    return 0;
}

static int pipe_inode_fstat(Inode *node, Stat *stat){
    int ret;
    memset(stat, 0, sizeof(struct stat));
    if((ret = vop_gettype(node, &stat->st_mode)) != 0){
        return ret;
    }
    Pipe_inode *pin = vop_info(node, pipe_inode);
    stat->st_nlinks = 1;
    stat->st_blocks = 0;
    stat->st_size = pipe_state_size(pin->state, pin->pin_type == PIN_WRONLY);
    return 0;
}

static int pipe_inode_namefile(Inode *node, Iobuf *iob){
    Pipe_inode *pin = vop_info(node, pipe_inode);
    size_t len = (pin->name != nullptr)? strlen(pin->name) : 0;
    if(iob->io_resid < len  + 1){
        return -E_NO_MEM;
    }
    if(pin->name != nullptr){
        memcpy(iob->io_base, pin->name, len);
    }
    ((char *)iob->io_base)[len++] = '\0';
    iobuf_skip(iob, len);
    return 0;
}

static int pipe_inode_reclaim(Inode *node){
    Pipe_inode *pin = vop_info(node, pipe_inode);
    if(pin->name != nullptr){
        Pipe_fs *pipe = fsop_info(vop_fs(node), pipe);
        lock_pipe(pipe);
        assert(pin->reclaim_count > 0);
        if((--pin->reclaim_count) != 0 || inode_ref_count(node) != 0){
            unlock_pipe(pipe);
            return -E_BUSY;
        }
        list_del(&pin->pipe_link);
        unlock_pipe(pipe);
        kfree(pin->name);
    }
    pipe_state_release(pin->state);
    vop_kill(node);
    return 0;
}

static int pipe_inode_gettype(Inode *node, uint32_t *type_store){
    *type_store = S_IFCHR;
    return 0;
}

static const struct inode_ops pipe_node_ops = {
    .vop_magic = VOP_MAGIC,
    .vop_open = pipe_inode_open,
    .vop_close = pipe_inode_close,
    .vop_read = pipe_inode_read,
    .vop_write = pipe_inode_write,
    .vop_fstat = pipe_inode_fstat,
    .vop_fsync = NULL_VOP_PASS,
    .vop_mkdir = NULL_VOP_NOTDIR,
    .vop_link = NULL_VOP_NOTDIR,
    .vop_rename = NULL_VOP_NOTDIR,
    .vop_readlink = NULL_VOP_INVAL,
    .vop_symlink = NULL_VOP_NOTDIR,
    .vop_namefile = pipe_inode_namefile,
    .vop_getdirentry = NULL_VOP_INVAL,
    .vop_reclaim = pipe_inode_reclaim,
    .vop_ioctl = NULL_VOP_INVAL,
    .vop_gettype = pipe_inode_gettype,
    .vop_tryseek = NULL_VOP_INVAL,
    .vop_truncate = NULL_VOP_INVAL,
    .vop_create = NULL_VOP_NOTDIR,
    .vop_unlink = NULL_VOP_NOTDIR,
    .vop_lookup = NULL_VOP_NOTDIR,
    .vop_lookup_parent = NULL_VOP_NOTDIR,
};

static void pipe_inode_init(Pipe_inode *pin, char *name, Pipe_state *state, bool readonly){
    assert(state != nullptr);
    pin->pin_type = readonly ? PIN_RDONLY : PIN_WRONLY;
    pin->name = name, pin->state = state, pin->reclaim_count = 1;
    list_init(&pin->pipe_link);
}

struct inode *pipe_create_inode(Fs *fs, const char *__name, Pipe_state *state, bool readonly){
    char *name = nullptr;
    if(__name == nullptr || (name = strdup(__name)) != nullptr){
        Inode *node;
        if((node = alloc_inode(pipe_inode)) != nullptr){
            vop_init(node, &pipe_node_ops, fs);
            pipe_inode_init(vop_info(node, pipe_inode), name, state, readonly);
            return node;
        }
        if(name != nullptr){
            kfree(name);
        }
    }
    return nullptr;
}

int pipe_open(Inode **rnode_store, Inode **wnode_store){
    int ret;
    Inode *root;
    if((ret = vfs_get_root("pipe", &root)) != 0){
        return ret;
    }
    ret = -E_NO_MEM;
    Pipe_state *state;
    if((state = pipe_state_create()) == nullptr){
        goto out;
    }
    Fs *fs = vop_fs(root);
    Inode *node[2] = {nullptr, nullptr};
    if((node[0] = pipe_create_inode(fs, nullptr, state, 1)) == nullptr){
        goto failed_cleanup_state;
    }
    pipe_state_acquire(state);
    if((node[1] = pipe_create_inode(fs, nullptr, state, 0)) == nullptr){
        goto failed_cleaup_node0;
    }
    vop_open_inc(node[0]), vop_open_inc(node[1]);
    *rnode_store = node[0];
    *wnode_store = node[1];
    ret = 0;
out:
    vop_ref_dec(root);
    return ret;
failed_cleaup_node0:
    vop_ref_dec(node[0]);
failed_cleanup_state:
    pipe_state_release(state);
    goto out;
}