#ifndef __LIBS_FILE_H__
#define __LIBS_FILE_H__
#include "stdarg.h"
#include "types.h"
#include "stat.h"
#include "printf.h"

static inline char transmode(Stat *stat){
    uint32_t mode = stat->st_mode;
    if (S_ISREG(mode)) return 'r';
    if (S_ISDIR(mode)) return 'd';
    if (S_ISLNK(mode)) return 'l';
    if (S_ISCHR(mode)) return 'c';
    if (S_ISBLK(mode)) return 'b';
    return '-';
}

static inline void print_stat(const char *name, int fd, Stat *stat) {
    printf("[%03d] %s\n", fd, name);
    printf("    mode    : %c\n", transmode(stat));
    printf("    links   : %lu\n", stat->st_nlinks);
    printf("    blocks  : %lu\n", stat->st_blocks);
    printf("    size    : %lu\n", stat->st_size);
}

#endif