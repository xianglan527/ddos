#include "assert.h"
#include "error.h"
#include "inode.h"
#include "string.h"
#include "stdio.h"
#include "vfs.h"
#include "proc.h"

int get_device(char *path, char **subpath, Inode **node_store, bool *relative) {
    int i, slash = -1, colon = -1;
    if (relative != nullptr) { *relative = false; }
    for(i = 0; path[i] != '\0'; i++){
        if(path[i] == ':'){colon = i; break;}
        if(path[i] == '/'){slash = i; break;}
    }
    if(colon < 0 && slash != 0){
        /* *
         * No colon before a slash, so no device name specified, and the slash isn't leading
         * or is also absent, so this is a relative path or just a bare filename. Start from
         * the current directory, and use the whole thing as the subpath.
         * */
        *subpath = path;
        if (relative != nullptr) { *relative = true; }
        return vfs_get_curdir(node_store);
    }
    if(colon > 0){
        /* device:path - get root of device's filesystem */
        path[colon] = '\0';
        /* device:/path - skip slash, treat as device:path */
        while(path[++colon] == '/');
        *subpath = path + colon;
        return vfs_get_root(path, node_store);
    }
    int ret;
    if(*path == '/'){
        if((ret = vfs_get_bootfs(node_store)) != 0){
            return ret;
        }
    }
    else{
        assert(*path == ':');
        Inode *node;
        if((ret = vfs_get_curdir(&node)) != 0){
            return ret;
        }
        /* The current directory may not be a device, so it must have a fs. */
        assert(node->in_fs != nullptr);
        *node_store = fsop_get_root(node->in_fs);
        vop_ref_dec(node);
    }
    /* ///... or :/... */
    while(*(++path) == '/');
    *subpath = path;
    return 0;
}

int vfs_lookup(char *path, Inode **node_store){
    int ret;
    Inode *node;
    if((ret = get_device(path, &path, &node, nullptr)) != 0){
        return ret;
    }
    if(*path != '\0'){
        ret = vop_lookup(node, path, node_store);
        vop_ref_dec(node);
        return ret;
    }
    *node_store = node;
    return 0;
}

int vfs_exec(char *path, Inode **node_store) {
    int ret;
    Inode *node;
    if ((ret = vfs_get_bootfs(&node)) != 0) { return ret; }
    if (*path != '\0') {
        ret = vop_lookup(node, path, node_store);
        vop_ref_dec(node);
        return ret;
    }
    *node_store = node;
    return 0;
}

int vfs_lookup_parent(char *path, Inode **node_store, char **endp){
    int ret;
    Inode *node;
    if((ret = get_device(path, &path, &node, nullptr)) != 0){
        return ret;
    }
    ret = (*path != '\0')? vop_lookup_parent(node, path, node_store, endp) : -E_INVAL;
    vop_ref_dec(node);
    return ret;
}