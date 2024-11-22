#include "assert.h"
#include "error.h"
#include "inode.h"
#include "proc.h"
#include "stdio.h"
#include "string.h"
#include "vfs.h"
#include "slab.h"

static Inode *get_cwd_nolock(void) { return myproc()->fs_struct->pwd; }

static void set_cwd_nolock(Inode *pwd) { myproc()->fs_struct->pwd = pwd; }

static void lock_cfs(void) { lock_fs(myproc()->fs_struct); }

static void unlock_cfs(void) { unlock_fs(myproc()->fs_struct); }

int vfs_get_curdir(Inode **dir_store) {
    Inode *node;
    if ((node = get_cwd_nolock()) != nullptr) {
        vop_ref_inc(node);
        *dir_store = node;
        return 0;
    }
    return -E_NOENT;
}

int vfs_set_curdir(Inode *dir) {
    int ret = 0;
    lock_cfs();
    Inode *old_dir;
    if ((old_dir = get_cwd_nolock()) != dir) {
        if (dir != nullptr) {
            uint32_t type;
            if ((ret = vop_gettype(dir, &type)) != 0) { goto out; }
            if (!S_ISDIR(type)) {
                ret = -E_NOTDIR;
                goto out;
            }
            vop_ref_inc(dir);
        }
        set_cwd_nolock(dir);
        if (old_dir != nullptr) { vop_ref_dec(old_dir); }
    }
out:
    unlock_cfs();
    return ret;
}

char fs_cur_pwd[SFS_PWD_LEN];
extern int get_device(char *path, char **subpath, Inode **node_store, bool *relative);

static void normalize_Path(const char *input, char *output) {
    char **stack = (char **)kmalloc(1024 * sizeof(char *));  
    assert(stack != nullptr);
    int top = -1;
    char temp[sfs_dentry_size];
    int temp_index = 0;
    for (int i = 0; input[i] != '\0'; i++) {
        if (input[i] == '/') {
            if (temp_index > 0) {
                temp[temp_index] = '\0'; 
                if (strcmp(temp, "..") == 0) {
                    if (top >= 0) {
                        kfree(stack[top]); 
                        top--;
                    }
                } else if (strcmp(temp, ".") != 0 && temp[0] != '\0') {
                    top++;
                    stack[top] = (char *)kmalloc(strlen(temp) + 1);  
                    assert(stack[top] != nullptr);
                    strcpy(stack[top], temp);
                }
                temp_index = 0; 
            }
        } else {
            temp[temp_index++] = input[i];
        }
    }
    if (temp_index > 0) {
        temp[temp_index] = '\0';
        if (strcmp(temp, "..") == 0) {
            if (top >= 0) {
                kfree(stack[top]); 
                top--;
            }
        } else if (strcmp(temp, ".") != 0 && temp[0] != '\0') {
            top++;
            stack[top] = (char *)kmalloc(strlen(temp) + 1);
            assert(stack[top] != nullptr);
            strcpy(stack[top], temp);
        }
    }
    if (top == -1) {
        strcpy(output, "/");
    } else {
        output[0] = '/';
        int index = 1;
        for (int i = 0; i <= top; i++) {
            strcpy(output + index, stack[i]);
            index += strlen(stack[i]);
            output[index++] = '/';
            kfree(stack[i]);  
        }
        output[index - 1] = '\0';
    }
    kfree(stack);
}

int vfs_chdir(char *path) {
    int ret;
    Inode *node;
    char alloc_path[MAXPATH];
    char raw_path[MAXPATH];
    memset(alloc_path, 0, MAXPATH);
    strncpy(alloc_path, path, MAXPATH);
    char *save_path = alloc_path;
    if ((ret = vfs_lookup(path, &node)) == 0) {
        ret = vfs_set_curdir(node);
        vop_ref_dec(node);
    }
    if (ret == 0 && node->in_type == __in_type(sfs_inode)) {
        bool relative;
        Inode *node;
        strncpy(raw_path, fs_cur_pwd, MAXPATH);
        assert(get_device(save_path, &save_path, &node, &relative) == 0);
        vop_ref_dec(node);
        if (relative == true) {
            assert(strlen(raw_path) + strlen(save_path) + 1 < SFS_PWD_LEN);
            strcat(raw_path, "/");
            strcat(raw_path, save_path);
        } else {
            assert(strlen(save_path) + 1 < SFS_PWD_LEN);
            memset(raw_path, 0, sizeof(raw_path));
            raw_path[0] = '/';
            strcat(raw_path, save_path);
        }
        memset(fs_cur_pwd, 0, SFS_PWD_LEN);
        normalize_Path(raw_path, fs_cur_pwd);
    }
    return ret;
}

int vfs_getcwd(Iobuf *iob) {
    int ret;
    Inode *node;
    if ((ret = vfs_get_curdir(&node)) != 0) { return ret; }
    assert(node->in_fs != nullptr);
    const char *devname = vfs_get_devname(node->in_fs);
    if ((ret = iobuf_move(iob, (char *)devname, strlen(devname), 1, nullptr)) != 0) { goto out; }
    char colon = ':';
    if ((ret = iobuf_move(iob, &colon, sizeof(colon), 1, nullptr)) != 0) { goto out; }
    ret = vop_namefile(node, iob);
out:
    vop_ref_dec(node);
    return ret;
}