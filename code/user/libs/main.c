#include "user.h"
#include "printf.h"
#include "assert.h"
#include "panic.h"

void forktest(void){
    printf("pid %d running forktest\n", getpid());
    const int max_child = 128;
    int n, pid;
    for (n = 0; n < max_child; n++) {
        if ((pid = fork()) == 0) {
            printf("I am child pid is:%d\n", getpid());
            exit(0);
        }
        assert(pid > 0);
    }
    if (n > max_child) { panic("fork claimed to work %d times\n", n); }
    for (; n > 0; n--) {
        if (wait() != 0) { panic("wait stopped early\n"); }
    }
    if (wait() == 0) { panic("wait got too many\n"); }
    printf("forktest pass.\n");
    return;
}

int main(void){
    printf("welcome user space\n");
    forktest();
    return 0;
}