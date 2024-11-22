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
#endif