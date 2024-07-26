#include "assert.h"
#include "panic.h"
#include "printf.h"
#include "user.h"
#include "string.h"
///////////////////////////////////////////////////////////////////
static void forktest(char *s) {
    printf("pid %d running forktest\n", getpid());
    const int max_child = 200;
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
    printf("%s pass.\n", s);
    return;
}
/////////////////////////////////////////////////////////////////////
static void yieldtest(char *s){
    int i;
    printf("I am process %d.\n", getpid());
    for(i = 0; i < 5; i++){
        yield();
        printf("Back in process %d, iteration %d.\n", getpid(), i);
    }
    printf("All done in process %d.\n", getpid());
    return;
}
////////////////////////////////////////////////////////////////////
#define ARRAYSIZE (1024 * 1024)
uint32_t bigarray[ARRAYSIZE];

static void bsstest(char *s){
    int i;
    for (i = 0; i < ARRAYSIZE; i++) {
        if (bigarray[i] != 0) { panic("bigarray[%d] isn't cleared!\n", i); }
    }
    for (i = 0; i < ARRAYSIZE; i++) { bigarray[i] = i; }
    for (i = 0; i < ARRAYSIZE; i++) {
        if (bigarray[i] != i) { panic("bigarray[%d] didn't hold its value!\n", i); }
    }
    return;
}
////////////////////////////////////////////////////////////////////
static void exittest(char *s) {
    int magic = -0x10384;
    int pid, code;
    printf("I am the parent. Forking the child...\n");
    if ((pid = fork()) == 0) {
        printf("I am the child.\n");
        yield();
        yield();
        yield();
        yield();
        yield();
        yield();
        yield();
        exit(magic);
    }
    assert(pid > 0);
    printf("I am the parent, waiting now..\n");
    assert(waitpid(pid, &code) == 0 && code == magic);
    assert(waitpid(pid, &code) != 0 && wait() != 0);
}
////////////////////////////////////////////////////////////////////////
#define DEPTH 10
void forktree(const char *cur);

void forkchild(const char *cur, char branch){
    char nxt[DEPTH + 1];
    if(strlen(cur) >= DEPTH)
        return;
    snprintf(nxt, DEPTH + 1, "%s%c", cur, branch);
    if(fork() == 0){
        forktree(nxt);
        yield();
        exit(0);
    }
    wait();
}

void forktree(const char *cur){
    printf("%d: I am '%s'\n", getpid(), cur);
    forkchild(cur, '0');
    forkchild(cur, '1');
}

static void forktreetest(char *s){
    forktree("");
}
#undef DEPTH
//////////////////////////////////////////////////////////////////////////
static void spintest(char *s){
    int pid, ret;
    printf("I am the parent. forking the child...\n");
    if((pid = fork()) == 0){
        printf("I am the child. spinning ...\n");
        while(1);
    }
    printf("I am the parent. Running the child...\n");
    yield();
    yield();
    yield();
    printf("I am the parent.  Killing the child...\n");
    assert((ret = kill(pid)) == 0);
    printf("kill returns %d\n", ret);

    assert((ret = waitpid(pid, NULL)) == 0);
    printf("wait returns %d\n", ret);
}
//////////////////////////////////////////////////////////////////////////
static int run(void f(char *), char *s){
    int pid;
    int xstatus = 1;
    int ret = 0;
    printf("test %s: \n", s);
    if((pid = fork()) < 0){
        printf("runtest : fork error\n");
        exit(1);
    }
    if(pid == 0){
        f(s);
        exit(0);
    }else{
        ret = waitpid(0, &xstatus);
        assert(ret == 0);
        if(xstatus != 0)
            printf("%s FAILED\n", s);
        else
            printf("%s OK\n", s);
        return xstatus == 0;
    }
}

struct test{
    void (*f)(char *);
    char *s;
} tests[] = {
    {forktest, "forktest"},
    {yieldtest, "yieldtest"},
    {bsstest, "bsstest"},
    {exittest, "exittest"},
    {forktreetest, "forktreetest"},
    {spintest, "spintest"},
    {nullptr, nullptr},
};

void test_main(){
    bool fail = false;
    for(struct test *t = tests; t->s != nullptr; t++){
        if(!run(t->f, t->s))
            fail = true;
    }
    if(fail){
        printf("SOME TESTS FAILED\n");
    }else{
        printf("ALL TESTS PASSED\n");
    }

}
