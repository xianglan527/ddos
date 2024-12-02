#ifndef __FS_VFS_VFS_H__
#define __FS_VFS_VFS_H__

#include "iobuf.h"
#include "pipe.h"
#include "sfs.h"
#include "types.h"

typedef struct inode Inode;
typedef struct device Device;

typedef struct fs Fs;
struct fs {
    union {
        Pipe_fs __pipe_info;
        Sfs_fs __sfs_info;
    } fs_info;
    enum {
        fs_type_pipe_info = 0x5678,
        fs_type_sfs_info,
    } fs_type;
    int (*fs_sync)(Fs *fs);
    Inode *(*fs_get_root)(Fs *fs);
    int (*fs_unmount)(Fs *fs);
    void (*fs_cleanup)(Fs *fs);
};

#define __fs_type(type) fs_type_##type##_info

#define check_fs_type(fs, type) ((fs)->fs_type == __fs_type(type))

#define __fsop_info(_fs, type)                                \
    ({                                                        \
        Fs *__fs = (_fs);                                     \
        assert(__fs != nullptr && check_fs_type(__fs, type)); \
        &(__fs->fs_info.__##type##_info);                     \
    })

#define fsop_info(fs, type) __fsop_info(fs, type)

#define info2fs(info, type) to_struct((info), Fs, fs_info.__##type##_info)

Fs *__alloc_fs(int type);

#define alloc_fs(type)      __alloc_fs(__fs_type(type))

#define fsop_sync(fs) ((fs)->fs_sync(fs))
#define fsop_get_root(fs) ((fs)->fs_get_root(fs))
#define fsop_unmount(fs) ((fs)->fs_unmount(fs))
#define fsop_cleanup(fs) ((fs)->fs_cleanup(fs))

/*
 * Virtual File System layer functions.
 *
 * The VFS layer translates operations on abstract on-disk files or
 * pathnames to operations on specific files on specific filesystems.
 */
void vfs_init(void);
void vfs_cleanup(void);
void vfs_devlist_init(void);

/*
 * VFS layer low-level operations.
 * See inode.h for direct operations on inodes.
 * See fs.h for direct operations on filesystems/devices.
 *
 *    vfs_set_curdir   - change current directory of current thread by inode
 *    vfs_get_curdir   - retrieve inode of current directory of current thread
 *    vfs_sync         - force all dirty buffers to disk
 *    vfs_get_root     - get root inode for the filesystem named DEVNAME
 *    vfs_get_devname  - get mounted device name for the filesystem passed in
 */
int vfs_set_curdir(Inode *dir);
int vfs_get_curdir(Inode **dir_store);
int vfs_sync(void);
int vfs_get_root(const char *devname, Inode **root_store);
const char *vfs_get_devname(Fs *fs);

/*
 * VFS layer high-level operations on pathnames
 * Because namei may destroy pathnames, these all may too.
 *
 *    vfs_open         - Open or create a file. FLAGS/MODE per the syscall.
 *    vfs_close  - Close a inode opened with vfs_open. Does not fail.
 *                 (See vfspath.c for a discussion of why.)
 *    vfs_link         - Create a hard link to a file.
 *    vfs_symlink      - Create a symlink PATH containing contents CONTENTS.
 *    vfs_readlink     - Read contents of a symlink into a uio.
 *    vfs_mkdir        - Create a directory. MODE per the syscall.
 *    vfs_unlink       - Delete a file/directory.
 *    vfs_rename       - rename a file.
 *    vfs_chdir  - Change current directory of current thread by name.
 *    vfs_getcwd - Retrieve name of current directory of current thread.
 *
 */
int vfs_open(char *path, uint32_t open_flags, Inode **inode_store);
int vfs_close(Inode *node);
int vfs_link(char *old_path, char *new_path);
int vfs_symlink(char *old_path, char *new_path);
int vfs_readlink(char *path, Iobuf *iob);
int vfs_mkdir(char *path);
int vfs_unlink(char *path);
int vfs_rename(char *old_path, char *new_path);
int vfs_chdir(char *path);
int vfs_getcwd(Iobuf *iob);

/*
 * VFS layer mid-level operations.
 *
 *    vfs_lookup     - Like VOP_LOOKUP, but takes a full device:path name,
 *                     or a name relative to the current directory, and
 *                     goes to the correct filesystem.
 *    vfs_lookparent - Likewise, for VOP_LOOKPARENT.
 *
 * Both of these may destroy the path passed in.
 */
int vfs_lookup(char *path, Inode **node_store);
int vfs_lookup_parent(char *path, Inode **node_store, char **endp);
int vfs_exec(char *path, Inode **node_store);
/*
 * Misc
 *
 *    vfs_set_bootfs - Set the filesystem that paths beginning with a
 *                    slash are sent to. If not set, these paths fail
 *                    with ENOENT. The argument should be the device
 *                    name or volume name for the filesystem (such as
 *                    "lhd0:") but need not have the trailing colon.
 *
 *    vfs_get_bootfs - return the inode of the bootfs filesystem.
 *
 *    vfs_add_fs     - Add a hardwired filesystem to the VFS named device
 *                    list. It will be accessible as "devname:". This is
 *                    intended for filesystem-devices like emufs, and
 *                    gizmos like Linux procfs or BSD kernfs, not for
 *                    mounting filesystems on disk devices.
 *
 *    vfs_add_dev    - Add a device to the VFS named device list. If
 *                    MOUNTABLE is zero, the device will be accessible
 *                    as "DEVNAME:". If the mountable flag is set, the
 *                    device will be accessible as "DEVNAMEraw:" and
 *                    mountable under the name "DEVNAME". Thus, the
 *                    console, added with MOUNTABLE not set, would be
 *                    accessed by pathname as "con:", and lhd0, added
 *                    with mountable set, would be accessed by
 *                    pathname as "lhd0raw:" and mounted by passing
 *                    "lhd0" to vfs_mount.
 *
 *    vfs_mount      - Attempt to mount a filesystem on a device. The
 *                    device named by DEVNAME will be looked up and
 *                    passed, along with DATA, to the supplied function
 *                    MOUNTFUNC, which should create a struct fs and
 *                    return it in RESULT.
 *
 *    vfs_unmount    - Unmount the filesystem presently mounted on the
 *                    specified device.
 *
 *    vfs_unmountall - Unmount all mounted filesystems.
 */
int vfs_set_bootfs(char *fsname);
int vfs_get_bootfs(Inode **node_store);

int vfs_add_fs(const char *devname, Fs *fs);
int vfs_add_dev(const char *devname, Inode *devnode, bool mountable);

int vfs_mount(const char *devname, int (*mountfunc)(Device *dev, Fs **fs_store));
int vfs_unmount(const char *devname);
int vfs_unmount_all(void);
#endif