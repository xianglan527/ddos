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

#define UNIQUE_VAR(name) name##__LINE__
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
#define DEPTH 8
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
static void sem_test(char *s) {
    sem_t sem_id = sem_init(1);
    assert(sem_id > 0);
    printf("sem_id = 0x%ld\n", sem_id);

    int i, value;
    for (i = 0; i < 10; i++) {
        assert(sem_get_value(sem_id, &value) == 0);
        assert(value == i + 1 && sem_post(sem_id) == 0);
    }
    printf("post ok.\n");

    for (; i > 0; i--) {
        assert(sem_wait(sem_id) == 0);
        assert(sem_get_value(sem_id, &value) == 0 && value == i);
    }
    printf("wait ok.\n");

    int pid, ret;
    if ((pid = fork()) == 0) {
        assert(sem_get_value(sem_id, &value) == 0);
        assert(value == 1 && sem_wait(sem_id) == 0);

        sleep(10);
        for (i = 0; i < 10; i++) {
            printf("sleep %d\n", i);
            sleep(20);
        }
        assert(sem_post(sem_id) == 0);
        exit(0);
    }
    assert(pid > 0);
    sleep(10);
    for (i = 0; i < 10; i++) { yield(); }

    printf("wait semaphore...\n");
    assert(sem_wait(sem_id) == 0);
    assert(sem_get_value(sem_id, &value) == 0 && value == 0);
    printf("hold semaphore.\n");
    assert(waitpid(pid, &ret) == 0 && ret == 0);
    assert(sem_get_value(sem_id, &value) == 0 && value == 0);
}
//////////////////////////////////////////////////////////////////////////
sem_t sem_rw_count, sem_rw_write;
int *sem_rw_pcount;

static int sem_rw_check_sem_value_sub(sem_t sem_id, int value) {
    int value_store;
    if (sem_get_value(sem_id, &value_store) != 0 || value_store != value) { return -1; }
    return 0;
}

static void sem_rw_check_sem_value(void) {
    assert(sem_rw_check_sem_value_sub(sem_rw_count, 1) == 0 &&
           sem_rw_check_sem_value_sub(sem_rw_write, 1) == 0);
    assert(*sem_rw_pcount == 0);
}

static void sem_rw_init(void) {
    assert((sem_rw_count = sem_init(1)) > 0 && (sem_rw_write = sem_init(1)) > 0);
    assert((sem_rw_pcount = shmem_malloc(sizeof(int))) != nullptr);
    *sem_rw_pcount = 0;
}

static void sem_rw_reader(int id, int time) {
    printf("reader %d: (pid:%d) arrive\n", id, getpid());
    sem_wait(sem_rw_count);
    if (*sem_rw_pcount == 0) { sem_wait(sem_rw_write); }
    (*sem_rw_pcount)++;
    sem_post(sem_rw_count);

    printf("    reader_rw %d: (pid:%d) start %d\n", id, getpid(), time);
    sleep(time);
    printf("    reader_rw %d: (pid:%d) end %d\n", id, getpid(), time);

    sem_wait(sem_rw_count);
    (*sem_rw_pcount)--;
    if (*sem_rw_pcount == 0) { sem_post(sem_rw_write); }
    sem_post(sem_rw_count);
}

static void sem_rw_writer(int id, int time) {
    printf("writer %d: (pid:%d) arrive\n", id, getpid());
    sem_wait(sem_rw_write);

    printf("    writer_rw %d: (pid:%d) start %d\n", id, getpid(), time);
    sleep(time);
    printf("    writer_rw %d: (pid:%d) end %d\n", id, getpid(), time);

    sem_post(sem_rw_write);
}

static void sem_rw_read_test(void) {
    printf("---------------------------------\n");
    sem_rw_check_sem_value();
    srand(0);
    int i, total = 10, time;
    for (i = 0; i < total; i++) {
        time = (unsigned int)simulate_rand() % 3;
        if (fork() == 0) {
            yield();
            sem_rw_reader(i, 100 + time * 10);
            exit(0);
        }
    }
    for (i = 0; i < total; i++) { assert(wait() == 0); }
    printf("read_test ok.\n");
}

static void sem_rw_write_test(void) {
    printf("---------------------------------\n");
    sem_rw_check_sem_value();
    srand(100);
    int i, total = 10, time;
    for (i = 0; i < total; i++) {
        time = (unsigned int)simulate_rand() % 3;
        if (fork() == 0) {
            yield();
            sem_rw_writer(i, 100 + time * 10);
            exit(0);
        }
    }
    for (i = 0; i < total; i++) { assert(wait() == 0); }
    printf("write_test ok.\n");
}

static void sem_rw_read_write_test(void) {
    printf("---------------------------------\n");
    sem_rw_check_sem_value();
    srand(200);
    int i, total = 10, time;
    for (i = 0; i < total; i++) {
        time = (unsigned int)simulate_rand() % 3;
        if (fork() == 0) {
            yield();
            if (time == 0) {
                sem_rw_writer(i, 100 + time * 10);
            } else {
                sem_rw_reader(i, 100 + time * 10);
            }
            exit(0);
        }
    }
    for (i = 0; i < total; i++) { assert(wait() == 0); }
    printf("read_write_test ok.\n");
}

static void sem_rw_test(char *s) {
    sem_rw_init();
    sem_rw_read_test();
    sem_rw_write_test();
    sem_rw_read_write_test();
}
//////////////////////////////////////////////////////////////////////////
int prime2_total = 100;

int *prime2_note;

// void *safe_shmem_malloc(size_t size) {
//     void *ret;
//     if ((ret = shmem_malloc(size)) == NULL) { panic("shmem_malloc error.\n"); }
//     return ret;
// }

int prime2_read(int index, sem_t sem[]) {
    int ret;
    if (sem_wait(sem[0]) != 0) { return -1; }
    ret = prime2_note[index];
    // if ((ret = prime2_note[index]) > 0) { prime2_note[index] = 0; }
    if (sem_post(sem[1]) != 0) { return -1; }
    return ret;
}

int prime2_write(int index, sem_t sem[], int val) {
    int ret;
    if (sem_wait(sem[1]) != 0) { return -1; }
    ret = prime2_note[index], prime2_note[index] = val;
    if (sem_post(sem[0]) != 0) { return -1; }
    return (ret >= 0) ? 0 : -1;
}

void read_and_quit(int index, sem_t sem[]) {
    sem_wait(sem[0]);
    prime2_note[index] = -1;
    sem_post(sem[1]);
}

void prime2proc(sem_t sem[]) {
    int index = 0, this, num, pid = 0;
    sem_t next_sem[2];
top:
    this = prime2_read(index, sem);
    printf("%d is a prime2r.\n", this);

    while ((num = prime2_read(index, sem)) > 0) {
        if ((num % this) == 0) { continue; }
        if (pid == 0) {
            if (index + 1 == prime2_total) { goto out; }
            assert((next_sem[0] = sem_init(0)) > 0 && (next_sem[1] = sem_init(1)) > 0);
            if ((pid = fork()) == 0) {
                sem[0] = next_sem[0];
                sem[1] = next_sem[1];
                index++;
                goto top;
            }
            assert(pid > 0);
        }
        if (prime2_write(index + 1, next_sem, num) != 0) { goto out; }
    }

out:
    printf("[%04d] %d quit.\n", getpid(), index);
    read_and_quit(index, sem);
    wait();
    exit(0);
}

static void prime2worktest(char *s) {
    prime2_note = safe_shmem_malloc(prime2_total * sizeof(int));
    sem_t sem[2];
    assert((sem[0] = sem_init(0)) > 0 && (sem[1] = sem_init(1)) > 0);

    int i, pid;

    printf("sharemem init ok.\n");

    unsigned int time = gettime();

    if ((pid = fork()) == 0) {
        prime2proc(sem);
        exit(0);
    }
    assert(pid > 0);

    for (i = 2;; i++) {
        if (prime2_write(0, sem, i) != 0) { break; }
    }
    wait();
    printf("use %d ticks.\n", gettime() - time);
}
//////////////////////////////////////////////////////////////////////////
Spipe pipe;
Thread spipetest_tids[10];
int spipetest_total = sizeof(spipetest_tids) / sizeof(spipetest_tids[0]);

int spipetest_thread_main(void *arg) {
    long id = (long)arg;
    printf("this is %d\n", id);
    size_t n = 1000;
    char *buf = malloc(sizeof(char) * n);
    assert(buf != nullptr);
    memset(buf, (char)id, n);
    int i, rounds = 20;
    for (i = 0; i < rounds; i++) {
        size_t ret = spipe_write(&pipe, buf, n);
        if (ret != n) {
            printf("pipe is closed, too early.\n");
            return -1;
        }
        if (id == 0) { printf("send %d/%d\n", i, rounds); }
    }
    return 0;
}

void spipetest_process_main(void) {
    int counts[spipetest_total], i;
    for (i = 0; i < spipetest_total; i++) { counts[i] = 0; }
    char buf[128];
    size_t n = sizeof(buf);
    while (1) {
        size_t ret = spipe_read(&pipe, buf, n);
        if (ret == 0) { break; }
        for (i = 0; i < ret; i++) { counts[((ulong)buf[i]) % spipetest_total]++; }
    }
    if (spipe_isclosed(&pipe)) {
        for (i = 0; i < spipetest_total; i++) { printf("%d reads %d\n", i, counts[i]); }
        exit(0);
    }
    exit(0xbad);
}

static void spipetest(char *s) {
    int pid, i;
    spipe(&pipe);
    if ((pid = fork()) == 0) { spipetest_process_main(); }
    assert(pid > 0);
    memset(spipetest_tids, 0, sizeof(Thread) * spipetest_total);
    for (i = 0; i < spipetest_total; i++) {
        assert(thread(spipetest_thread_main, (void *)(long)i, spipetest_tids + i) == 0);
    }
    int exit_code;
    for (i = 0; i < spipetest_total; i++) {
        assert(thread_wait(spipetest_tids + i, &exit_code) == 0 && exit_code == 0);
    }
    for (i = 0; i < spipetest_total; i++) { yield(); }
    spipe_close(&pipe);
    assert(waitpid(pid, &exit_code) == 0 && exit_code == 0);
}
//////////////////////////////////////////////////////////////////////////
sem_t sem2_mutex;
static void sem2_test(char *s) {
    assert((sem2_mutex = sem_init(0)) > 0);

    printf("wait now...\n");

    assert(sem_wait_timeout(sem2_mutex, 500) == -E_TIMEOUT);

    printf("wait timeout\n");

    int pid;
    if ((pid = fork()) == 0) {
        printf("child now sleep\n");
        sleep(500);
        sem_post(sem2_mutex);
        exit(0);
    }
    assert(pid > 0);

    yield();
    assert(sem_wait_timeout(sem2_mutex, ~0) == 0);
    int exit_code;
    assert(waitpid(pid, &exit_code) == 0 && exit_code == 0);
}
//////////////////////////////////////////////////////////////////////////
sem_t sem3_mutex;

static void sem3_test(char *s) {
    assert((sem3_mutex = sem_init(1)) > 0);
    assert(sem_post(sem3_mutex) == 0);

    int value, pid;
    assert(sem_get_value(sem3_mutex, &value) == 0 && value == 2);
    assert(sem_free(sem3_mutex) == 0 && sem_get_value(sem3_mutex, &value) != 0);

    assert((sem3_mutex = sem_init(0)) > 0);
    if ((pid = fork()) == 0) {
        assert(sem_wait(sem3_mutex) != 0);
        printf("child exit ok.\n");
        exit(0);
    }
    assert(pid > 0);
    assert(sem_free(sem3_mutex) == 0 && waitpid(pid, &value) == 0 && value == 0);
}
//////////////////////////////////////////////////////////////////////////
void __event_test1(void) {
    int pid, parent = getpid();
    if ((pid = fork()) == 0) {
        int event_num;
        while (event_recv(&pid, &event_num) == 0 && parent == pid) {
            if (event_num == -1) {
                printf("child1 Hmmm!\n");
                sleep(100);
                printf("child1 quit\n");
                exit(-1);
            }
            printf("child1 receive %08x from %d\n", event_num, pid);
        }
    }
    assert(pid > 0);
    int i = 10;
    while (event_send(pid, i) == 0) {
        i--;
        sleep(50);
    }
    int value;
    assert(waitpid(pid, &value) == 0 && value == -1);
}

void __event_test2(void) {
    int pid;
    if ((pid = fork()) == 0) {
        printf("child2 is spinning...\n");
        while (1);
    }
    assert(pid > 0);
    assert(event_send_timeout(pid, 0xbee, 100) == -E_TIMEOUT);
    kill(pid);
    wait();
}

void __event_test3(void) {
    int pid;
    if ((pid = fork()) == 0) {
        int event;
        assert(event_recv_timeout(NULL, &event, 100) == -E_TIMEOUT);
        exit(0);
    }
    assert(pid > 0);
    assert(waitpid(pid, NULL) == 0);
}

static void event_test(char *s) {
    // __event_test1();
    __event_test2();
    __event_test3();
}
//////////////////////////////////////////////////////////////////////////
const int prime3work_total = 100;
int *prime3work_dong;
void prime3work_primeproc(void) {
    int index = 0, this, num, recv_pid, pid = 0;
top:
    event_recv(&recv_pid, &this);
    printf("%d is a primer.\n", this);

    while (event_recv(NULL, &num) == 0) {
        if (num == -1) goto out;
        if ((num % this) == 0) { continue; }
        if (pid == 0) {
            if (index + 1 == prime3work_total) {
                *prime3work_dong = 1;
                goto out;
            }
            if ((pid = fork()) == 0) {
                index++;
                goto top;
            }
            if (pid < 0) { goto out; }
        }
        if (*prime3work_dong == 0) {
            if (event_send(pid, num) != 0) { goto out; }
        }
    }
out:
    printf("[%04d] %d quit.   recv_pid id %d\n", getpid(), index, recv_pid);
    // printf("pid id %d 111111\n", getpid());
    if (this != 2) event_send(recv_pid, -1);
    // printf("pid id %d 222222\n", getpid());
    wait();
    // printf("pid id %d 333333\n", getpid());
    exit(0);
}

static void prime3worktest(char *s) {
    int i, pid;
    prime3work_dong = shmem_malloc(sizeof(*prime3work_dong));
    assert(prime3work_dong != nullptr);
    *prime3work_dong = 0;
    unsigned int time = gettime();
    if ((pid = fork()) == 0) {
        prime3work_primeproc();
        exit(0);
    }
    assert(pid > 0);

    for (i = 2;; i++) {
        if (event_send(pid, i) != 0) { break; }
    }
    int ret_value;
    // event_recv(nullptr, &ret_value);
    // assert(ret_value == -1);
    printf("use %d ticks.\n", gettime() - time);
    wait();
}
//////////////////////////////////////////////////////////////////////////
// Mboxbuf __buf, *buf = &__buf;
static void mboxtest(char *s) {
    int mbox_id = mbox_init(1);
    assert(mbox_id >= 0);
    Mboxbuf __buf, *buf = &__buf;
    buf->size = 4096;
    buf->len = buf->size;
    buf->data = malloc(sizeof(char) * buf->size);
    assert(buf->data != nullptr);
    char *data = (char *)(buf->data);
    int i;
    for (i = 0; i < buf->size; i++) { data[i] = (char)i; }
    ulong timeout = 100, save_ticks = gettime();
    assert(mbox_send(mbox_id, buf) == 0);
    assert(mbox_send_timeout(mbox_id, buf, timeout) == -E_TIMEOUT);
    assert((ulong)(gettime() - save_ticks) >= timeout);
    size_t save_size = buf->size;
    buf->size = 100;
    assert(mbox_recv(mbox_id, buf) != 0);
    buf->size = save_size - 1;
    assert(mbox_recv(mbox_id, buf) != 0);
    buf->size = save_size;
    memset(buf->data, 0, sizeof(char) * buf->size);
    assert(mbox_recv(mbox_id, buf) == 0);
    assert(buf->size == save_size && buf->len == save_size);
    data = (char *)(buf->data);
    for (i = 0; i < buf->size; i++) { assert(data[i] == (char)i); }
    save_ticks = gettime();
    assert(mbox_recv_timeout(mbox_id, buf, timeout) == -E_TIMEOUT);
    assert((ulong)(gettime() - save_ticks) >= timeout);
    assert(mbox_free(mbox_id) == 0);
    assert(mbox_send(mbox_id, buf) != 0);
    sleep(500);  // for wait mbox_cleanup;
}
//////////////////////////////////////////////////////////////////////////
const int mboxmap_mod = 23;
const int mboxmap_max_data = 2048;
const int mboxmap_max_slots = 1024;

int mboxmap_send(int id, void *data, size_t len) {
    Mboxbuf buf;
    buf.data = data, buf.len = len;
    return mbox_send(id, &buf);
}

int mboxmap_recv(int id, void *data, size_t size, size_t *lenp) {
    Mboxbuf buf;
    buf.data = data, buf.size = size;
    int ret;
    if ((ret = mbox_recv(id, &buf)) == 0) { *lenp = buf.len; }
    return ret;
}

void mboxmap_filter_main(int *data, int mbox_data, int mbox[]) {
    int i, count[mboxmap_mod];
    size_t size = mboxmap_max_data * sizeof(int), len;
    while (mboxmap_recv(mbox_data, data, size, &len) == 0) {
        assert((len % sizeof(int)) == 0);
        memset(count, 0, sizeof(count));
        len /= sizeof(int);
        for (i = 0; i < len; i++) { count[data[i] % mboxmap_mod]++; }
        for (i = 0; i < mboxmap_mod; i++) { mboxmap_send(mbox[i], count + i, sizeof(int)); }
    }
}

void mboxmap_select_main(int mbox, int pid) {
    size_t len;
    int count = 0, ans;
    while (mboxmap_recv(mbox, &ans, sizeof(int), &len) == 0) {
        assert(len == sizeof(int));
        count += ans;
    }
    event_send(pid, count);
}

int mboxmap_wait_for_empty(int mbox) {
    Mboxinfo info;
    while (1) {
        if (mbox_info(mbox, &info) != 0) { return -1; }
        if (info.slots == 0) { return 0; }
        sleep(10);
    }
}

int mboxmap_wait_for_quit(int count[], int pids[]) {
    int i, j, pid, event, ans[mboxmap_mod];
    memset(ans, 0, sizeof(ans));
    for (i = 0; i < mboxmap_mod; i++) {
        if (event_recv(&pid, &event) != 0) { return -1; }
        for (j = 0; j < mboxmap_mod; j++) {
            if (pids[j] == pid) {
                ans[j] = event;
                printf("-- recv count %02d: %08d\n", j, event);
            }
        }
    }
    int err = 0;
    for (i = 0; i < mboxmap_mod; i++) {
        if (count[i] != ans[i]) {
            err++;
            printf("wrong: %d, %d, %d.\n", i, count[i], ans[i]);
        }
    }
    return err;
}

static void mboxmaptest(char *s) {
    int i, j, k, mbox[mboxmap_mod], count[mboxmap_mod];
    for (i = 0; i < mboxmap_mod; i++) {
        mbox[i] = mbox_init(mboxmap_max_slots);
        assert(mbox[i] >= 0);
        printf("mod_%d id is %d\n", i, mbox[i]);
    }
    int mbox_data = mbox_init(mboxmap_max_slots);
    assert(mbox_data >= 0);
    printf("mbox_data is %d\n", mbox_data);

    size_t len, size;
    len = size = mboxmap_max_data * sizeof(int);

    int *data = malloc(size);
    assert(data != nullptr);

    int s_pids[mboxmap_mod], this = getpid();
    memset(s_pids, 0, sizeof(s_pids));

    for (i = 0; i < mboxmap_mod; i++) {
        if ((s_pids[i] = fork()) == 0) {
            mboxmap_select_main(mbox[i], this);
            exit(0);
        }
        assert(s_pids[i] > 0);
    }
    int f_pids[100], f_pids_num = sizeof(f_pids) / sizeof(f_pids[0]);
    memset(f_pids, 0, sizeof(f_pids));

    for (i = 0; i < f_pids_num; i++) {
        if ((f_pids[i] = fork()) == 0) {
            // memset(data, 0, size);
            mboxmap_filter_main(data, mbox_data, mbox);
            exit(0);
        }
        assert(f_pids[i] > 0);
    }
    printf("fork children ok.\n");
    memset(count, 0, sizeof(count));
    srand(913);
    for (i = 0; i < 5; i++) {
        for (j = 0; j < 512; j++) {
            for (k = 0; k < mboxmap_max_data; k++) {
                data[k] = simulate_rand();
                count[data[k] % mboxmap_mod]++;
            }
            assert(mboxmap_send(mbox_data, data, len) == 0);
        }
        printf("round %d\n", i);
    }
    for (i = 0; i < mboxmap_mod; i++) { printf("-- send count %02d: %08d\n", i, count[i]); }
    sleep(500);
    if (mboxmap_wait_for_empty(mbox_data) == 0) {
        assert(mbox_free(mbox_data) == 0);
        int exit_code;
        for (i = 0; i < f_pids_num; i++) {
            waitpid(f_pids[i], &exit_code);
            assert(exit_code == 0);
        }
        for (i = 0; i < mboxmap_mod; i++) { assert(mboxmap_wait_for_empty(mbox[i]) == 0); }
        printf("wait children ok.\n");
        for (i = 0; i < mboxmap_mod; i++) { assert(mbox_free(mbox[i]) == 0); }
        assert(mboxmap_wait_for_quit(count, s_pids) == 0);
        for (i = 0; i < mboxmap_mod; i++) {
            waitpid(s_pids[i], &exit_code);
            assert(exit_code == 0);
        }
    }
    sleep(3000);  // for wait mbox_cleanup;
}
//////////////////////////////////////////////////////////////////////////
static bool sigtest_exit_flag = false;
void sigtest_signal_handler(void) {
    printf("Signal handler invoked by child process!\n");
    sigtest_exit_flag = true;
}

static void sigtest(char *s) {
    int pid = fork();
    if (pid == 0) {
        printf("Child process %d started\n", getpid());
        Sigaction sa;
        sa.sa_handler = sigtest_signal_handler;
        sa.sa_flags = 0;
        sa.sa_mask = 0;
        assert(set_sigaction(SIGUSR1, &sa) == 0);
        printf("Child process completed\n");
        while (sigtest_exit_flag == false);
        exit(0);
    } else {
        assert(pid > 0);
        sleep(100);
        assert(send_signal(pid, SIGUSR1) == 0);
        int status;
        waitpid(pid, &status);
        assert(status == 0);
    }
}
//////////////////////////////////////////////////////////////////////////
#define sched_CFS_NUM_PROCS 5
#define sched_CFS_LOOP 100000000

static void sched_CFS_test(char *s) {
    int i;
    int pids[sched_CFS_NUM_PROCS];
    int nice_values[sched_CFS_NUM_PROCS] = {-20, -10, 0, 10, 19};
    double user_times[sched_CFS_NUM_PROCS];

    for (i = 0; i < sched_CFS_NUM_PROCS; i++) {
        pids[i] = fork();
        if (pids[i] == 0) {
            assert(setpriority(getpid(), nice_values[i]) == 0);
            ulong j, sum = 0;
            set_proc_cpu(getpid(), 0);
            ulong start_ticks = gettime();
            for (j = 0; j < sched_CFS_LOOP; j++) { sum += j % 100; }
            ulong end_ticks = gettime();
            printf("Process %d with nice value %d used %lu ticks\n", getpid(), getpriority(getpid()),
                   end_ticks - start_ticks);
            exit(0);
        }
        assert(pids[i] > 0);
    }
    for (i = 0; i < sched_CFS_NUM_PROCS; i++) { waitpid(pids[i], nullptr); }
}
//////////////////////////////////////////////////////////////////////////
#define LOAD_BALANCE_NUM_TASKS 100

void cpu_intensive_work() {
    unsigned long sum = 0;
    for (unsigned long i = 0; i < 40000000; i++) { sum += i % 100; }
}

int task_fn_bind_cpu0(void *arg) {
    set_proc_cpu(getpid(), 0);
    cpu_intensive_work();
    exit(0);
}

int task_fn_clear_bind(void *arg) {
    clear_proc_setcpu(getpid());
    cpu_intensive_work();
    exit(0);
}

static void cpu_load_balance_test(char *s) {
    int pids[LOAD_BALANCE_NUM_TASKS];
    uint64_t start_ticks, end_ticks;
    printf("Starting phase 1: All tasks bound to CPU 0...\n");
    for (int i = 0; i < LOAD_BALANCE_NUM_TASKS; i++) {
        if ((pids[i] = fork()) == 0) { task_fn_bind_cpu0((void *)(uintptr_t)i); }
        assert(pids[i] > 0);
    }
    start_ticks = gettime();
    for (int i = 0; i < LOAD_BALANCE_NUM_TASKS; i++) {
        int exit_code;
        assert(waitpid(pids[i], &exit_code) == 0);
    }
    end_ticks = gettime();
    uint64_t total_time_bind_cpu0 = end_ticks - start_ticks;
    printf("Starting phase 2: Tasks using kernel load balancing...\n");
    for (int i = 0; i < LOAD_BALANCE_NUM_TASKS; i++) {
        if ((pids[i] = fork()) == 0) { task_fn_clear_bind((void *)(uintptr_t)i); }
        assert(pids[i] > 0);
    }
    start_ticks = gettime();
    for (int i = 0; i < LOAD_BALANCE_NUM_TASKS; i++) {
        int exit_code;
        assert(waitpid(pids[i], &exit_code) == 0);
    }
    end_ticks = gettime();
    uint64_t total_time_load_balanced = end_ticks - start_ticks;
    printf("Phase 2 Results: Tasks using kernel load balancing\n");
    printf("\nComparison of two phases:\n");
    printf("Total time (Phase 1, bound to CPU 0): %lu ticks.\n", total_time_bind_cpu0);
    printf("Total time (Phase 2, load balanced): %lu ticks.\n", total_time_load_balanced);

    if (total_time_load_balanced < total_time_bind_cpu0 >> 1) {
        printf("Load balancing improved performance by distributing tasks across CPUs.\n");
    } else {
        printf("No significant improvement in load balancing.\n");
    }
}
//////////////////////////////////////////////////////////////////////////
static void fprintf_test(char *s) {
    fprintf(1, "Hello world!!.\n");
    fprintf(1, "I am process %d.\n", getpid());
}
//////////////////////////////////////////////////////////////////////////
static void fread_test(char *s) {
    /* type 'q' to stop reading */
    char c;
    printf("now reading...\n");
    do {
        int ret = read(0, &c, sizeof(c));
        assert(ret == 1);
        printf("type [%03d] %c.\n", c, c);
    } while (c != 'q');
}
//////////////////////////////////////////////////////////////////////////
static void fread_test2(char *s) {
    int pid, ret;
    if ((pid = fork()) == 0) {
        do {
            char c;
            ret = read(0, &c, sizeof(c));
            assert(ret == 1);
        } while (1);
    }
    assert(pid > 0);
    sleep(100);
    kill(pid);
    assert(waitpid(pid, &ret) == 0 && ret != 0);
}
//////////////////////////////////////////////////////////////////////////
static void fwrite_test_testfd(const char *name, int fd) {
    struct stat __stat, *stat = &__stat;
    int ret = fstat(fd, stat);
    assert(ret == 0);
    print_stat(name, fd, stat);
}

static void fwrite_test(char *s) {
    int fd = dup(1);
    assert(fd >= 0);

    fwrite_test_testfd("stdin", 0);
    fwrite_test_testfd("stdout", 1);
    fwrite_test_testfd("dup: stdout", fd);

    int size = 1024, len = 0;
    char buf[size];
    len += snprintf(buf + len, size - len, "Hello world!!.\n");
    len += snprintf(buf + len, size - len, "I am process %d.\n", getpid());
    write(fd, buf, len);

    int ret;
    while ((ret = dup(fd)) >= 0) /* do nothing */
        ;

    close(fd);
    len = snprintf(buf, size, "FAIL: T.T\n");
    write(fd, buf, len);
    printf("dup fd ok.\n");

    int pid;
    if ((pid = fork()) == 0) {
        sleep(10);
        len = snprintf(buf, size, "fork fd ok.\n");
        ret = write(1, buf, len);
        assert(ret == len);
        exit(0);
    }
    assert(pid > 0);
    assert(waitpid(pid, &ret) == 0 && ret == 0);
}
//////////////////////////////////////////////////////////////////////////
static void fnull_test(char *s) {
    int fd = 0, ret;
    char buf[5];
    buf[0] = buf[1] = buf[2] = buf[3] = 'a';
    buf[4] = '\n';
    fd = open("null:", O_RDWR);
    printf("NULL fd is %d\n", fd);
    ret = write(fd, "hello", 5);
    printf("write %d to NULL\n", ret);
    ret = read(fd, buf, 5);
    printf("read %d from NULL, buf is %s\n", ret, buf);

    fprintf(1, "Hello world!!.\n");
    fprintf(1, "I am process %d.\n", getpid());
    fprintf(1, "hello2 pass.\n");
}
//////////////////////////////////////////////////////////////////////////
static char __pipe_test_buf[4096];

static void __pipe_test1(void) {
    int __fd[2];
    assert(mkpipe(NULL) != 0 && mkpipe(__fd) == 0);

    int i, pid, fd, len;
    if ((pid = fork()) == 0) {
        fd = __fd[1];
        for (i = 0; i < 10; i++) { yield(); }
        if (write(fd, "A", 1) == 1) { printf("pid : %d child write ok\n", getpid()); }
        exit(0);
    }
    assert(pid > 0);
    fd = __fd[0], close(__fd[1]);
    assert((len = read(fd, __pipe_test_buf, sizeof(__pipe_test_buf))) == 1 && __pipe_test_buf[0] == 'A');
    assert(wait() == 0 && close(fd) == 0);

    printf("parent read ok\n");
    printf("pipetest step1 pass.\n");
}

static void __pipe_test2(void) {
    int __fd[2], fd, ret;
    assert(mkpipe(NULL) != 0 && mkpipe(__fd) == 0);

    struct stat __stat, *stat = &__stat;

    fd = __fd[0], ret = fstat(fd, stat);
    assert(ret == 0);
    print_stat("pipe0", fd, stat);
    assert(S_ISCHR(stat->st_mode));

    fd = __fd[1], ret = fstat(fd, stat);
    assert(ret == 0);
    print_stat("pipe1", fd, stat);
    assert(S_ISCHR(stat->st_mode));

    close(__fd[0]), close(__fd[1]);
    printf("pipetest step2 pass.\n");
}

static void __pipe_test3(void) {
    int __fd[2];
    assert(mkpipe(NULL) != 0 && mkpipe(__fd) == 0);

    char *msg = "Hello world!!.";
    size_t len = strlen(msg);

    int pid, fd, ret;
    if ((pid = fork()) == 0) {
        fd = __fd[1];
        assert(read(fd, __pipe_test_buf, sizeof(__pipe_test_buf)) < 0);
        ret = write(fd, msg, len);
        assert(ret == len);
        exit(0);
    }
    assert(pid > 0);

    fd = __fd[0], close(__fd[1]);
    assert(write(fd, msg, len) < 0);

    int total = 0;
    while ((ret = read(fd, __pipe_test_buf + total, sizeof(__pipe_test_buf) - total)) > 0) { total += ret; }
    __pipe_test_buf[total] = '\0';
    assert(total == len && strcmp(__pipe_test_buf, msg) == 0);
    assert(wait() == 0 && close(fd) == 0);
    printf("pipetest step3 pass.\n");
}

static void __pipe_test4(void) {
    char *name = "test";
    int fd, __fd;
    fd = __fd = mkfifo(name, O_CREAT | O_RDONLY);
    assert(fd >= 0);

    fd = mkfifo(name, O_CREAT | O_RDONLY);
    assert(fd >= 0 && close(fd) == 0);

    assert(mkfifo(name, O_CREAT | O_EXCL | O_RDONLY) < 0);
    assert(mkfifo(name, O_WRONLY) < 0);

    fd = mkfifo(name, O_CREAT | O_WRONLY | O_EXCL);
    assert(fd >= 0 && close(fd) == 0);
    assert(read(__fd, __pipe_test_buf, sizeof(__pipe_test_buf)) == 0 && close(__fd) == 0);

    fd = mkfifo(name, O_CREAT | O_RDONLY | O_EXCL);
    assert(fd >= 0);

    int pid, ret;
    if ((pid = fork()) == 0) {
        fd = mkfifo(name, O_CREAT | O_WRONLY | O_EXCL);
        assert(fd >= 0);
        memset(__pipe_test_buf, 'A', sizeof(__pipe_test_buf));
        int ret = write(fd, __pipe_test_buf, sizeof(__pipe_test_buf));
        assert(ret == sizeof(__pipe_test_buf));
        exit(0);
    }
    assert(pid > 0);

    size_t total = 0;
    while ((ret = read(fd, __pipe_test_buf + total, sizeof(__pipe_test_buf) - total)) > 0) { total += ret; }
    assert(total == sizeof(__pipe_test_buf));

    int i;
    for (i = 0; i < total; i++) { assert(__pipe_test_buf[i] == 'A'); }
    assert(wait() == 0);
    printf("pipetest step4 pass.\n");
}

static void pipe_test(char *s) {
    __pipe_test1();
    __pipe_test2();
    __pipe_test3();
    __pipe_test4();
}
//////////////////////////////////////////////////////////////////////////
int __pipe_test2_fd[2], pipe_test2_fd;

Thread pipe_test2_tids[10];
int pipe_test2_total = sizeof(pipe_test2_tids) / sizeof(pipe_test2_tids[0]);

int pipe_test2_thread_main(void *arg) {
    int id = (long)arg;
    printf("this is %d\n", id);

    size_t n = 1000;
    char *buf = malloc(sizeof(char) * n);
    if (buf == NULL) { return -1; }

    memset(buf, (char)id, n);

    int i, rounds = 20, ret;
    for (i = 0; i < rounds; i++) {
        if ((ret = write(pipe_test2_fd, buf, n)) < 0 || ret != n) {
            printf("pipe is closed, too early.\n");
            return -1;
        }
        if (id == 0) { printf("send %d/%d\n", i, rounds); }
    }
    return 0;
}

void pipe_test2_process_main(void) {
    int counts[pipe_test2_total], i, ret;
    for (i = 0; i < pipe_test2_total; i++) { counts[i] = 0; }

    char buf[128];
    size_t n = sizeof(buf);

    while (1) {
        if ((ret = read(pipe_test2_fd, buf, n)) <= 0) { break; }
        for (i = 0; i < ret; i++) { counts[((unsigned int)buf[i]) % pipe_test2_total]++; }
    }
    for (i = 0; i < pipe_test2_total; i++) { printf("%d reads %d\n", i, counts[i]); }
    exit(0);
}

static void pipe_test2(char *s) {
    int pid, i;
    assert(mkpipe(__pipe_test2_fd) == 0);

    if ((pid = fork()) == 0) {
        pipe_test2_fd = __pipe_test2_fd[0], close(__pipe_test2_fd[1]);
        pipe_test2_process_main();
    }
    assert(pid > 0);

    pipe_test2_fd = __pipe_test2_fd[1], close(__pipe_test2_fd[0]);
    memset(pipe_test2_tids, 0, sizeof(Thread) * pipe_test2_total);
    for (i = 0; i < pipe_test2_total; i++) {
        assert(thread(pipe_test2_thread_main, (void *)(long)i, pipe_test2_tids + i) == 0);
    }

    int exit_code;
    for (i = 0; i < pipe_test2_total; i++) {
        assert(thread_wait(pipe_test2_tids + i, &exit_code) == 0 && exit_code == 0);
    }
    for (i = 0; i < pipe_test2_total; i++) { yield(); }
    close(__pipe_test2_fd[0]), close(__pipe_test2_fd[1]);
    assert(waitpid(pid, &exit_code) == 0 && exit_code == 0);
}
//////////////////////////////////////////////////////////////////////////
static int safe_open(char *path, int open_flags) {
    int fd = open(path, open_flags);
    if (fd < 0) { int pp = 3; }
    assert(fd >= 0);
    return fd;
}

static Stat *safe_fstat(int fd) {
    static struct stat __stat, *stat = &__stat;
    int ret = fstat(fd, stat);
    assert(ret == 0);
    return stat;
}

static int safe_dup(int fd1) {
    int fd2 = dup(fd1);
    assert(fd2 >= 0);
    return fd2;
}

static void safe_read(int fd, void *data, size_t len) {
    long ret = read(fd, data, len);
    assert(ret == len);
}

static void safe_write(int fd, void *data, size_t len) {
    long ret = write(fd, data, len);
    assert(ret == len);
}

static void safe_seek(int fd, off_t pos, int whence) {
    int ret = seek(fd, pos, whence);
    assert(ret == 0);
}

static void __fs_read_test() {
    char buf[SFS_BSIZE];
    int fd = safe_open("README", O_RDONLY);
    printf("fs_read fd is %d\n", fd);
    int ret = read(fd, buf, SFS_BSIZE);
    assert(ret > 0);
    printf("read %d from %d, buf is :%s\n", ret, fd, buf);
    assert(open("README", O_CREAT | O_EXCL | O_RDONLY) == -E_EXISTS);
    close(fd);
    printf("__fs_read_test ok.\n");
}

static void __fs_write_test() {
    char buf_write[SFS_BSIZE] = "this a simple write test!!!";
    char buf_read[SFS_BSIZE];
    int fd = safe_open("__fs_write_test", O_CREAT | O_RDWR | O_EXCL);
    printf("fs_write_test fd is %d\n", fd);
    safe_write(fd, buf_write, SFS_BSIZE);
    printf("write to %d, buf is :%s\n", fd, buf_write);
    safe_seek(fd, 0, LSEEK_SET);
    safe_read(fd, buf_read, SFS_BSIZE);
    printf("read from %d, buf is :%s\n", fd, buf_read);
    assert(strcmp(buf_read, buf_write) == 0);
    close(fd);
    printf("__fs_write_test ok.\n");
}

static void __fs_trunc_test() {
    char buf_write[SFS_BSIZE] = "this a simple trunc test!!!";
    char buf_read[SFS_BSIZE];
    int fd = safe_open("__fs_trunc_test", O_CREAT | O_RDWR | O_EXCL);
    printf("fs_trunc_test fd is %d\n", fd);
    safe_write(fd, buf_write, SFS_BSIZE);
    printf("write to %d, buf is :%s\n", fd, buf_write);
    Stat *stat = safe_fstat(fd);
    assert(stat->st_size == SFS_BSIZE && S_ISREG(stat->st_mode) && stat->st_blocks == 1);
    safe_seek(fd, 0, LSEEK_SET);
    safe_read(fd, buf_read, SFS_BSIZE);
    printf("read from %d, buf is :%s\n", fd, buf_read);
    assert(strcmp(buf_read, buf_write) == 0);
    fd = safe_open("__fs_trunc_test", O_RDWR | O_TRUNC);
    printf("__fs_trunc_test fd is %d\n", fd);
    stat = safe_fstat(fd);
    assert(stat->st_size == 0 && S_ISREG(stat->st_mode) && stat->st_blocks == 0);
    safe_seek(fd, 0, LSEEK_SET);
    long ret = read(fd, buf_read, SFS_BSIZE);
    assert(ret < 0);
    printf("read from %d, buf is :%s\n", fd, buf_read);
    close(fd);
    printf("__fs_trunc_test ok.\n");
}

static void __fs_append_test() {
    char buf_write[SFS_BSIZE] = "this a simple append test!!!";
    char buf_read[SFS_BSIZE];
    int fd = safe_open("__fs_append_test", O_CREAT | O_RDWR | O_EXCL);
    printf("__fs_append_test fd is %d\n", fd);
    char *str1 = "ABCD";
    char *str2 = "EFGH";
    char *str3 = "IJKL";
    char *str4 = "MNOP";
    safe_write(fd, str1, strlen(str1));
    safe_write(fd, str2, strlen(str2));
    safe_seek(fd, 0, LSEEK_SET);
    read(fd, buf_read, SFS_BSIZE);
    printf("read from %d, buf is :%s\n", fd, buf_read);
    safe_seek(fd, 0, LSEEK_SET);
    safe_write(fd, str3, strlen(str3));
    memset(buf_read, 0, SFS_BSIZE);
    safe_seek(fd, 0, LSEEK_SET);
    read(fd, buf_read, SFS_BSIZE);
    printf("read from %d, buf is :%s\n", fd, buf_read);
    safe_seek(fd, 0, LSEEK_SET);
    fd = safe_open("__fs_append_test", O_RDWR | O_APPEND);
    safe_write(fd, str4, strlen(str4));
    memset(buf_read, 0, SFS_BSIZE);
    safe_seek(fd, 0, LSEEK_SET);
    read(fd, buf_read, SFS_BSIZE);
    printf("read from %d, buf is :%s\n", fd, buf_read);
    close(fd);
    printf("__fs_append_test ok.\n");
}

static void __fs_seek_test() {
    char buf_read[SFS_BSIZE];
    const int buf_nr = 1000;
    int fd = safe_open("__fs_seek_test", O_CREAT | O_RDWR | O_TRUNC);
    Stat *stat = safe_fstat(fd);
    assert(stat->st_size == 0 && S_ISREG(stat->st_mode) && stat->st_blocks == 0);
    safe_seek(fd, sizeof(buf_read) * buf_nr, LSEEK_SET);
    stat = safe_fstat(fd);
    assert(stat->st_size == sizeof(buf_read) * buf_nr && S_ISREG(stat->st_mode) && stat->st_blocks == buf_nr);
    for (int i = 0; i < sizeof(buf_read); i++) { buf_read[i] = i; }
    safe_write(fd, buf_read, sizeof(buf_read));
    stat = safe_fstat(fd);
    assert(stat->st_size == sizeof(buf_read) * (buf_nr + 1) && S_ISREG(stat->st_mode) &&
           stat->st_blocks == buf_nr + 1);
    memset(buf_read, 0, sizeof(buf_read));
    safe_seek(fd, -sizeof(buf_read), LSEEK_END);
    safe_read(fd, buf_read, sizeof(buf_read));
    for (int i = 0; i < sizeof(buf_read); i++) { assert(buf_read[i] == (unsigned char)i); }
    fd = safe_open("__fs_seek_test", O_RDWR | O_TRUNC);
    printf("__fs_seek_test fd is %d\n", fd);
    stat = safe_fstat(fd);
    assert(stat->st_size == 0 && S_ISREG(stat->st_mode) && stat->st_blocks == 0);
    printf("__fs_seek_test ok.\n");
}

static void fs_basic_test(char *s) {
    __fs_read_test();
    __fs_write_test();
    __fs_trunc_test();
    __fs_append_test();
    __fs_seek_test();
}
//////////////////////////////////////////////////////////////////////////
static uint32_t fs_file_buffer[1024];

void init_data(int fd, int pages) {
    uint32_t value = 0;
    for (int i = 0; i < pages; i++) {
        for (int j = 0; j < sizeof(fs_file_buffer) / sizeof(fs_file_buffer[0]); j++) {
            fs_file_buffer[j] = value++;
        }
        safe_write(fd, fs_file_buffer, sizeof(fs_file_buffer));
    }
}

void random_test(int fd, int pages) {
    const int size = sizeof(fs_file_buffer) / sizeof(fs_file_buffer[0]);
    safe_seek(fd, 0, LSEEK_SET);
    srand(527);
    for (int i = 0; i < pages; i++) {
        safe_read(fd, fs_file_buffer, sizeof(fs_file_buffer));
        for (int j = 0; j < 32; j++) {
            uint32_t value = simulate_rand() % size;
            assert(fs_file_buffer[value] == i * size + value);
        }
    }
}

static void fs_file_test(char *s) {
    int fd1 = safe_open("fs_file_test", O_CREAT | O_RDWR | O_EXCL);
    Stat *stat = safe_fstat(fd1);
    assert(stat->st_size == 0 && S_ISREG(stat->st_mode) && stat->st_blocks == 0);
    const int npages = 128;
    init_data(fd1, npages);
    printf("init_data ok.\n");
    int fd2 = safe_dup(fd1);
    stat = safe_fstat(fd2);
    assert(stat->st_size == npages * sizeof(fs_file_buffer) && S_ISREG(stat->st_mode));
    random_test(fd2, npages);
    printf("random_test ok.\n");
    int fd3 = safe_open("fs_file_test", O_RDWR | O_TRUNC);
    stat = safe_fstat(fd3);
    assert(stat->st_size == 0 && S_ISREG(stat->st_mode) && stat->st_blocks == 0);
    safe_seek(fd3, sizeof(fs_file_buffer), LSEEK_END);
    safe_seek(fd3, 0, LSEEK_SET);
    stat = safe_fstat(fd3);
    assert(stat->st_size == sizeof(fs_file_buffer));
    safe_seek(fd3, sizeof(fs_file_buffer), LSEEK_END);
    stat = safe_fstat(fd3);
    assert(stat->st_size == sizeof(fs_file_buffer) * 2);
    safe_seek(fd3, -sizeof(fs_file_buffer), LSEEK_CUR);
    safe_read(fd3, fs_file_buffer, sizeof(fs_file_buffer));
    for (int i = 0; i < sizeof(fs_file_buffer) / sizeof(fs_file_buffer[0]); i++) {
        assert(fs_file_buffer[i] == 0);
    }
    int fd4 = safe_open("fs_file_test", O_RDWR | O_TRUNC);
    printf("fs_trunc_test fd is %d\n", fd4);
    stat = safe_fstat(fd4);
    assert(stat->st_size == 0 && S_ISREG(stat->st_mode) && stat->st_blocks == 0);
}
//////////////////////////////////////////////////////////////////////////
static char getmode(uint32_t st_mode) {
    char mode = '?';
    if (S_ISREG(st_mode)) mode = '-';
    if (S_ISDIR(st_mode)) mode = 'd';
    if (S_ISLNK(st_mode)) mode = 'l';
    if (S_ISCHR(st_mode)) mode = 'c';
    if (S_ISBLK(st_mode)) mode = 'b';
    return mode;
}

static void safe_stat_print(char *name) {
    Stat __stat, *stat = &__stat;
    int fd = open(name, O_RDONLY), ret = fstat(fd, stat);
    assert(fd >= 0 && ret == 0);
    printf("%c %3d   %4d %10d  ", getmode(stat->st_mode), stat->st_nlinks, stat->st_blocks, stat->st_size);
}

static void safe_chdir(char *path) {
    int ret = chdir(path);
    assert(ret == 0);
}

static void safe_getcwd(int round) {
    static char fs_dir_buffer[SFS_PWD_LEN];
    int ret = getcwd(fs_dir_buffer, sizeof(fs_dir_buffer));
    assert(ret == 0);
    printf("%d: current: %s\n", round, fs_dir_buffer);
}

static void safe_lsdir(int round) {
    DIR *dir = opendir(".");
    assert(dir != nullptr);
    Dirent *dirent;
    while ((dirent = readdir(dir)) != nullptr) {
        printf("%d: ", round);
        safe_stat_print(dirent->name);
        printf("%s\n", dirent->name);
    }
    closedir(dir);
}

static void changedir(char *path) {
    static int fs_dir_test_round = 0;
    printf("------------------------\n");
    safe_chdir(path);
    safe_getcwd(fs_dir_test_round);
    safe_lsdir(fs_dir_test_round);
    fs_dir_test_round++;
}

static void fs_dir_test(char *s) {
    changedir("/");
    mkdir("/AA");
    changedir("/");
    changedir("/AA");
    mkdir("/AA/BB");
    changedir("/AA/BB");
    changedir("/AA/BB/./../../AA");
    mkdir("/AA/BB/./../../AA/BB/CC");
    changedir("/AA/BB/CC");
    changedir("../..");
}
//////////////////////////////////////////////////////////////////////////
static void safe_link(char *oldpath, char *newpath) {
    int ret = link(oldpath, newpath);
    assert(ret == 0);
}

static void safe_unlink(char *path) {
    int ret = unlink(path);
    assert(ret == 0);
}

static uint32_t fs_link_buffer[1024];

static void fs_link_test(char *s) {
    char *str = "hello";
    int fd = safe_open("lf1", O_CREAT | O_RDWR);
    safe_write(fd, str, strlen(str));
    // safe_seek(fd, 0, LSEEK_SET);
    close(fd);
    changedir(".");
    safe_link("lf1", "lf2");
    changedir(".");
    safe_unlink("lf1");
    changedir(".");
    assert(open("lf1", 0) < 0);
    fd = safe_open("lf2", 0);
    safe_read(fd, fs_link_buffer, strlen(str));
    char *fs_link = (char *)fs_link_buffer;
    assert(strcmp(str, fs_link) == 0);
    close(fd);
    assert(link("lf2", "lf2") < 0);
    changedir(".");
    safe_unlink("lf2");
    assert(unlink("lf2") < 0);
    changedir(".");
}
//////////////////////////////////////////////////////////////////////////
// four processes create and delete different files in same directory
static void fs_test1(char *s) {
    enum { N = 10, NCHILD = 10 };
    int pid, i, fd, pi;
    char name[32];
    for (pi = 0; pi < NCHILD; pi++) {
        if ((pid = fork()) == 0) {
            name[0] = 'p' + pi;
            name[2] = '\0';
            for (i = 0; i < N; i++) {
                name[1] = '0' + i;
                fd = safe_open(name, O_CREAT | O_RDWR);
                close(fd);
                if (i > 0 && (i % 2) == 0) {
                    name[1] = '0' + (i / 2);
                    safe_unlink(name);
                }
            }
            exit(0);
        }
        assert(pid > 0);
    }
    int xstatus;
    for (pi = 0; pi < NCHILD; pi++) { assert(waitpid(0, &xstatus) == 0 && xstatus == 0); }
    name[0] = name[1] = name[2] = 0;
    for (i = 0; i < N; i++) {
        for (pi = 0; pi < NCHILD; pi++) {
            name[0] = 'p' + pi;
            name[1] = '0' + i;
            fd = open(name, 0);
            if ((i == 0 || i >= N / 2) && fd < 0) {
                panic("i is: %d  fd is: %d", i, fd);
            } else if ((i >= 1 && i < N / 2) && fd >= 0) {
                panic("i is: %d  fd is: %d", i, fd);
            }
        }
        if (fd > 0) { close(fd); }
    }
    for (i = 0; i < N; i++) {
        for (pi = 0; pi < NCHILD; pi++) {
            name[0] = 'p' + i;
            name[1] = '0' + i;
            unlink(name);
        }
    }
}
//////////////////////////////////////////////////////////////////////////
// another concurrent link/unlink/create test,
// to look for deadlocks.
static void fs_test2(char *s) {
    int pid, i;
    uint32_t x;
    unlink("x");
    pid = fork();
    assert(pid >= 0);
    pid ? srand(1) : srand(527);
    for (i = 0; i < 100; i++) {
        x = simulate_rand();
        if ((x % 3) == 0) {
            close(open("x", O_RDWR | O_CREAT));
        } else if ((x % 3) == 1) {
            link("cat", "x");
        } else {
            unlink("x");
        }
    }
    if (pid)
        wait();
    else
        exit(0);
}
//////////////////////////////////////////////////////////////////////////
// can I unlink a file and still read it?
char fs_test3_buf[SFS_BSIZE];
static void fs_test3(char *s) {
    enum { SZ = 5 };
    int fd, fd1;
    fd = safe_open("unlinkread", O_CREAT | O_RDWR);
    safe_write(fd, "hello", SZ);
    close(fd);
    fd = safe_open("unlinkread", O_RDWR);
    safe_unlink("unlinkread");  // At this point, although the file has been deleted from the file system, it
                                // has not been completely removed because the file descriptor (fd) is still
                                // open (open_count > 0 && ref_count > 0).
    fd1 = safe_open("unlinkread", O_CREAT | O_RDWR);
    safe_write(fd1, "yyy", 3);
    close(fd1);
    safe_read(fd, fs_test3_buf, SZ);
    assert(fs_test3_buf[0] == 'h');
    safe_write(fd, fs_test3_buf, 10);
    close(fd);
    unlink("unlinkread");
}
//////////////////////////////////////////////////////////////////////////
static void fs_test4(char *s) {
    enum { N = 10 };
    char file[3];
    int i, pid, fd;
    char fa[N];
    file[0] = 'C';
    file[2] = '\0';
    for (i = 0; i < N; i++) {
        file[1] = '0' + i;
        unlink(file);
        pid = fork();
        assert(pid >= 0);
        if (pid && (i % 3) == 1) {
            link("C0", file);
        } else if (pid == 0 && (i % 5) == 1) {
            link("C0", file);
        } else {
            fd = safe_open(file, O_CREAT | O_RDWR);
            close(fd);
        }
        if (pid == 0) {
            exit(0);
        } else {
            int xstatus;
            assert(waitpid(0, &xstatus) == 0 && xstatus == 0);
        }
    }

    memset(fa, 0, sizeof(fa));
    int n = 0;
    DIR *dir = opendir(".");
    assert(dir != nullptr);
    Dirent *dirent;
    while ((dirent = readdir(dir)) != nullptr) {
        if (dirent->name[0] == 'C' && dirent->name[2] == '\0') {
            i = dirent->name[1] - '0';
            assert(i >= 0 && i < sizeof(fa));
            assert(!fa[i]);
            fa[i] = 1;
            n++;
        }
    }
    closedir(dir);
    assert(n == N);

    for (i = 0; i < N; i++) {
        file[1] = '0' + i;
        pid = fork();
        assert(pid >= 0);

        if (((i % 3) == 0 && pid == 0) || ((i % 3) == 1 && pid != 0)) {
            for (int j = 0; j < 6; j++) { close(open(file, 0)); }
        } else {
            for (int j = 0; j < 6; j++) { unlink(file); }
        }
        if (pid == 0) {
            exit(0);
        } else {
            assert(waitpid(0, NULL) == 0);
        }
    }
}
//////////////////////////////////////////////////////////////////////////
// four processes write different files at the same
// time, to test block allocation.
static void fs_test5(char *s) {
    int fd, pid, i, j, n, total, pi;
    char *names[] = {"f0", "f1", "f2", "f3"};
    char *fname;
    enum { N = 12, NCHILD = 4, SZ = 500 };
    char buf[SZ];

    for (pi = 0; pi < NCHILD; pi++) {
        fname = names[pi];
        unlink(fname);
        pid = fork();
        assert(pid >= 0);
        if (pid == 0) {
            fd = safe_open(fname, O_CREAT | O_RDWR);
            memset(buf, '0' + pi, SZ);
            for (i = 0; i < N; i++) { safe_write(fd, buf, SZ); }
            close(fd);
            exit(0);
        }
    }

    int xstatus;
    for (pi = 0; pi < NCHILD; pi++) { assert(waitpid(0, &xstatus) == 0 && xstatus == 0); }

    for (i = 0; i < NCHILD; i++) {
        fname = names[i];
        fd = safe_open(fname, O_RDONLY);
        total = 0;
        while ((n = read(fd, buf, sizeof(buf))) > 0) {
            for (j = 0; j < n; j++) { assert(buf[j] == '0' + i); }
            total += n;
        }
        close(fd);
        assert(total == N * SZ);
        safe_unlink(fname);
    }
}
//////////////////////////////////////////////////////////////////////////
// two processes write to the same file descriptor
// is the offset shared? does inode locking work?
static void fs_test6(char *s) {
    int fd, pid, i, n, nc, np;
    enum { N = 1000, SZ = 10 };
    char buf[SZ];

    unlink("sharedfd");
    fd = safe_open("sharedfd", O_CREAT | O_RDWR);
    pid = fork();
    assert(pid >= 0);
    memset(buf, pid == 0 ? 'c' : 'p', sizeof(buf));
    for (i = 0; i < N; i++) {
        safe_write(fd, buf, sizeof(buf));  // file->pos is not shared, so subsequent write operations will
                                           // overwrite the previous content. Therefore, the total number of
                                           // 'C' and 'p' characters should add up to N * SZ.
    }
    if (pid == 0) {
        close(fd);
        exit(0);
    } else {
        int xstatus;
        assert(waitpid(0, &xstatus) == 0 && xstatus == 0);
    }

    close(fd);
    fd = safe_open("sharedfd", O_RDONLY);
    nc = np = 0;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        for (i = 0; i < n; i++) {
            if (buf[i] == 'c') nc++;
            if (buf[i] == 'p') np++;
        }
    }
    close(fd);
    safe_unlink("sharedfd");
    assert(nc + np == N * SZ);
}

//////////////////////////////////////////////////////////////////////////
// concurrent writes to try to provoke deadlock in the virtio disk
// driver.
char fs_test7_buf[SFS_BSIZE];
static void fs_test7(char *s) {
    int nchildren = 4;
    int howmany = 30;  // increase to look for deadlock

    for (int ci = 0; ci < nchildren; ci++) {
        int pid = fork();
        assert(pid >= 0);
        if (pid == 0) {
            char name[3];
            name[0] = 'b';
            name[1] = 'a' + ci;
            name[2] = '\0';
            unlink(name);
            for (int iters = 0; iters < howmany; iters++) {
                for (int i = 0; i < ci + 1; i++) {
                    int fd = safe_open(name, O_CREAT | O_RDWR);
                    int sz = sizeof(fs_test7_buf);
                    safe_write(fd, fs_test7_buf, sz);
                    close(fd);
                }
                unlink(name);
            }
            unlink(name);
            exit(0);
        }
    }
    int xstatus;
    for (int ci = 0; ci < nchildren; ci++) { assert(waitpid(0, &xstatus) == 0 && xstatus == 0); }
}
//////////////////////////////////////////////////////////////////////////
// test write a big file
char fs_test8_buf[SFS_BSIZE];
static void fs_test8(char *s) {
    int i, fd, n;
    unlink("big");
    fd = safe_open("big", O_CREAT | O_RDWR);
    for (i = 0; i < SFS_NDIRECT + SFS_NINDIRECT + 100; i++) {
        ((int *)fs_test8_buf)[0] = i;
        safe_write(fd, fs_test8_buf, SFS_BSIZE);
    }
    close(fd);
    fd = safe_open("big", O_RDONLY);
    n = 0;
    for (;;) {
        i = read(fd, fs_test8_buf, SFS_BSIZE);
        if (i <= 0) {
            assert(n == SFS_NDIRECT + SFS_NINDIRECT + 100);
            break;
        } else {
            assert(i == SFS_BSIZE);
        }
        assert(((int *)fs_test8_buf)[0] == n);
        n++;
    }
    close(fd);
    safe_unlink("big");
}
//////////////////////////////////////////////////////////////////////////
// test open a dir
static void fs_test9(char *s) {
    int pid, xstatus;
    unlink("oldir");
    assert(mkdir("oldir") == 0);
    pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        assert(open("oldir", O_RDWR) == -E_ISDIR);
        exit(0);
    }
    sleep(200);
    safe_unlink("oldir");
    assert(waitpid(0, &xstatus) == 0 && xstatus == 0);
}
//////////////////////////////////////////////////////////////////////////
// test unlink pwd and chdir
static void fs_test10(char *s) {
    unlink("inputdir");
    assert(mkdir("inputdir") == 0);
    changedir("inputdir");
    safe_unlink("../inputdir");
    changedir("/");
}
//////////////////////////////////////////////////////////////////////////
// test remove "." and ".."
static void fs_test11(char *s) {
    unlink("dots");
    assert(mkdir("dots") == 0);
    changedir("dots");
    assert(unlink(".") < 0);
    assert(unlink("..") < 0);
    changedir("/");
    assert(unlink("dots/.") < 0);
    assert(unlink("dots/..") < 0);
    safe_unlink("dots");
}
//////////////////////////////////////////////////////////////////////////
void safe_symlink(char *oldpath, char *newpath) {
    int ret = symlink(oldpath, newpath);
    assert(ret == 0);
}

static int stat_slink(char *pn, struct stat *st) {
    int fd = open(pn, O_RDONLY | O_NOFOLLOW);
    if (fd < 0) return -1;
    if (fstat(fd, st) != 0) return -1;
    return 0;
}

static void fs_symlink_test(char *s) {
    int fd1 = -1, fd2 = -1;
    char buf[4] = {'a', 'b', 'c', 'd'};
    char c = 0, c2 = 0;
    Stat st;
    assert(mkdir("/testsymlink") == 0);
    fd1 = safe_open("/testsymlink/a", O_CREAT | O_RDWR);
    safe_symlink("/testsymlink/a", "/testsymlink/b");
    safe_write(fd1, buf, sizeof(buf));
    if (stat_slink("/testsymlink/b", &st) != 0) panic("failed to stat b");
    if (!(S_ISLNK(st.st_mode))) panic("b isn't a symlink");
    fd2 = safe_open("/testsymlink/b", O_RDWR);
    safe_read(fd2, &c, 1);
    assert(c == 'a');
    safe_unlink("/testsymlink/a");
    assert(open("/testsymlink/b", O_RDWR) < 0);
    safe_symlink("/testsymlink/b", "/testsymlink/a");
    assert(open("/testsymlink/b", O_RDWR) < 0);
    safe_symlink("/testsymlink/nonexistent", "/testsymlink/c");

    safe_symlink("/testsymlink/2", "/testsymlink/1");
    safe_symlink("/testsymlink/3", "/testsymlink/2");
    safe_symlink("/testsymlink/4", "/testsymlink/3");
    close(fd1);
    close(fd2);

    fd1 = safe_open("/testsymlink/4", O_CREAT | O_RDWR);
    fd2 = safe_open("/testsymlink/1", O_RDWR);
    c = '#';
    safe_write(fd2, &c, 1);
    safe_read(fd1, &c2, 1);
    assert(c == c2);
}
//////////////////////////////////////////////////////////////////////////
// concurrent symlink/unlink/ test,
// to look for deadlocks.
static void fs_test12(char *s) {
    int pid, i;
    int fd;
    Stat st;
    uint32_t x;
    int nchild = 2;
    assert(mkdir("/fs_test12") == 0);
    fd = safe_open("/fs_test12/z", O_CREAT | O_RDWR);
    close(fd);
    for (int j = 0; j < nchild; j++) {
        pid = fork();
        assert(pid >= 0);
        if (pid == 0) {
            srand(1);
            for (i = 0; i < 100; i++) {
                x = simulate_rand();
                if ((x % 3) == 0) {
                    symlink("/fs_test12/z", "/fs_test12/y");
                    if (stat_slink("/fs_test12/y", &st) == 0) { assert(S_ISLNK(st.st_mode)); }
                } else {
                    unlink("/fs_test12/y");
                }
                // unlink("/fs_test12/y");
            }
            exit(0);
        }
    }
    int xstatus;
    for (int ci = 0; ci < nchild; ci++) { assert(waitpid(0, &xstatus) == 0 && xstatus == 0); }
}
/////////////////////////////////////////////////////////////////////////
// test writes that are larger than the log.
char fs_test13_buf[(SFS_MAXOPBLOCKS + 2) * SFS_BSIZE];

static void fs_test13(char *s) {
    int fd, sz = (SFS_MAXOPBLOCKS + 2) * SFS_BSIZE;
    memset(fs_test13_buf, 5, sz);
    unlink("bigwrite");
    for (sz = (SFS_MAXOPBLOCKS - 10) * SFS_BSIZE - 471; sz < (SFS_MAXOPBLOCKS + 2) * SFS_BSIZE; sz += SFS_BSIZE + 499) {
        fd = open("bigwrite", O_CREAT | O_RDWR);
        if (fd < 0) {
            printf("%s: cannot create bigwrite\n", s);
            exit(1);
        }
        int i;
        static int kk = 1;
        for (i = 0; i < 2; i++) {
            long cc = write(fd, fs_test13_buf, sz);
            assert(cc == sz);
        }
        close(fd);
        unlink("bigwrite");
    }
}
/////////////////////////////////////////////////////////////////////////
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
    {mmaptest, "mmaptest"},
    {shmemtest, "shmemtest"},
    {threadtest, "threadtest"},
    {threadforktest, "threadforktest"},
    {threadworktest, "threadworktest"},
    {primeworktest, "primeworktest"},
    {sem_test, "sem_test"},
    {sem_rw_test, "sem_rw_test"},
    {prime2worktest, "prime2worktest"},
    {spipetest, "spipetest"},
    {sem2_test, "sem2_test"},
    {sem3_test, "sem3_test"},
    {event_test, "event_test"},
    {prime3worktest, "prime3worktest"},
    {mboxtest, "mboxtest"},
    {mboxmaptest, "mboxmaptest"},
    {sigtest, "sigtest"},
    {sched_CFS_test, "sched_CFS_test"},
    {cpu_load_balance_test, "cpu_load_balance_test"},
    {fprintf_test, "fprintf_test"},
    // {fread_test, "fread_test"},
    {fread_test2, "fread_test2"},
    {fwrite_test, "fwrite_test"},
    {fnull_test, "fnull_test"},
    {pipe_test, "pipe_test"},
    {pipe_test2, "pipe_test2"},
    {fs_basic_test, "fs_basic_test"},
    {fs_file_test, "fs_file_test"},
    {fs_dir_test, "fs_dir_test"},
    {fs_link_test, "fs_link_test"},
    {fs_test1, "fs_test1"},
    {fs_test2, "fs_test2"},
    {fs_test3, "fs_test3"},
    {fs_test4, "fs_test4"},
    {fs_test5, "fs_test5"},
    {fs_test6, "fs_test6"},
    {fs_test7, "fs_test7"},
    {fs_test8, "fs_test8"},
    {fs_test9, "fs_test9"},
    {fs_test10, "fs_test10"},
    {fs_test11, "fs_test11"},
    {fs_symlink_test, "fs_symlink_test"},
    {fs_test12, "fs_test12"},
    {fs_test13, "fs_test13"},
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
