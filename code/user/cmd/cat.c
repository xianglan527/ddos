#include "assert.h"
#include "dir.h"
#include "error.h"
#include "file.h"
#include "lock.h"
#include "malloc.h"
#include "panic.h"
#include "printf.h"
#include "rand.h"
#include "spipe.h"
#include "string.h"
#include "sysdef.h"
#include "thread.h"
#include "user.h"

char buf[SFS_BSIZE];

static void safe_write(int fd, void *data, size_t len) {
    long ret = write(fd, data, len);
    assert(ret == len);
}

void cat(int fd){
    long n;
    while((n = read(fd, buf, sizeof(buf))) > 0){
        if(write(1, buf, n) != n){
            fprintf(2, "cat: write error\n");
            exit(1);
        }
    }
    if(n < 0){
        fprintf(2, "cat: read error\n");
        exit(n);
    }
}

int main(int argc, char *argv[]){
    // prework();
    int fd;
    if(argc <= 1){
        cat(0);
        exit(0);
    }
    for(int i = 1; i < argc; i++){
        if((fd = open(argv[i], 0)) < 0){
            fprintf(2, "cat: cannot open %s\n", argv[i]);
            exit(fd);
        }
        cat(fd);
        close(fd);
    }
    exit(0);
}