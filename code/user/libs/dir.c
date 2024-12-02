#include "dir.h"
#include "error.h"
#include "malloc.h"
#include "stat.h"
#include "string.h"
#include "user.h"
#include "assert.h"
#include "printf.h"

DIR *opendir(char *path){
    DIR *dir;
    if((dir = malloc(sizeof(DIR))) == nullptr){
        return nullptr;
    }
    if((dir->fd = open(path, O_RDONLY)) < 0){
        warn("open failed error num is %d", dir->fd);
        goto failed;
    }
    Stat __stat, *stat = &__stat;
    if(fstat(dir->fd, stat) != 0){
        warn("fstat failed");
        goto failed;
    }
    if(!S_ISDIR(stat->st_mode)){
        warn("not a dir");
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

static char getmode(uint32_t st_mode) {
    char mode = '?';
    if (S_ISREG(st_mode)) mode = '-';
    if (S_ISDIR(st_mode)) mode = 'd';
    if (S_ISLNK(st_mode)) mode = 'l';
    if (S_ISCHR(st_mode)) mode = 'c';
    if (S_ISBLK(st_mode)) mode = 'b';
    return mode;
}

static void safe_stat_print(char *name) {
    Stat __stat, *stat = &__stat;
    int fd = open(name, O_RDONLY), ret = fstat(fd, stat);
    assert(fd >= 0 && ret == 0);
    printf("%c %3d   %4d %10d  ", getmode(stat->st_mode), stat->st_nlinks, stat->st_blocks, stat->st_size);
}

char fs_dir_buffer[SFS_PWD_LEN];

void print_cwd(void) {
    int ret = getcwd(fs_dir_buffer, sizeof(fs_dir_buffer));
    assert(ret == 0);
    printf("current: %s\n",fs_dir_buffer);
}

void lsdir(char *path) {
    DIR *dir = opendir(path);
    if(dir == nullptr) return;
    Dirent *dirent;
    size_t index = 0;
    while ((dirent = readdir(dir)) != nullptr) {
        printf("%d: ", index++);
        safe_stat_print(dirent->name);
        printf("%s\n", dirent->name);
    }
    closedir(dir);
}

void print_current_dir(void) {
    print_cwd();
    lsdir(fs_dir_buffer);
}