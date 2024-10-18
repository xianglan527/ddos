#include "assert.h"
#include "dev.h"
#include "error.h"
#include "inode.h"
#include "list.h"
#include "sem.h"
#include "slab.h"
#include "stdio.h"
#include "string.h"
#include "vfs.h"

typedef struct vfs_dev Vfs_dev;
struct vfs_dev {
    const char *devname;
    Inode *devnode;
    Fs *fs;
    bool mountable;
    List_entry vdev_link;
};

#define le2vdev(le) to_struct((le), Vfs_dev, vdev_link)

static List_entry vdev_list;
static Sem vdev_list_sem;

static void lock_vdev_list(void) { down(&vdev_list_sem); }

static void unlock_vdev_list(void) { up(&vdev_list_sem); }

void vfs_devlist_init(void) {
    list_init(&vdev_list);
    sem_init(&vdev_list_sem, 1);
}

void vfs_cleanup(void) {
    if (!list_empty(&vdev_list)) {
        lock_vdev_list();
        {
            List_entry *le;
            list_for_each(le, &vdev_list) {
                Vfs_dev *vdev = le2vdev(le);
                if (vdev->fs != nullptr) { fsop_cleanup(vdev->fs); }
            }
        }
        unlock_vdev_list();
    }
}

int vfs_sync(void) {
    if (!list_empty(&vdev_list)) {
        lock_vdev_list();
        {
            List_entry *le;
            list_for_each(le, &vdev_list) {
                Vfs_dev *vdev = le2vdev(le);
                if (vdev->fs != nullptr) {
                    int ret = fsop_sync(vdev->fs);
                    if (ret != 0) return ret;
                }
            }
        }
        unlock_vdev_list();
    }
    return 0;
}

int vfs_get_root(const char *devname, Inode **node_store) {
    assert(devname != nullptr);
    int ret = -E_NO_DEV;
    if (!list_empty(&vdev_list)) {
        lock_vdev_list();
        {
            List_entry *le;
            list_for_each(le, &vdev_list) {
                Vfs_dev *vdev = le2vdev(le);
                if (strcmp(devname, vdev->devname) == 0) {
                    Inode *found = nullptr;
                    /*
                     * If this device has a mounted filesystem, and
                     * DEVNAME names either the filesystem or the device,
                     * return the root of the filesystem.
                     *
                     * If it has no mounted filesystem, it's mountable,
                     * and DEVNAME names the device, return -E_NA_DEV.
                     */
                    if (vdev->fs != nullptr) {
                        found = fsop_get_root(vdev->fs);
                    } else if (!vdev->mountable) {
                        vop_ref_inc(vdev->devnode);
                        found = vdev->devnode;
                    }
                    if (found != nullptr) {
                        /*
                         * If DEVNAME names the device, and we get here, it
                         * must have no fs and not be mountable. In this case,
                         * we return the inode of device itself--node_store.
                         */
                        ret = 0, *node_store = found;
                    } else {
                        ret = -E_NA_DEV;
                    }
                    break;
                }
            }
        }
        unlock_vdev_list();
    }
    return ret;
}

const char *vfs_get_devname(Fs *fs) {
    assert(fs != nullptr);
    lock_vdev_list();
    {
        List_entry *le;
        list_for_each(le, &vdev_list) {
            Vfs_dev *vdev = le2vdev(le);
            if (vdev->fs == fs) { return vdev->devname; }
        }
    }
    unlock_vdev_list();
    return nullptr;
}

static bool check_devname_conflict(const char *devname) {
    List_entry *le;
    list_for_each(le, &vdev_list) {
        Vfs_dev *vdev = le2vdev(le);
        if (strcmp(vdev->devname, devname) == 0) { return false; }
    }
    return true;
}

/*
 * Add a new device to the VFS layer's device table.
 *
 * If "mountable" is set, the device will be treated as one that expects
 * to have a filesystem mounted on it.
 */
static int vfs_do_add(const char *devname, Inode *devnode, Fs *fs, bool mountable) {
    assert(devname != nullptr);
    assert((devnode == nullptr && !mountable) || (devnode != nullptr && check_inode_type(devnode, device)));
    if (strlen(devname) > FS_MAX_DNAME_LEN) { return -E_TOO_BIG; }
    int ret = -E_NO_MEM;
    char *s_devname;
    if ((s_devname = strdup(devname)) == nullptr) { return ret; }
    Vfs_dev *vdev;
    if ((vdev = kmalloc(sizeof(Vfs_dev))) == nullptr) { goto failed_cleanup_name; }
    ret = -E_EXISTS;
    lock_vdev_list();
    if (!check_devname_conflict(s_devname)) {
        unlock_vdev_list();
        goto failed_cleanup_vdev;
    }
    vdev->devname = s_devname;
    vdev->devnode = devnode;
    vdev->mountable = mountable;
    vdev->fs = fs;
    list_add(&vdev_list, &vdev->vdev_link);
    unlock_vdev_list();
    return 0;
failed_cleanup_vdev:
    kfree(vdev);
failed_cleanup_name:
    kfree(s_devname);
    return ret;
}

int vfs_add_fs(const char *devname, Fs *fs) { return vfs_do_add(devname, nullptr, fs, 0); }

int vfs_add_dev(const char *devname, Inode *devnode, bool mountable) {
    return vfs_do_add(devname, devnode, nullptr, mountable);
}

static int find_mount(const char *devname, Vfs_dev **vdev_store) {
    assert(devname != nullptr);
    List_entry *le;
    list_for_each(le, &vdev_list) {
        Vfs_dev *vdev = le2vdev(le);
        if (vdev->mountable && strcmp(vdev->devname, devname) == 0) {
            *vdev_store = vdev;
            return 0;
        }
    }
    return -E_NO_DEV;
}

int vfs_mount(const char *devname, int (*mountfunc)(Device *dev, Fs **fs_store)) {
    int ret;
    lock_vdev_list();
    Vfs_dev *vdev;
    if ((ret = find_mount(devname, &vdev)) != 0) { goto out; }
    if (vdev->fs != nullptr) {
        ret = -E_BUSY;
        goto out;
    }
    assert(vdev->devname != nullptr && vdev->mountable);
    Device *dev = vop_info(vdev->devnode, device);
    if ((ret = mountfunc(dev, &vdev->fs)) == 0) {
        assert(vdev->fs != nullptr);
        cprintf("vfs: mount %s.\n", vdev->devname);
    }
out:
    unlock_vdev_list();
    return ret;
}

int vfs_unmount(const char *devname) {
    int ret;
    lock_vdev_list();
    Vfs_dev *vdev;
    if ((ret = find_mount(devname, &vdev)) != 0) { goto out; }
    if (vdev->fs != nullptr) {
        ret = -E_BUSY;
        goto out;
    }
    assert(vdev->devname != nullptr && vdev->mountable);
    if ((ret = fsop_sync(vdev->fs)) != 0) { goto out; }
    if ((ret = fsop_unmount(vdev->fs)) == 0) {
        vdev->fs = nullptr;
        cprintf("vfs: unmount %s.\n", vdev->devname);
    }
out:
    unlock_vdev_list();
    return ret;
}

int vfs_unmount_all(void) {
    if (!list_empty(&vdev_list)) {
        lock_vdev_list();
        {
            List_entry *le;
            list_for_each(le, &vdev_list) {
                Vfs_dev *vdev = le2vdev(le);
                if (vdev->mountable && vdev->fs != NULL) {
                    int ret;
                    if ((ret = fsop_sync(vdev->fs)) != 0) {
                        cprintf("vfs: warning: sync failed for %s: %e.\n", vdev->devname, ret);
                        continue;
                    }
                    if ((ret = fsop_unmount(vdev->fs)) != 0) {
                        cprintf("vfs: warning: unmount failed for %s: %e.\n", vdev->devname, ret);
                        continue;
                    }
                    vdev->fs = nullptr;
                    cprintf("vfs: unmount %s.\n", vdev->devname);
                }
            }
        }
        unlock_vdev_list();
    }
    return 0;
}