#ifndef __LIBS_DIR_H__
#define __LIBS_DIR_H__
#include "stdarg.h"
#include "types.h"
#include "user.h"

typedef struct{
    int fd;
    Dirent dirent;
}DIR;

DIR *opendir(char *path);
Dirent *readdir(DIR *dir);
void closedir(DIR *dir);

void print_cwd(void);
void lsdir(char *path);
void print_current_dir(void);
#endif