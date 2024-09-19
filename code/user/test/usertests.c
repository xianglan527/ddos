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
#include "rand.h"
#include "spipe.h"
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
static void sem_test(char *s){
    sem_t sem_id = sem_init(1);
    assert(sem_id > 0);
    printf("sem_id = 0x%ld\n", sem_id);

    int i, value;
    for(i = 0; i < 10; i++){
        assert(sem_get_value(sem_id, &value) == 0);
        assert(value == i + 1 && sem_post(sem_id) == 0);
    }
    printf("post ok.\n");
    
    for(; i > 0; i--){
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
    for (i = 0; i < 10; i ++) {
        yield();
    }

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
    if (sem_get_value(sem_id, &value_store) != 0 || value_store != value) {
        return -1;
    }
    return 0;
}

static void sem_rw_check_sem_value(void) {
    assert(sem_rw_check_sem_value_sub(sem_rw_count, 1) == 0 &&
           sem_rw_check_sem_value_sub(sem_rw_write, 1) == 0);
    assert(*sem_rw_pcount == 0);
}

static void sem_rw_init(void) {
    assert ((sem_rw_count = sem_init(1)) > 0 && (sem_rw_write = sem_init(1)) > 0);
    assert ((sem_rw_pcount = shmem_malloc(sizeof(int))) != nullptr);
    *sem_rw_pcount = 0;
}

static void sem_rw_reader(int id, int time) {
    printf("reader %d: (pid:%d) arrive\n", id, getpid());
    sem_wait(sem_rw_count);
    if (*sem_rw_pcount == 0) {
        sem_wait(sem_rw_write);
    }
    (*sem_rw_pcount)++;
    sem_post(sem_rw_count);

    printf("    reader_rw %d: (pid:%d) start %d\n", id, getpid(), time);
    sleep(time);
    printf("    reader_rw %d: (pid:%d) end %d\n", id, getpid(), time);

    sem_wait(sem_rw_count);
    (*sem_rw_pcount)--;
    if (*sem_rw_pcount == 0) {
        sem_post(sem_rw_write);
    }
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
    for (i = 0; i < total; i++) {
        assert(wait() == 0);
    }
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
            if(time == 0){
                sem_rw_writer(i, 100 + time * 10);
            }else{
                sem_rw_reader(i, 100 + time * 10);
            }          
            exit(0);
        }
    }
    for (i = 0; i < total; i++) { assert(wait() == 0); }
    printf("read_write_test ok.\n");
}

static void sem_rw_test(char *s){
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
           if(index + 1 == prime2_total){
                goto out;
           }
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

int spipetest_thread_main(void *arg){
    long id = (long)arg;
    printf("this is %d\n", id);
    size_t n = 1000;
    char *buf = malloc(sizeof(char) * n);
    assert(buf != nullptr);
    memset(buf, (char)id, n);
    int i, rounds = 20;
    for(i = 0; i < rounds; i++){
        size_t ret = spipe_write(&pipe, buf, n);
        if(ret != n){
            printf("pipe is closed, too early.\n");
            return -1;
        }
        if(id == 0){
            printf("send %d/%d\n", i, rounds);
        }
    }
    return 0;
}

void spipetest_process_main(void){
    int counts[spipetest_total], i;
    for(i = 0; i < spipetest_total; i++){
        counts[i] = 0;
    }
    char buf[128];
    size_t n = sizeof(buf);
    while(1){
        size_t ret = spipe_read(&pipe, buf, n);
        if(ret == 0) {
            break;
        }
        for(i = 0; i < ret; i++){
            counts[((ulong)buf[i]) % spipetest_total] ++;
        }
    }
    if(spipe_isclosed(&pipe)){
        for(i = 0; i < spipetest_total; i++){
            printf("%d reads %d\n", i, counts[i]);
        }
        exit(0);
    }
    exit(0xbad);
}

static void spipetest(char *s){
    int pid, i;
    spipe(&pipe);
    if((pid = fork()) == 0){
        spipetest_process_main();
    }
    assert(pid > 0);
    memset(spipetest_tids, 0, sizeof(Thread) * spipetest_total);
    for(i = 0; i < spipetest_total; i++){
        assert(thread(spipetest_thread_main, (void *)(long)i, spipetest_tids + i) == 0);
    }
    int exit_code;
    for (i = 0; i < spipetest_total; i++) {
        assert(thread_wait(spipetest_tids + i, &exit_code) == 0 && exit_code == 0);
    }
    for (i = 0; i < spipetest_total; i ++) {
        yield();
    }
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
void __event_test1(void){
    int pid, parent = getpid();
    if((pid = fork()) == 0){
        int event_num;
        while(event_recv(&pid, &event_num) == 0 && parent == pid){
            if(event_num == -1){
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

void __event_test3(void){
    int pid;
    if ((pid = fork()) == 0) {
        int event;
        assert(event_recv_timeout(NULL, &event, 100) == -E_TIMEOUT);
        exit(0);
    }
    assert(pid > 0);
    assert(waitpid(pid, NULL) == 0);
}

static void event_test(char *s){
    __event_test1();
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
        if(num == -1) goto out;
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
        if(*prime3work_dong == 0){
            if (event_send(pid, num) != 0) { goto out; }
        }
    }
out:
    printf("[%04d] %d quit.   recv_pid id %d\n", getpid(), index, recv_pid);
    // printf("pid id %d 111111\n", getpid());
    if(this != 2)
        event_send(recv_pid, -1);
    // printf("pid id %d 222222\n", getpid());
    wait();
    // printf("pid id %d 333333\n", getpid());
    exit(0);
}

static void prime3worktest(char *s){
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
static void mboxtest(char *s){
    int mbox_id = mbox_init(1);
    assert(mbox_id >= 0);
    Mboxbuf __buf, *buf = &__buf;
    buf->size = 4096;
    buf->len = buf->size;
    buf->data = malloc(sizeof(char) * buf->size);
    assert(buf->data != nullptr);
    char *data = (char *)(buf->data);
    int i;
    for(i = 0; i < buf->size; i++){
        data[i] = (char)i;
    }
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
    for(i = 0; i < buf->size; i++){
        assert(data[i] == (char)i);
    }
    save_ticks = gettime();
    assert(mbox_recv_timeout(mbox_id, buf, timeout) == -E_TIMEOUT);
    assert((ulong)(gettime() - save_ticks) >= timeout);
    assert(mbox_free(mbox_id) == 0);
    assert(mbox_send(mbox_id, buf) != 0);
    sleep(500);  //for wait mbox_cleanup;
}
//////////////////////////////////////////////////////////////////////////
const int mboxmap_mod = 23;
const int mboxmap_max_data = 2048;
const int mboxmap_max_slots = 1024;

int mboxmap_send(int id, void *data, size_t len){
    Mboxbuf buf;
    buf.data = data, buf.len = len;
    return mbox_send(id, &buf);
}

int mboxmap_recv(int id, void *data, size_t size, size_t *lenp){
    Mboxbuf buf;
    buf.data = data, buf.size = size;
    int ret;
    if((ret = mbox_recv(id, &buf)) == 0){
        *lenp = buf.len;
    }
    return ret;
}

void mboxmap_filter_main(int *data, int mbox_data, int mbox[]){
    int i, count[mboxmap_mod];
    size_t size = mboxmap_max_data * sizeof(int), len;
    while (mboxmap_recv(mbox_data, data, size, &len) == 0) {
        assert((len % sizeof(int)) == 0);
        memset(count, 0, sizeof(count));
        len /= sizeof(int);
        for(i = 0; i < len; i++){
            count[data[i] % mboxmap_mod]++;
        }
        for(i = 0; i < mboxmap_mod; i++){
            mboxmap_send(mbox[i], count + i, sizeof(int));
        }
    }
}

void mboxmap_select_main(int mbox, int pid){
    size_t len;
    int count = 0, ans;
    while (mboxmap_recv(mbox, &ans, sizeof(int), &len) == 0) {
        assert(len == sizeof(int));
        count += ans;
    }
    event_send(pid, count);
}

int mboxmap_wait_for_empty(int mbox){
    Mboxinfo info;
    while(1){
        if(mbox_info(mbox, &info) != 0){
            return -1;
        }
        if(info.slots == 0){
            return 0;
        }
        sleep(10);
    }
}

int mboxmap_wait_for_quit(int count[], int pids[]){
    int i, j, pid, event, ans[mboxmap_mod];
    memset(ans, 0, sizeof(ans));
    for(i = 0; i < mboxmap_mod; i++){
        if(event_recv(&pid, &event) != 0){
            return -1;
        }
        for(j = 0; j < mboxmap_mod; j++){
            if(pids[j] == pid){
                ans[j] = event;
                printf("-- recv count %02d: %08d\n", j, event);
            }
        }
    }
    int err = 0;
    for(i = 0; i < mboxmap_mod; i++){
        if(count[i] != ans[i]){
            err++;
            printf("wrong: %d, %d, %d.\n", i, count[i], ans[i]);
        }
    }
    return err;
}

static void mboxmaptest(char *s){
    int i, j, k, mbox[mboxmap_mod], count[mboxmap_mod];
    for(i = 0; i < mboxmap_mod; i++){
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

    for(i = 0; i < mboxmap_mod; i++){
        if((s_pids[i] = fork()) == 0){
            mboxmap_select_main(mbox[i], this);
            exit(0);
        }
        assert(s_pids[i] > 0);
    }
    int f_pids[100], f_pids_num = sizeof(f_pids) / sizeof(f_pids[0]);
    memset(f_pids, 0, sizeof(f_pids));

    for(i = 0; i < f_pids_num; i++){
        if((f_pids[i] = fork()) == 0){
            // memset(data, 0, size);
            mboxmap_filter_main(data, mbox_data, mbox);
            exit(0);
        }
        assert(f_pids[i] > 0);
    }
    printf("fork children ok.\n");
    memset(count, 0, sizeof(count));
    srand(913);
    for(i = 0; i < 5; i++){
        for(j = 0; j < 512; j++){
            for(k = 0; k < mboxmap_max_data; k++){
                data[k] = simulate_rand();
                count[data[k] % mboxmap_mod]++;
            }
            assert(mboxmap_send(mbox_data, data, len) == 0);    
        }
        printf("round %d\n", i);
    }
    for (i = 0; i < mboxmap_mod; i ++) {
        printf("-- send count %02d: %08d\n", i, count[i]);
    }
    sleep(500);
    if(mboxmap_wait_for_empty(mbox_data) == 0){
        assert(mbox_free(mbox_data) == 0);
        int exit_code;
        for(i = 0; i < f_pids_num; i++){
            waitpid(f_pids[i], &exit_code);
            assert(exit_code == 0);
        }
        for(i = 0; i < mboxmap_mod; i++){
            assert(mboxmap_wait_for_empty(mbox[i]) == 0);
        }
        printf("wait children ok.\n");
        for(i = 0; i < mboxmap_mod; i++){
            assert(mbox_free(mbox[i]) == 0);
        }
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
        while(sigtest_exit_flag == false);
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
        printf("%d page nums diff\n", nr_free_pages_store - nr_free_pages_store1);
        printf("%d slab nums diff\n", slab_allocated_store1 - slab_allocated_store);
        while(1);
        assert(nr_free_pages_store == nr_free_pages_store1);
        assert(slab_allocated_store == slab_allocated_store1);
        return xstatus == 0;
    }
}

struct test {
    void (*f)(char *);
    char *s;
} tests[] = {
    // {forktest, "forktest"},
    // {yieldtest, "yieldtest"},
    // {bsstest, "bsstest"},
    // {exittest, "exittest"},
    // {forktreetest, "forktreetest"},
    // {spintest, "spintest"},
    // {brktest, "brktest"},
    // {brkfreetest, "brkfreetest"},
    // {sleepkilltest, "sleepkilltest"},
    // {sleeptest, "sleeptest"},
    // {cowtest, "cowtest"},
    // {mmaptest, "mmaptest"},
    // {shmemtest, "shmemtest"},
    // {threadtest, "threadtest"},
    // {threadforktest, "threadforktest"},
    // {threadworktest, "threadworktest"},
    // {primeworktest, "primeworktest"},
    // {sem_test, "sem_test"},
    // {sem_rw_test, "sem_rw_test"},
    // {prime2worktest, "prime2worktest"},
    // {spipetest, "spipetest"},
    // {sem2_test, "sem2_test"},
    // {sem3_test, "sem3_test"},
    // {event_test, "event_test"},
    // {prime3worktest, "prime3worktest"},
    // {mboxtest, "mboxtest"},
    // {mboxmaptest, "mboxmaptest"},
    {sigtest, "sigtest"},
    // {swaptest, "swaptest"},
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
