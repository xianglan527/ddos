#include "user.h"
#include "printf.h"
#include "assert.h"
#include "panic.h"
#include "usertests.h"
#include "sysdef.h"

static int initfd(int fd2,char *path, uint32_t open_flags){
    int fd1, ret;
    if((fd1 = open(path, open_flags)) < 0){
        return fd1;
    }
    if(fd1 != fd2){
        close(fd2);
        ret = dup2(fd1, fd2);
        close(fd1);
    }
    return ret;
}

static void prework(void){
    int fd;
    if((fd = initfd(0, "stdin:", O_RDONLY)) < 0){
        while(1);
    }
    if ((fd = initfd(1, "stdout:", O_WRONLY)) < 0) {
        while(1);
    }
}

int main(void){
    prework();
    printf("welcome user space\n");
    test_main();
    return 0;
}