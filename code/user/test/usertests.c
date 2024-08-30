#include "assert.h"
#include "error.h"
#include "lock.h"
#include "malloc.h"
#include "panic.h"
#include "printf.h"
#include "string.h"
#include "sysdef.h"
#include "thread.h"
#include "user.h"
///////////////////////////////////////////////////////////////////
static void forktest(char *s) {
    printf("pid %d running forktest\n", getpid());
    const int max_child = 500;
    int n, pid;
    size_t nr_free_pages_store1 = get_free_page_size();
    for (n = 0; n < max_child; n++) {
        if ((pid = fork()) == 0) {
            printf("I am child pid is:%d\n", getpid());
            exit(0);
        }
        assert(pid > 0);
    }
    size_t nr_free_pages_store2 = get_free_page_size();
    if (n > max_child) { panic("fork claimed to work %d times\n", n); }
    for (; n > 0; n--) {
        if (wait() != 0) { panic("wait stopped early\n"); }
    }
    size_t nr_free_pages_store3 = get_free_page_size();
    if (wait() == 0) { panic("wait got too many\n"); }
    size_t nr_free_pages_store4 = get_free_page_size();
    printf("%s pass.\n", s);
    return;
}
/////////////////////////////////////////////////////////////////////
static void yieldtest(char *s) {
    int i;
    printf("I am process %d.\n", getpid());
    for (i = 0; i < 5; i++) {
        yield();
        printf("Back in process %d, iteration %d.\n", getpid(), i);
    }
    printf("All done in process %d.\n", getpid());
    return;
}
////////////////////////////////////////////////////////////////////
#define ARRAYSIZE (1024 * 1024)
uint32_t bigarray[ARRAYSIZE];

static void bsstest(char *s) {
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
#define DEPTH 9
void forktree(const char *cur);

void forkchild(const char *cur, char branch) {
    char nxt[DEPTH + 1];
    if (strlen(cur) >= DEPTH) return;
    snprintf(nxt, DEPTH + 1, "%s%c", cur, branch);
    if (fork() == 0) {
        forktree(nxt);
        yield();
        exit(0);
    }
    wait();
}

void forktree(const char *cur) {
    printf("%d: I am '%s'\n", getpid(), cur);
    forkchild(cur, '0');
    forkchild(cur, '1');
}

static void forktreetest(char *s) { forktree(""); }
#undef DEPTH
//////////////////////////////////////////////////////////////////////////
static void spintest(char *s) {
    int pid, ret;
    printf("I am the parent. forking the child...\n");
    if ((pid = fork()) == 0) {
        printf("I am the child. spinning ...\n");
        while (1);
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
struct slot {
    char data[4096];
    struct slot *next;
};
static void brktest(char *s) {
    struct slot *tmp, *head = nullptr;
    int n = 0, rounds = 20;
    printf("I am going to eat out all the mem.\n");
    while (rounds > 0 && (tmp = (struct slot *)malloc(sizeof(*tmp))) != nullptr) {
        if ((++n) % 1000 == 0) {
            printf("I ate %d slots.\n", n);
            rounds--;
        }
        tmp->next = head;
        head = tmp;
        head->data[0] = (char)n;
    }
    printf("I ate (at least) %d byte memory.\n", n * sizeof(struct slot));
    int error = 0;
    while (head != nullptr) {
        if (head->data[0] != (char)(n--)) { error++; }
        tmp = head->next;
        free(head);
        head = tmp;
    }
    assert(error == 0);
    printf("I free all the memory.(%d)\n", error);
}
//////////////////////////////////////////////////////////////////////////
static void __brkfreetest(void) {
    uintptr_t oldbrk = 0;
    assert(sbrk(&oldbrk) == 0);
    uintptr_t newbrk = oldbrk + 4096;
    assert(sbrk(&newbrk) == 0 && newbrk >= oldbrk + 4096);
    char *p = (void *)oldbrk;
    int i;
    for (i = 0; i < 4096; i++) { p[i] = (char)(i * 31 + (i & 0xF)); }
    for (i = 0; i < 4096; i++) { assert(p[i] == (char)(i * 31 + (i & 0xF))); }
    newbrk = oldbrk;
    assert(sbrk(&newbrk) == 0 && newbrk == oldbrk);
    // printf("page fault!!\n");
    // p[0] = 0;
}

static void brkfreetest(char *s) {
    int pid, exit_code;
    if ((pid = fork()) == 0) {
        __brkfreetest();
        exit(0xdead);
    }
    assert(pid > 0);
    assert(waitpid(pid, &exit_code) == 0 && exit_code == 0xdead);
}
//////////////////////////////////////////////////////////////////////////
static void sleepkilltest(char *s) {
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
static void glutton(void) {
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
    int i, time = 10;
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
    assert(waitpid(pid1, &exit_code) == 0 && exit_code == E_KILLED);
    assert(pid2 > 0);

    assert(waitpid(pid2, &exit_code) == 0 && exit_code == 0);
    printf("use %04d ticks.\n", gettime() - time);
}
//////////////////////////////////////////////////////////////////////////
// struct slot {
//     char data[4096];
//     struct slot *next;
// };

static struct slot *expand(int num) {
    struct slot *tmp, *head = NULL;
    while (num > 0) {
        tmp = (struct slot *)malloc(sizeof(struct slot));
        tmp->next = head;
        head = tmp;
        num--;
    }
    return head;
}

struct slot *cowtest_head;

static void sweeper(void) {
    struct slot *p = cowtest_head;
    while (p != NULL) {
        p->data[0] = (char)0xEF;
        p = p->next;
    }
    p = cowtest_head;
    while (p != NULL) {
        assert(p->data[0] == (char)0xEF);
        p = p->next;
    }
    exit(0xbeaf);
}
int pid, exit_code;

static void cowtest(char *s) {
    cowtest_head = expand(6000);
    // int pid, exit_code;
    if ((pid = fork()) == 0) { sweeper(); }
    assert(pid > 0);
    assert(waitpid(pid, &exit_code) == 0 && exit_code == 0xbeaf);
}
//////////////////////////////////////////////////////////////////////////
const int swaptest_size = 5 * 1024 * 1024;
char *swaptest_buffer;

int swaptest_pid[200] = {0}, swaptest_pids;

static void do_yield(void) {
    int i;
    for (i = 0; i < 5; i++) { yield(); }
}

static void swaptest_work(int num) {
    // do_yield();
    int i, j;
    for (i = 0; i < swaptest_size; i++) { assert(swaptest_buffer[i] == (char)(i * i)); }
    char c = (char)num;
    // do_yield();
    for (i = 0; i < 5; i++, c++) {
        memset(swaptest_buffer, c, swaptest_size);
        for (j = 0; j < swaptest_size; j++) { assert(swaptest_buffer[i] == c); }
    }
    // do_yield();
    printf("proc pid %d has completed work\n", num);
}

static void swaptest(char *s) {
    // int swaptest_pid[10] = {0}, swaptest_pids;
    assert((swaptest_buffer = malloc(swaptest_size)) != NULL);
    printf("swaptest_bufferr size = 0x%08x\n", swaptest_size);

    swaptest_pids = sizeof(swaptest_pid) / sizeof(swaptest_pid[0]);

    int i;
    for (i = 0; i < swaptest_size; i++) { swaptest_buffer[i] = (char)(i * i); }
    for (i = 0; i < swaptest_pids; i++) {
        if ((swaptest_pid[i] = fork()) == 0) {
            // sleep((swaptest_pids - i) * 10);
            printf("child %d fork ok, pid = %d.\n", i, getpid());
            sleep(50);
            swaptest_work(getpid());
            exit(0xbee);
        }
        assert(swaptest_pid[i] > 0);
    }
    printf("parent init ok.\n");
    for (i = 0; i < swaptest_pids; i++) {
        int exit_code, ret;
        ret = waitpid(swaptest_pid[i], &exit_code);
        assert(ret == 0 && exit_code == 0xbee);
    }
    printf("wait ok.\n");
    for (i = 0; i < swaptest_size; i++) { assert(swaptest_buffer[i] == (char)(i * i)); }
}
//////////////////////////////////////////////////////////////////////////
static void mmaptest(char *s) {
    const int size = 4096;
    void *mapped[10] = {nullptr};
    uintptr_t addr = 0;
    assert(mmap(nullptr, size, 0) != 0);
    int i;
    for (i = 0; i < 10; i++) {
        assert(mmap(&addr, size, MMAP_WRITE) == 0 && addr != 0);
        mapped[i] = (void *)addr, addr = 0;
    }
    addr = 0x90000000;
    assert(mmap(&addr, size, MMAP_WRITE) == 0);
    mapped[0] = (void *)addr;

    addr = 0x90000000 + 0x1000;
    assert(mmap(&addr, size, MMAP_WRITE) == 0);
    mapped[1] = (void *)addr;

    addr = 0x90000000 + 0x1800;
    assert(mmap(&addr, size, MMAP_WRITE) != 0);

    assert(munmap((uintptr_t)mapped[0], size * 2 + 100) == 0);

    addr = 0;
    assert(mmap(&addr, 128, MMAP_WRITE) == 0 && addr != 0);
    mapped[0] = (void *)addr;

    char *buffer = mapped[0];
    for (i = 0; i < 128; i++) { buffer[i] = (char)(i * i); }
    for (i = 0; i < 128; i++) { assert(buffer[i] == (char)(i * i)); }
}
//////////////////////////////////////////////////////////////////////////
void *shmembuf1, *shmembuf2;
static void shmemtest(char *s) {
    assert((shmembuf1 = shmem_malloc(8192)) != nullptr);
    assert((shmembuf2 = malloc(4096)) != nullptr);

    int i;
    for (i = 0; i < 4096; i++) { *(char *)(shmembuf1 + i) = (char)i; }
    memset(shmembuf2, 0, 4096);
    int pid, exit_code;
    if ((pid = fork()) == 0) {
        for (i = 0; i < 4096; i++) { assert(*(char *)(shmembuf1 + i) == (char)i); }
        memcpy(shmembuf1 + 4096, shmembuf1, 4096);
        memset(shmembuf1, 0, 4096);
        memset(shmembuf2, 0xff, 4096);
        exit(0);
    }
    assert(pid > 0 && waitpid(pid, &exit_code) == 0 && exit_code == 0);

    for (i = 0; i < 4096; i++) {
        assert(*(char *)(shmembuf1 + 4096 + i) == (char)i);
        assert(*(char *)(shmembuf1 + i) == 0);
        assert(*(char *)(shmembuf2 + i) == 0);
    }
    free(shmembuf1);
    free(shmembuf2);
}
//////////////////////////////////////////////////////////////////////////
static int __threadtest(void *arg) {
    printf("child ok.\n");
    return 0xbee;
}

static void threadtest(char *s) {
    Thread tid;
    assert(thread(__threadtest, nullptr, &tid) == 0);
    printf("thread ok.\n");

    int exit_code;
    assert(thread_wait(&tid, &exit_code) == 0 && exit_code == 0xbee);
}
//////////////////////////////////////////////////////////////////////////
const int threadfork_forknum = 125;
static void threadfork_do_yield(void) {
    for (int i = 0; i < 30; i++) { yield(); }
}

static int threadfork_thread_main(void *arg) {
    int pid;
    for (int i = 0; i < threadfork_forknum; i++) {
        if ((pid = fork()) == 0) {
            printf("threadfork_thread_main i : %d.\n", i);
            threadfork_do_yield();
            exit(0);
        }
    }
    threadfork_do_yield();
    return 0;
}

static void threadforktest(char *s) {
    Thread tids[10];
    int n = sizeof(tids) / sizeof(tids[0]);
    for (int i = 0; i < n; i++) { assert(thread(threadfork_thread_main, nullptr, tids + i) == 0); }
    int count = 0;
    while (wait() == 0) { count++; }
    assert(count == (threadfork_forknum + 1) * n);
}
//////////////////////////////////////////////////////////////////////////
char **threadwork_buffer;
Thread *threadwork_tids;
const int threadwork_size = 100, threadwork_rounds = 100;

int threadwork_work(void *arg) {
    long n = (long)arg;
    long value = n * n * 527;
    printf("i am %d, %d, i got %d\n", n, getpid(), value);
    yield();
    int i, j;
    for (i = 0; i < threadwork_size; i++) {
        for (j = n; j < threadwork_size * threadwork_rounds; j += threadwork_size) {
            threadwork_buffer[i][j] = (char)value;
        }
    }
    return 0xbee;
}

int threadwork_loop(void *arg) {
    printf("child: do nothing\n");
    while (1);
}

static void threadworktest(char *s) {
    threadwork_buffer = (char **)malloc(sizeof(char *) * threadwork_size);
    int i, j, k, ret;
    for (i = 0; i < threadwork_size; i++) {
        assert((threadwork_buffer[i] = (char *)malloc(sizeof(char) * threadwork_size * threadwork_rounds)) !=
               nullptr);
    }
    assert((threadwork_tids = (Thread *)malloc(sizeof(Thread) * threadwork_size)) != nullptr);
    memset(threadwork_tids, 0, sizeof(Thread) * threadwork_size);
    for (i = 0; i < threadwork_size; i++) {
        assert(thread(threadwork_work, (void *)(long)i, threadwork_tids + i) == 0);
    }
    printf("thread ok.\n");
    for (i = 0; i < threadwork_size; i++) {
        int exit_code = 0;
        assert(thread_wait(threadwork_tids + i, &exit_code) == 0);
        assert(exit_code == 0xbee);
    }
    printf("thread wait ok.\n");
    for (k = 0; k < threadwork_size; k++) {
        long value = k * k * 527;
        for (i = 0; i < threadwork_size; i++) {
            for (j = k; j < threadwork_size * threadwork_rounds; j += threadwork_size) {
                assert(threadwork_buffer[i][j] == (char)value);
            }
        }
    }
    Thread loop_tid;
    assert(thread(threadwork_loop, nullptr, &loop_tid) == 0);
    printf("loop init ok.\n");
    assert(thread_kill(&loop_tid) == 0);
    assert(wait() == 0 && wait() != 0);
}
//////////////////////////////////////////////////////////////////////////
int prime_total = 200;

int *prime_note;
lock_t *prime_locks;

void *safe_shmem_malloc(size_t size) {
    void *ret;
    if ((ret = shmem_malloc(size)) == NULL) { panic("shmem_malloc error.\n"); }
    return ret;
}

int prime_read(int index) {
    lock_t *l = prime_locks + index;
    int ret;
try_again:
    lock(l);
    if ((ret = prime_note[index]) > 0) { prime_note[index] = 0; }
    unlock(l);
    if (ret == 0) {
        yield();
        goto try_again;
    }
    return ret;
}

int prime_write(int index, int val, bool force) {
    lock_t *l = prime_locks + index;
    int ret;
try_again:
    lock(l);
    if ((ret = prime_note[index]) >= 0) {
        if (ret == 0 || force) { prime_note[index] = val; }
    }
    unlock(l);
    if (ret > 0 && !force) {
        yield();
        goto try_again;
    }
    return (ret > 0) ? 0 : ret;
}

void primeproc(void) {
    int index = 0, this, num, pid = 0;
top:
    this = prime_read(index);
    printf("%d is a primer.\n", this);

    while ((num = prime_read(index)) > 0) {
        if ((num % this) == 0) { continue; }
        if (pid == 0) {
            if (index + 1 == prime_total || (pid = fork()) < 0) { goto out; }
            if (pid == 0) {
                index++;
                goto top;
            }
        }
        if (prime_write(index + 1, num, 0) != 0) { goto out; }
    }

out:
    printf("[%04d] %d quit.\n", getpid(), index);
    prime_write(index, -1, 1);
    wait();
    exit(0);
}

static void primeworktest(char *s) {
    prime_note = safe_shmem_malloc(prime_total * sizeof(int));
    prime_locks = safe_shmem_malloc(prime_total * sizeof(lock_t));

    int i, pid;
    for (i = 0; i < prime_total; i++) {
        prime_note[i] = 0;
        lock_init(prime_locks + i);
    }

    printf("sharemem init ok.\n");

    unsigned int time = gettime();

    if ((pid = fork()) == 0) {
        primeproc();
        exit(0);
    }
    assert(pid > 0);

    for (i = 2;; i++) {
        if (prime_write(0, i, 0) != 0) { break; }
    }
    wait();
    printf("use %d ticks.\n", gettime() - time);
}
//////////////////////////////////////////////////////////////////////////
static int run(void f(char *), char *s) {
    int pid;
    int xstatus = 1;
    int ret = 0;
    printf("test %s: \n", s);
    size_t nr_free_pages_store = get_free_page_size();
    size_t slab_allocated_store = get_slab_allocated_size();
    if ((pid = fork()) < 0) {
        printf("runtest : fork error\n");
        exit(1);
    }
    if (pid == 0) {
        f(s);
        exit(0);
    } else {
        ret = waitpid(0, &xstatus);
        // sleep(100);
        assert(ret == 0);
        if (xstatus != 0)
            printf("%s FAILED\n", s);
        else
            printf("%s OK\n", s);
        size_t nr_free_pages_store1 = get_free_page_size();
        size_t slab_allocated_store1 = get_slab_allocated_size();
        // printf("%d page nums diff\n", nr_free_pages_store - nr_free_pages_store1);
        // printf("%d slab nums diff\n", slab_allocated_store1 - slab_allocated_store);
        // while(1);
        assert(nr_free_pages_store == nr_free_pages_store1);
        assert(slab_allocated_store == slab_allocated_store1);
        return xstatus == 0;
    }
}

struct test {
    void (*f)(char *);
    char *s;
} tests[] = {
    {mmaptest, "mmaptest"},
    {shmemtest, "shmemtest"},
    {threadtest, "threadtest"},
    {threadforktest, "threadforktest"},
    {threadworktest, "threadworktest"},
    {primeworktest, "primeworktest"},
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
    {cowtest, "cowtest"},
    {swaptest, "swaptest"},
    {nullptr, nullptr},
};

void test_main() {
    bool fail = false;
    for (struct test *t = tests; t->s != nullptr; t++) {
        if (!run(t->f, t->s)) fail = true;
    }
    if (fail) {
        printf("SOME TESTS FAILED\n");
    } else {
        printf("ALL TESTS PASSED\n");
    }
}
