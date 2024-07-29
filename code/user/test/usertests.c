#include "assert.h"
#include "panic.h"
#include "printf.h"
#include "user.h"
#include "string.h"
#include "malloc.h"
#include "error.h"
///////////////////////////////////////////////////////////////////
static void forktest(char *s) {
    printf("pid %d running forktest\n", getpid());
    const int max_child = 500;
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
#define DEPTH 8
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
struct slot{
    char data[4096];
    struct slot *next;
};
static void brktest(char *s){
    struct slot *tmp, *head = nullptr;
    int n = 0, rounds = 20;
    printf("I am going to eat out all the mem.\n");
    while(rounds > 0 && (tmp = (struct slot *)malloc(sizeof(*tmp))) != nullptr){
        if((++n) % 1000 == 0){
            printf("I ate %d slots.\n", n);
            rounds--;
        }
        tmp->next = head;
        head = tmp;
        head->data[0] = (char)n;
    }
    printf("I ate (at least) %d byte memory.\n", n * sizeof(struct slot));
    int error = 0;
    while(head != nullptr){
        if(head->data[0] != (char)(n--)){
            error++;
        }
        tmp = head->next;
        free(head);
        head = tmp;
    }
    assert(error == 0);
    printf("I free all the memory.(%d)\n", error);
}
//////////////////////////////////////////////////////////////////////////
static void __brkfreetest(void){
    uintptr_t oldbrk = 0;
    assert(sbrk(&oldbrk) == 0);
    uintptr_t newbrk = oldbrk + 4096;
    assert(sbrk(&newbrk) == 0 && newbrk >= oldbrk + 4096);
    char *p = (void *)oldbrk;
    int i;
    for(i = 0; i < 4096; i++){
        p[i] = (char)(i * 31 + (i & 0xF));
    }
    for (i = 0; i < 4096; i++) { assert(p[i] == (char)(i * 31 + (i & 0xF))); }
    newbrk = oldbrk;
    assert(sbrk(&newbrk) == 0 && newbrk == oldbrk);
    // printf("page fault!!\n");
    // p[0] = 0;
}

static void brkfreetest(char *s){
    int pid, exit_code;
    if((pid = fork()) == 0){
        __brkfreetest();
        exit(0xdead);
    }
    assert(pid > 0);
    assert(waitpid(pid, &exit_code) == 0 && exit_code == 0xdead);
}
//////////////////////////////////////////////////////////////////////////
static void sleepkilltest(char *s){
    int ret;
    int pid;
    if ((pid = fork()) == 0) {
        sleep(~0);
        exit(0xdead);
    }
    assert(pid > 0);
    sleep(100);
    assert(kill(pid) == 0);
    assert((ret = waitpid(pid, NULL)) == 0);
}
//////////////////////////////////////////////////////////////////////////
// struct slot {
//     char data[4096];
//     struct slot *next;
// };
static void glutton(void){
    struct slot *tmp, *head = NULL;
    int n = 0;
    printf("I am child and I will eat out all the memory.\n");
    while ((tmp = (struct slot *)malloc(sizeof(struct slot))) != NULL) {
        if ((++n) % 1000 == 0) {
            printf("I ate %d slots.\n", n);
            sleep(50);
        }
        tmp->next = head;
        head = tmp;
    }
    exit(0xdead);
}

static void sleepy(int pid) {
    int i, time = 100;
    for (i = 0; i < 10; i++) {
        sleep(time);
        printf("sleep %d x %d slices.\n", i + 1, time);
    }
    assert(kill(pid) == 0);
    exit(0);
}

static void sleeptest(char *s) {
    unsigned int time = gettime();
    int pid1, pid2, exit_code;
    if ((pid1 = fork()) == 0) { glutton(); }
    assert(pid1 > 0);

    if ((pid2 = fork()) == 0) { sleepy(pid1); }
    assert(waitpid(pid1, &exit_code) == 0 && exit_code == -E_KILLED);
    assert(pid2 > 0);

    assert(waitpid(pid2, &exit_code) == 0 && exit_code == 0);
    printf("use %04d msecs.\n", gettime() - time);
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
    {brktest, "brktest"},
    {brkfreetest, "brkfreetest"},
    {sleepkilltest, "sleepkilltest"},
    {sleeptest, "sleeptest"},
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
