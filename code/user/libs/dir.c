#include "dir.h"
#include "error.h"
#include "malloc.h"
#include "stat.h"
#include "string.h"
#include "user.h"

DIR *opendir(char *path){
    DIR *dir;
    if((dir = malloc(sizeof(DIR))) == nullptr){
        return nullptr;
    }
    if((dir->fd = open(path, O_RDONLY)) < 0){
        goto failed;
    }
    Stat __stat, *stat = &__stat;
    if(fstat(dir->fd, stat) != 0 || !S_ISDIR(stat->st_mode)){
        goto failed;
    }
    dir->dirent.offset = 0;
    return dir;
failed:
    free(dir);
    return nullptr;
}

Dirent *readdir(DIR *dir){
    if(getdirentry(dir->fd, &dir->dirent) == 0){
        return &dir->dirent;
    }
    return nullptr;
}

void closedir(DIR *dir){
    close(dir->fd);
    free(dir);
}