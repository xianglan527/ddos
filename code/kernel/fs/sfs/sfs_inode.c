#include "assert.h"
#include "bitmap.h"
#include "dev.h"
#include "error.h"
#include "fs.h"
#include "inode.h"
#include "iobuf.h"
#include "list.h"
#include "sfs.h"
#include "slab.h"
#include "stdio.h"
#include "string.h"
#include "types.h"
#include "vfs.h"
#include "proc.h"

static void buf_zero(Sfs_fs *sfs, uint blockno) {
    Disk_buf *buf = buf_read(sfs, blockno);
    memset(buf->data, 0, SFS_BSIZE);
    buf_write(buf);
    buf_release(sfs, buf);
}

static uint blk_alloc(Sfs_fs *sfs) {
    uint blk, bit, index;
    Disk_buf *buf = nullptr;
    for (blk = 0; blk < sfs->sb.size; blk += SFS_BLKBITS) {
        buf = buf_read(sfs, block2map_block(blk, sfs->sb));
        for (bit = 0; bit < SFS_BLKBITS && blk + bit < sfs->sb.size; bit++) {
            index = 1 << (bit % 8);
            if ((buf->data[bit / 8] & index) == 0) {
                buf->data[bit / 8] |= index;
                buf_write(buf);
                buf_release(sfs, buf);
                buf_zero(sfs, blk + bit);
                return blk + bit;
            }
        }
    }
    buf_release(sfs, buf);
    panic("blk_alloc: out of blocks.\n");
    return 0;
}

static void blk_free(Sfs_fs *sfs, uint blockno) {
    uint bit, index;
    Disk_buf *buf = buf_read(sfs, block2map_block(blockno, sfs->sb));
    bit = blockno % SFS_BLKBITS;
    index = 1 << (bit % 8);
    if ((buf->data[bit / 8] & index) == 0) panic("freeing free block.\n");
    buf->data[bit / 8] &= ~index;
    buf_write(buf);
    buf_release(sfs, buf);
}

static const struct inode_ops sfs_node_dirops;

static const struct inode_ops sfs_node_fileops;

static const Inode_ops *sfs_get_ops(ushort type) {
    switch (type) {
        case SFS_TYPE_DIR: return &sfs_node_dirops;
        case SFS_TYPE_FILE: 
        case SFS_TYPE_LINK:
        return &sfs_node_fileops;
    }
    panic("invalid file type %d.\n", type);
    return nullptr;
}

static List_entry *sfs_hash_list(Sfs_fs *sfs, uint32_t ino) { return sfs->hash_list + inode_hashfn(ino); }

static void sfs_set_links(Sfs_fs *sfs, Sfs_inode *sin) {
    list_add(&sfs->inode_list, &sin->inode_link);
    list_add(sfs_hash_list(sfs, sin->ino), &sin->hash_link);
}

static void sfs_remove_links(Sfs_inode *sin) {
    list_del(&sin->inode_link);
    list_del(&sin->hash_link);
}

static int sfs_create_inode(Sfs_fs *sfs, Dinode *din, uint ino, Inode **node_store) {
    Inode *inode;
    if ((inode = alloc_inode(sfs_inode)) != nullptr) {
        vop_init(inode, sfs_get_ops(din->type), info2fs(sfs, sfs));
        Sfs_inode *sin = vop_info(inode, sfs_inode);
        sin->din = din, sin->ino = ino, sin->valid = false;
        *node_store = inode;
        return 0;
    }
    return -E_NO_MEM;
}

static Inode *lookup_sinode_nolock(Sfs_fs *sfs, uint ino) {
    Inode *inode;
    List_entry *le;
    list_for_each(le, sfs_hash_list(sfs, ino)) {
        Sfs_inode *sin = le2sfs_inode(le, hash_link);
        if (sin->ino == ino) {
            inode = info2node(sin, sfs_inode);
            atomic_inc(&inode->ref_count);
            return inode;
        }
    }
    return nullptr;
}

Inode *get_inode(Sfs_fs *sfs, uint ino) {
    lock_sfs_inodes(sfs);
    Inode *inode;
    if ((inode = lookup_sinode_nolock(sfs, ino)) != nullptr) {
        unlock_sfs_inodes(sfs);
        return inode;
    }
    Dinode *din;
    if ((din = kmalloc(sizeof(Dinode))) == nullptr) { goto failed1; }
    Disk_buf *buf;
    buf = buf_read(sfs, inum2block(ino, sfs->sb));
    Dinode *dinp = (Dinode *)buf->data + ino % inodes_per_block;
    *din = *dinp;
    buf_release(sfs, buf);
    assert(din->nlink != 0);
    if (sfs_create_inode(sfs, din, ino, &inode) != 0) { goto failed2; }
    vop_info(inode, sfs_inode)->valid = true;
    sfs_set_links(sfs, vop_info(inode, sfs_inode));
    unlock_sfs_inodes(sfs);
    return inode;
failed2:
    kfree(din);
failed1:
    unlock_sfs_inodes(sfs);
    return nullptr;
}

static Inode *inode_alloc(Sfs_fs *sfs, ushort type) {
    uint inum;
    Disk_buf *buf;
    Dinode *din;
    for (inum = 1; inum < sfs->sb.ninodes; inum++) {
        buf = buf_read(sfs, inum2block(inum, sfs->sb));
        din = (Dinode *)buf->data + inum % inodes_per_block;
        if (din->type == 0) {
            memset(din, 0, sizeof(*din));
            din->type = type;
            din->size = 0;
            din->nlink = 1;
            buf_write(buf);
            buf_release(sfs, buf);
            return get_inode(sfs, inum);
        }
        buf_release(sfs, buf);
    }
    panic("inode_alloc: no inodes.\n");
    return nullptr;
}

static void sinode_updata_nolock(Sfs_fs *sfs, Sfs_inode *sin, int mem2disk) {
    if (sin->valid == true) return;
    Disk_buf *buf;
    Dinode *din;
    buf = buf_read(sfs, inum2block(sin->ino, sfs->sb));
    din = (Dinode *)buf->data + sin->ino % inodes_per_block;
    if (mem2disk == MEM2DISK) {
        *din = *sin->din;
        buf_write(buf);
    } else {
        *sin->din = *din;
    }
    buf_release(sfs, buf);
    sin->valid = true;
}

void sinode_lock(Inode *inode) {
    Sfs_fs *sfs = fsop_info(vop_fs(inode), sfs);
    Sfs_inode *sin = vop_info(inode, sfs_inode);
    if (inode == nullptr /*|| atomic_read(&inode->ref_count) < 1*/) { panic("sinode_lock"); }
    down(&inode->sem);
    sinode_updata_nolock(sfs, sin, DISK2MEM);
    if (sin->din->type == 0) { panic("sinode_lock: no type"); }
}

void sinode_unlock(Inode *inode) {
    Sfs_fs *sfs = fsop_info(vop_fs(inode), sfs);
    Sfs_inode *sin = vop_info(inode, sfs_inode);
    if (inode == nullptr /*|| atomic_read(&inode->ref_count) < 1*/) { panic("sinode_unlock"); }
    up(&inode->sem);
}

static int sfs_fsync(Inode *inode) {
    Sfs_fs *sfs = fsop_info(vop_fs(inode), sfs);
    Sfs_inode *sin = vop_info(inode, sfs_inode);
    if (sin->din->nlink == 0 || sin->valid == true) { return 0; }
    sinode_lock(inode);
    sinode_updata_nolock(sfs, sin, MEM2DISK);
    sinode_unlock(inode);
    return 0;
}

static uint bmap_load_nolock(Sfs_fs *sfs, Sfs_inode *sin, uint bn) {
    uint addr, *data;
    Disk_buf *buf;
    if (bn < SFS_NDIRECT) {
        if ((addr = sin->din->addrs[bn]) == 0) { sin->din->addrs[bn] = addr = blk_alloc(sfs); }
        sin->valid = false;
        return addr;
    }
    bn -= SFS_NDIRECT;
    if (bn < SFS_NINDIRECT) {
        if ((addr = sin->din->addrs[SFS_NDIRECT]) == 0) {
            sin->din->addrs[SFS_NDIRECT] = addr = blk_alloc(sfs);
            sin->valid = false;
        }
        buf = buf_read(sfs, addr);
        data = (uint *)buf->data;
        if ((addr = data[bn]) == 0) {
            data[bn] = addr = blk_alloc(sfs);
            buf_write(buf);
        }
        buf_release(sfs, buf);
        return addr;
    }
    bn -= SFS_NINDIRECT;
    if (bn < SFS_NINDIRECT * SFS_NINDIRECT) {
        int idx = bn / SFS_NINDIRECT;
        int off = bn % SFS_NINDIRECT;
        if ((addr = sin->din->addrs[SFS_NDIRECT + 1]) == 0) {
            sin->din->addrs[SFS_NDIRECT + 1] = addr = blk_alloc(sfs);
            sin->valid = false;
        }
        buf = buf_read(sfs, addr);
        data = (uint *)buf->data;
        if ((addr = data[idx]) == 0) {
            data[idx] = addr = blk_alloc(sfs);
            buf_write(buf);
        }
        buf_release(sfs, buf);
        buf = buf_read(sfs, addr);
        data = (uint *)buf->data;
        if ((addr = data[off]) == 0) {
            data[off] = addr = blk_alloc(sfs);
            buf_write(buf);
        }
        buf_release(sfs, buf);
        return addr;
    } else {
        panic("sfs_bmap_load_nolock: out of range");
        return 0;
    }
}

static void bmap_free_nolock(Sfs_fs *sfs, Sfs_inode *sin, uint bn) {
    uint addr, *data;
    Disk_buf *buf;
    if (bn < SFS_NDIRECT) {
        if (sin->din->addrs[bn] != 0) {
            blk_free(sfs, sin->din->addrs[bn]);
            sin->din->addrs[bn] = 0;
            sin->valid = false;
        }
        return;
    }
    bn -= SFS_NDIRECT;
    if (bn < SFS_NINDIRECT) {
        assert((addr = sin->din->addrs[SFS_NDIRECT]) != 0);
        buf = buf_read(sfs, addr);
        data = (uint *)buf->data;
        if (data[bn] != 0) {
            blk_free(sfs, data[bn]);
            data[bn] = 0;
            buf_write(buf);
        }
        bool del_indirect = true;
        for (int i = 0; i < SFS_NINDIRECT; i++) {
            if (data[i] != 0) {
                del_indirect = false;
                break;
            }
        }
        buf_release(sfs, buf);
        if (del_indirect == true) {
            blk_free(sfs, sin->din->addrs[SFS_NDIRECT]);
            sin->din->addrs[SFS_NDIRECT] = 0;
            sin->valid = false;
        }
    } else {
        bn -= SFS_NINDIRECT;
        if (bn < SFS_NINDIRECT * SFS_NINDIRECT) {
            int idx = bn / SFS_NINDIRECT;
            int off = bn % SFS_NINDIRECT;
            assert((addr = sin->din->addrs[SFS_NDIRECT + 1]) != 0);
            buf = buf_read(sfs, addr);
            data = (uint *)buf->data;
            assert(data[idx] != 0);
            Disk_buf *buf_lvl2;
            uint *data_lvl2;
            buf_lvl2 = buf_read(sfs, data[idx]);
            data_lvl2 = (uint *)buf_lvl2->data;
            if (data_lvl2[off] != 0) {
                blk_free(sfs, data_lvl2[off]);
                data_lvl2[off] = 0;
                buf_write(buf_lvl2);
            }

            bool del_lvl2 = true;
            for (int i = 0; i < SFS_NINDIRECT; i++) {
                if (data_lvl2[i] != 0) {
                    del_lvl2 = false;
                    break;
                }
            }
            buf_release(sfs, buf_lvl2);
            if (del_lvl2 == true) {
                blk_free(sfs, data[idx]);
                data[idx] = 0;
                buf_write(buf);
            }
            bool del_indirect = true;
            for (int i = 0; i < SFS_NINDIRECT; i++) {
                if (data[i] != 0) {
                    del_indirect = false;
                    break;
                }
            }
            buf_release(sfs, buf);
            if (del_indirect == true) {
                blk_free(sfs, sin->din->addrs[SFS_NDIRECT + 1]);
                sin->din->addrs[SFS_NDIRECT + 1] = 0;
                sin->valid = false;
            }
        } else {
            panic("sfs_bmap_free_nolock: out of range");
        }
    }
}

static int sfs_truncfile_nolock(Inode *inode, off_t len) {
    if (len < 0 || len > SFS_MAXFILE_SIZE) { return -E_INVAL; }
    Sfs_fs *sfs = fsop_info(vop_fs(inode), sfs);
    Sfs_inode *sin = vop_info(inode, sfs_inode);
    Dinode *din = sin->din;
    // assert(din->type != SFS_TYPE_DIR);
    uint32_t nblks = ROUNDUP(sin->din->size, SFS_BSIZE) / SFS_BSIZE;
    uint32_t tblks = ROUNDUP(len, SFS_BSIZE) / SFS_BSIZE;
    if (din->size == len) { return 0; }
    if (nblks < tblks) {
        while (nblks != tblks) { bmap_load_nolock(sfs, sin, nblks++); }
    } else if (tblks < nblks) {
        while (tblks != nblks) { bmap_free_nolock(sfs, sin, --nblks); }
    }
    din->size = len;
    sin->valid = false;
    return 0;
}

static int sfs_truncfile(Inode *inode, off_t len) {
    int ret;
    Sfs_fs *sfs = fsop_info(vop_fs(inode), sfs);
    Sfs_inode *sin = vop_info(inode, sfs_inode);
    sinode_lock(inode);
    ret = sfs_truncfile_nolock(inode, len);
    sinode_updata_nolock(sfs, sin, MEM2DISK);
    sinode_unlock(inode);
    return ret;
}

static int sfs_reclaim(Inode *inode) {
    int ret = 0;
    Sfs_fs *sfs = fsop_info(vop_fs(inode), sfs);
    Sfs_inode *sin = vop_info(inode, sfs_inode);
    assert(atomic_read(&inode->ref_count) == 0 && atomic_read(&inode->open_count) == 0);
    // if (sin->din->nlink == 0) {
    //     vop_truncate(inode, 0);
    // } else if (sin->valid == false) {
    //     vop_fsync(inode);
    // }

    // lock_sfs_inodes(sfs);
    sfs_remove_links(sin);
    // unlock_sfs_inodes(sfs);

    sinode_updata_nolock(sfs, sin, DISK2MEM);
    // sinode_lock(inode);
    if (sin->din->nlink == 0) {     
        ret = sfs_truncfile_nolock(inode, 0); 
        if(ret != 0){
              sfs_set_links(sfs, vop_info(inode, sfs_inode));
              unlock_sfs_inodes(sfs);
              return ret;
        }
    }
    sinode_updata_nolock(sfs, sin, MEM2DISK);
    // sinode_unlock(inode);

    kfree(sin->din);
    vop_kill(inode);
    return 0;
}

static int sfs_gettype(Inode *inode, uint32_t *type_store) {
    Dinode *din = vop_info(inode, sfs_inode)->din;
    switch (din->type) {
        case SFS_TYPE_DIR: *type_store = S_IFDIR; return 0;
        case SFS_TYPE_FILE: *type_store = S_IFREG; return 0;
        case SFS_TYPE_LINK: *type_store = S_IFLNK; return 0;
    }
    panic("invalid file type %d.\n", din->type);
    return 0;
}

static int sfs_fstat(Inode *inode, Stat *stat) {
    int ret;
    memset(stat, 0, sizeof(Stat));
    if ((ret = vop_gettype(inode, &stat->st_mode)) != 0) { return ret; }
    Dinode *din = vop_info(inode, sfs_inode)->din;
    stat->st_nlinks = din->nlink;
    stat->st_blocks = ROUNDUP(din->size, SFS_BSIZE) / SFS_BSIZE;
    stat->st_size = din->size;
    return 0;
}

int sfs_read_nolock(Inode *inode, void *dst, off_t off, size_t len, size_t *len_store) {
    Sfs_fs *sfs = fsop_info(vop_fs(inode), sfs);
    Sfs_inode *sin = vop_info(inode, sfs_inode);
    size_t tot, mlen;
    Disk_buf *buf;
    int ret;
    if (off < 0 || off >= sin->din->size) {
        ret = -E_INVAL;
        return ret;
    }
    if (off + len >= sin->din->size) { len = sin->din->size - off; }
    for (tot = 0; tot < len; tot += mlen, off += mlen, dst += mlen) {
        buf = buf_read(sfs, bmap_load_nolock(sfs, sin, off / SFS_BSIZE));
        mlen = min(len - tot, SFS_BSIZE - off % SFS_BSIZE);
        memmove(dst, buf->data + (off % SFS_BSIZE), mlen);
        buf_release(sfs, buf);
    }
    if (len_store != nullptr) *len_store = tot;
    if (tot != len) { return -E_FAULT; }
    return 0;
}

static int sfs_read(Inode *inode, Iobuf *iob) {
    int ret;
    size_t alen = 0;
    Sfs_fs *sfs = fsop_info(vop_fs(inode), sfs);
    Sfs_inode *sin = vop_info(inode, sfs_inode);
    sinode_lock(inode);
    ret = sfs_read_nolock(inode, iob->io_base, iob->io_offset, iob->io_len, &alen);
    if (alen != 0) { iobuf_skip(iob, alen); }
    sinode_unlock(inode);
    return ret;
}

static int sfs_write_nolock(Inode *inode, void *src, off_t off, size_t len, size_t *len_store) {
    Sfs_fs *sfs = fsop_info(vop_fs(inode), sfs);
    Sfs_inode *sin = vop_info(inode, sfs_inode);
    size_t tot, mlen;
    Disk_buf *buf;
    int ret;
    if (off < 0 || off + len > SFS_MAXFILE_SIZE) {
        ret = -E_INVAL;
        return ret;
    }
    for (tot = 0; tot < len; tot += mlen, off += mlen, src += mlen) {
        buf = buf_read(sfs, bmap_load_nolock(sfs, sin, off / SFS_BSIZE));
        mlen = min(len - tot, SFS_BSIZE - off % SFS_BSIZE);
        memmove(buf->data + (off % SFS_BSIZE), src, mlen);
        buf_write(buf);
        buf_release(sfs, buf);
    }
    if (len > 0) {
        if (off >= sin->din->size) {
            sin->din->size = off;
            sin->valid = false;
        }
        // write the i-node back to disk even if the size didn't change
        // because the loop above might have called bmap_load_nolock() and added a new
        // block to din->addrs[].
        sinode_updata_nolock(sfs, sin, MEM2DISK);
    }
    if (len_store != nullptr) *len_store = tot;
    if (tot != len) { return -E_FAULT; }
    return 0;
}

static int sfs_write(Inode *inode, Iobuf *iob) {
    int ret;
    size_t alen;
    Sfs_fs *sfs = fsop_info(vop_fs(inode), sfs);
    Sfs_inode *sin = vop_info(inode, sfs_inode);
    sinode_lock(inode);
    ret = sfs_write_nolock(inode, iob->io_base, iob->io_offset, iob->io_len, &alen);
    if (alen != 0) { iobuf_skip(iob, alen); }
    sinode_unlock(inode);
    return ret;
}

static int sfs_lookup_once_nolock(Sfs_fs *sfs, Inode *inode, const char *name, Inode **inode_store,
                                  uint *slot_off) {
    uint off;
    Sfs_dirent de;
    int ret;
    Sfs_inode *sin = vop_info(inode, sfs_inode);
    if (sin->din->type != SFS_TYPE_DIR) { return -E_NOTDIR; }
    for (off = 0; off < sin->din->size; off += sizeof(de)) {
        if ((ret = sfs_read_nolock(inode, &de, off, sizeof(de), nullptr)) != 0) { return ret; }
        if (de.inum == 0) continue;
        if (strcmp(name, de.name) == 0) {
            assert(inode_store != nullptr);
            *inode_store = get_inode(sfs, de.inum);
            break;
        }
    }
    if (off >= sin->din->size) { return -E_NOENT; }
    if (slot_off != nullptr) { *slot_off = off; }
    return 0;
}

static char *sfs_lookup_subpath(char *path) {
    if ((path = strchr(path, '/')) != nullptr) {
        while (*path == '/') { *path++ = '\0'; }
        if (*path == '\0') { return nullptr; }
    }
    return path;
}

void sinode_unlock_put(Inode *inode) {
    sinode_unlock(inode);
    vop_ref_dec(inode);
}

static int __sfs_lookup(Inode *inode, char *path, Inode **inode_store, bool nameiparent, char **endp) {
    Sfs_fs *sfs = fsop_info(vop_fs(inode), sfs);
    assert(*path != '\0' && *path != '/');
    char *subpath;
    Inode *next_inode;
    int ret;
    do {
        sinode_lock(inode);
        Sfs_inode *sin = vop_info(inode, sfs_inode);
        if (sin->din->type != SFS_TYPE_DIR) {
            sinode_unlock_put(inode);
            return -E_NOTDIR;
        }
        subpath = sfs_lookup_subpath(path);
        if (strlen(path) > SFS_DIRSIZE) {
            sinode_unlock_put(inode);
            return -E_TOO_BIG;
        }
        // if(nameiparent && subpath == nullptr){
        //     sinode_unlock_put(inode);
        //     return -E_NOTDIR;
        // }
        if (nameiparent && subpath == nullptr) {
            sinode_unlock(inode);
            *inode_store = inode;
            if (endp != nullptr) { *endp = path; }
            return 0;
        }
        if ((ret = sfs_lookup_once_nolock(sfs, inode, path, &next_inode, nullptr)) != 0) {
            sinode_unlock_put(inode);
            return ret;
        }
        sinode_unlock_put(inode);
        inode = next_inode;
        path = subpath;
    } while (path != nullptr);
    *inode_store = inode;
    return 0;
}

static int sfs_lookup(Inode *inode, char *path, Inode **inode_store) {
    int ret = __sfs_lookup(inode, path, inode_store, false, nullptr);
    vop_ref_inc(inode);
    return ret;
}

int sfs_lookup_parent(Inode *inode, char *path, Inode **inode_store, char **endp) {
    int ret = __sfs_lookup(inode, path, inode_store, true, endp);
    vop_ref_inc(inode);
    return ret;
}

static int sfs_dir_link_nolock(Inode *inode, const char *name, Inode *link_node) {
    uint off;
    Sfs_dirent de;
    int ret;
    Sfs_fs *sfs = fsop_info(vop_fs(inode), sfs);
    Sfs_inode *sin = vop_info(inode, sfs_inode);
    Sfs_inode *lsin = vop_info(link_node, sfs_inode);
    Inode *sub_inode;
    if (sfs_lookup_once_nolock(sfs, inode, name, &sub_inode, nullptr) == 0) {
        vop_ref_dec(sub_inode);
        return -E_EXISTS;
    }
    for (off = 0; off < sin->din->size; off += sizeof(de)) {
        if ((ret = sfs_read_nolock(inode, &de, off, sizeof(de), nullptr)) != 0) { return ret; }
        if (de.inum == 0) { break; }
    }
    strncpy(de.name, name, sfs_dentry_size);
    de.inum = lsin->ino;
    if ((ret = sfs_write_nolock(inode, &de, off, sizeof(de), nullptr)) != 0) { return ret; }
    return 0;
}

static int sfs_dir_link(Inode *inode, const char *name, Inode *link_node) {
    int ret;
    Sfs_fs *sfs = fsop_info(vop_fs(inode), sfs);
    Sfs_inode *sin = vop_info(inode, sfs_inode);
    Sfs_inode *lsin = vop_info(link_node, sfs_inode);
    sinode_lock(link_node);
    if (lsin->din->type == SFS_TYPE_DIR) {
        sinode_unlock(link_node);
        return -E_ISDIR;
    }
    lsin->din->nlink++;
    lsin->valid = false;
    sinode_updata_nolock(sfs, lsin, MEM2DISK);
    sinode_unlock(link_node);
    sinode_lock(inode);
    if ((ret = sfs_dir_link_nolock(inode, name, link_node)) != 0) {
        sinode_unlock(inode);
        sinode_lock(link_node);
        lsin->din->nlink--;
        lsin->valid = false;
        sinode_updata_nolock(sfs, lsin, MEM2DISK);
        sinode_unlock(link_node);
        // if(ret == -E_EXISTS){
        //     vop_ref_dec(link_node);
        // }
        // vop_ref_dec(link_node);
        return ret;
    }
    sinode_unlock(inode);
    return 0;
}

static bool is_dir_empty(Inode *inode) {
    Sfs_fs *sfs = fsop_info(vop_fs(inode), sfs);
    Sfs_inode *sin = vop_info(inode, sfs_inode);
    uint off;
    Sfs_dirent de;
    int ret;
    for (off = 2 * sizeof(de); off < sin->din->size; off += sizeof(de)) {
        if ((ret = sfs_read_nolock(inode, &de, off, sizeof(de), nullptr)) != 0) { return ret; }
        if (de.inum != 0) { return false; }
    }
    return true;
}

static int sfs_dir_unlink(Inode *inode, const char *name) {
    int ret;
    uint off;
    Sfs_dirent de;
    Sfs_fs *sfs = fsop_info(vop_fs(inode), sfs);
    Sfs_inode *sin = vop_info(inode, sfs_inode);
    Inode *link_node;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) { return -E_INVAL; }
    sinode_lock(inode);
    if ((ret = sfs_lookup_once_nolock(sfs, inode, name, &link_node, &off)) != 0) { goto failed; }
    sinode_lock(link_node);
    Sfs_inode *lsin = vop_info(link_node, sfs_inode);
    if (lsin->din->nlink < 1) { panic("sfs_dir_unlink: nlink < 1"); }
    if (lsin->din->type == SFS_TYPE_DIR && !is_dir_empty(link_node)) { goto failed2; }
    memset(&de, 0, sizeof(de));
    if ((ret = sfs_write_nolock(inode, &de, off, sizeof(de), nullptr)) != 0) { goto failed2; }
    if (lsin->din->type == SFS_TYPE_DIR) {
        sin->din->nlink--;
        sin->valid = false;
        sinode_updata_nolock(sfs, sin, MEM2DISK);
    }
    lsin->din->nlink--;
    lsin->valid = false;
    sinode_updata_nolock(sfs, lsin, MEM2DISK);
    sinode_unlock_put(link_node);
    sinode_unlock(inode);
    // vop_ref_dec(link_node);
    return 0;
failed2:
    sinode_unlock_put(link_node);
failed:
    sinode_unlock(inode);
    return ret;
}

int sfs_create(Inode *inode, const char *name, bool excl, Inode **inode_store) {
    Sfs_fs *sfs = fsop_info(vop_fs(inode), sfs);
    int ret;
    Inode *sub_inode = nullptr;
    sinode_lock(inode);
    if ((ret = sfs_lookup_once_nolock(sfs, inode, name, &sub_inode, nullptr)) == 0) {
        assert(sub_inode != nullptr);
        sinode_unlock(inode);
        if (excl) {
            ret = -E_EXISTS;
            vop_ref_dec(sub_inode);
            return ret;
        }
        *inode_store = sub_inode;
        return 0;
    } else if (ret == -E_NOENT) {
        sub_inode = inode_alloc(sfs, SFS_TYPE_FILE);
        if ((ret = sfs_dir_link_nolock(inode, name, sub_inode)) != 0) {
            sinode_unlock(inode);
            vop_ref_dec(sub_inode);
            return ret;
        }
        sinode_unlock(inode);
        *inode_store = sub_inode;
        return 0;
    } else {
        sinode_unlock(inode);
        return ret;
    }
}

int sfs_mkdir(Inode *inode, const char *name) {
    Sfs_fs *sfs = fsop_info(vop_fs(inode), sfs);
    Sfs_inode *sin = vop_info(inode, sfs_inode);
    int ret;
    if (sin->din->type != SFS_TYPE_DIR) { return -E_NOTDIR; }
    Inode *sub_inode = inode_alloc(sfs, SFS_TYPE_DIR);
    if ((ret = sfs_dir_link_nolock(sub_inode, ".", sub_inode)) != 0) { goto failed; }
    if ((ret = sfs_dir_link_nolock(sub_inode, "..", inode)) != 0) { goto failed2; }
    sinode_lock(inode);
    if ((ret = sfs_lookup_once_nolock(sfs, inode, name, &sub_inode, nullptr)) == 0) {
        assert(sub_inode != nullptr);
        ret = -E_EXISTS;
        goto failed3;
    }
    if ((ret = sfs_dir_link_nolock(inode, name, sub_inode)) != 0) { goto failed3; }
    sin->din->nlink++;  // for ".."
    sin->valid = false;
    sinode_updata_nolock(sfs, sin, MEM2DISK);
    sinode_unlock(inode);
    vop_ref_dec(sub_inode);
    return 0;
failed3:
    sinode_unlock(inode);
    sfs_dir_unlink(sub_inode, "..");
failed2:
    sfs_dir_unlink(sub_inode, ".");
failed:
    vop_ref_dec(sub_inode);
    return ret;
}

static int sfs_opendir(Inode *inode, uint32_t open_flags) {
    switch (open_flags & O_ACCMODE) {
        case O_RDONLY: break;
        case O_WRONLY:
        case O_RDWR:
        default: return -E_ISDIR;
    }
    if (open_flags & O_APPEND) { return -E_ISDIR; }
    return 0;
}

static int sfs_openfile(Inode *inode, uint32_t open_flags) {
    Sfs_inode *sin = vop_info(inode, sfs_inode);
    if (sin->din->type == SFS_TYPE_FILE && open_flags & O_NOFOLLOW) { 
        return -E_LOOP; 
    }
    return 0; 
}

static int sfs_close(Inode *node) { return vop_fsync(node); }

extern char fs_cur_pwd[SFS_PWD_LEN];

static int sfs_namefile(Inode *inode, Iobuf *iob) {
    int ret = iobuf_move(iob, fs_cur_pwd, iob->io_resid, 1, nullptr);
    return ret;
}

static int sfs_getdirentry(Inode *inode, Iobuf *iob) {
    off_t off;
    Sfs_dirent de;
    int ret;
    Sfs_fs *sfs = fsop_info(vop_fs(inode), sfs);
    Sfs_inode *sin = vop_info(inode, sfs_inode);
    off = iob->io_offset;
    if (off < 0 || off >= sin->din->size || off % sizeof(de) != 0) { return -E_INVAL; }
    sinode_lock(inode);
    if ((ret = sfs_read_nolock(inode, &de, off, sizeof(de), nullptr)) != 0) {
        sinode_unlock(inode);
        return ret;
    }
    if (de.inum == 0) {
        sinode_unlock(inode);
        return -E_NOENT;
    }
    ret = iobuf_move(iob, de.name, sfs_dentry_size, 1, nullptr);
    sinode_unlock(inode);
    return ret;
}

static int sfs_tryseek(Inode *inode, off_t pos) {
    if (pos < 0 || pos > SFS_MAXFILE_SIZE) { return -E_INVAL; }
    Dinode *din = vop_info(inode, sfs_inode)->din;
    if (pos > din->size) { return vop_truncate(inode, pos); }
    return 0;
}

static int sfs_symlink(Inode *inode, const char *name, const char *path){
    Sfs_fs *sfs = fsop_info(vop_fs(inode), sfs);
    Sfs_inode *sin = vop_info(inode, sfs_inode);
    int ret;
    if (sin->din->type != SFS_TYPE_DIR) { return -E_NOTDIR; }
    Inode *sub_inode = nullptr;
    sinode_lock(inode);
    if ((ret = sfs_lookup_once_nolock(sfs, inode, name, &sub_inode, nullptr)) == 0) {
        assert(sub_inode != nullptr);
        sinode_unlock(inode);
        vop_ref_dec(sub_inode);
        return -E_EXISTS;
    } else if (ret == -E_NOENT) {
        sub_inode = inode_alloc(sfs, SFS_TYPE_LINK);
        if ((ret = sfs_dir_link_nolock(inode, name, sub_inode)) != 0 ||
            (ret = sfs_write_nolock(sub_inode, (void *)path, 0, MAXPATH, nullptr)) != 0) {
            sinode_unlock(inode);
            vop_ref_dec(sub_inode);
            return ret;
        }
        // if((ret = sfs_write_nolock(sub_inode, (void *)path, 0, MAXPATH, nullptr)) != 0){
        //     sinode_unlock(inode);
        //     vop_ref_dec(sub_inode);
        //     return ret;
        // }
        vop_ref_dec(sub_inode);
        sinode_unlock(inode);
        return 0;
    } else {
        sinode_unlock(inode);
        return ret;
    }
}

static const struct inode_ops sfs_node_dirops = {
    .vop_magic = VOP_MAGIC,
    .vop_open = sfs_opendir,
    .vop_close = sfs_close,
    .vop_read = NULL_VOP_ISDIR,
    .vop_write = NULL_VOP_ISDIR,
    .vop_fstat = sfs_fstat,
    .vop_fsync = sfs_fsync,
    .vop_mkdir = sfs_mkdir,
    .vop_link = sfs_dir_link,
    .vop_rename = NULL_VOP_UNIMP,
    .vop_readlink = NULL_VOP_ISDIR,
    .vop_symlink = sfs_symlink,
    .vop_namefile = sfs_namefile,
    .vop_getdirentry = sfs_getdirentry,
    .vop_reclaim = sfs_reclaim,
    .vop_ioctl = NULL_VOP_INVAL,
    .vop_gettype = sfs_gettype,
    .vop_tryseek = NULL_VOP_ISDIR,
    .vop_truncate = sfs_truncfile,
    .vop_create = sfs_create,
    .vop_unlink = sfs_dir_unlink,
    .vop_lookup = sfs_lookup,
    .vop_lookup_parent = sfs_lookup_parent,
};

static const struct inode_ops sfs_node_fileops = {
    .vop_magic = VOP_MAGIC,
    .vop_open = sfs_openfile,
    .vop_close = sfs_close,
    .vop_read = sfs_read,
    .vop_write = sfs_write,
    .vop_fstat = sfs_fstat,
    .vop_fsync = sfs_fsync,
    .vop_mkdir = NULL_VOP_NOTDIR,
    .vop_link = NULL_VOP_NOTDIR,
    .vop_rename = NULL_VOP_NOTDIR,
    .vop_readlink = NULL_VOP_NOTDIR,
    .vop_symlink = NULL_VOP_NOTDIR,
    .vop_namefile = NULL_VOP_NOTDIR,
    .vop_getdirentry = NULL_VOP_NOTDIR,
    .vop_reclaim = sfs_reclaim,
    .vop_ioctl = NULL_VOP_INVAL,
    .vop_gettype = sfs_gettype,
    .vop_tryseek = sfs_tryseek,
    .vop_truncate = sfs_truncfile,
    .vop_create = NULL_VOP_NOTDIR,
    .vop_unlink = NULL_VOP_NOTDIR,
    .vop_lookup = sfs_lookup,
    .vop_lookup_parent = sfs_lookup_parent,
};
