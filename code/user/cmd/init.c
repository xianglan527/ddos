#include "assert.h"
#include "panic.h"
#include "printf.h"
#include "sysdef.h"
#include "user.h"
char *sh_argv[] = {"sh", 0};

int main(int argc, char *argv[]) {
    prework();
    // for (int i = 0; i < argc; i++) { printf("arg %d is %s\n", i, argv[i]); }
    printf("welcome user space\n");
    // test_main();
    int pid, ret, xstatus;
    for(;;){
        printf("init: starting sh\n");
        pid = fork();
        assert(pid >= 0);
        if(pid == 0){
            exec("sh", sh_argv);
            printf("init: exec sh failed\n");
            exit(1);
        }
        for(;;){
            ret = waitpid(0, &xstatus);
            if(ret == 0){
                if(xstatus != 0){
                    warn("init: sh exit status is %d\n", xstatus);
                }
                break;  // the shell exited; restart it.
            }else{
                panic("init: sh returned an %d error\n", ret);
            }
        }
    }
}