#include "console.h"
#include "proc.h"
#include "riscv.h"
#include "slab.h"
#include "spinlock.h"
#include "string.h"
#include "syscall.h"
#include "stdio.h"

extern uint64_t ticks;
extern struct {
    Spinlock lock;
    int locking;
} pr;

uint64_t sys_write(void) {
    int n;
    uint64_t p;
    char str[MAXPATH];
    if (arg_int(2, &n) < 0 || arg_str(1, str, MAXPATH) < 0) return -1;
    assert(n <= MAXPATH);
    cprintf(str);
    return n;
}

uint64_t sys_sti(void) { return 0; }

uint64_t sys_cli(void) { return 0; }

uint64_t sys_getpid(void) { return myproc()->pid; }

uint64_t sys_fork(void) { return do_fork(0); }

uint64_t sys_exit(void) {
    int n;
    arg_int(0, &n);
    do_exit(n);
    return 0;
}

uint64_t sys_wait(void) {
    int pid;
    uint64_t store;
    arg_int(0, &pid);
    arg_addr(1, &store);
    return do_wait(pid, (int *)store);
}

uint64_t sys_yield(void) {
    do_yield();
    return 0;
}

uint64_t sys_exec(void) {
    char path[MAXPATH], *argv[MAXARG];
    int i;
    uint64_t uargv, uarg;
    if (arg_str(0, path, MAXPATH) < 0 || arg_addr(1, &uargv) < 0) return -1;
    memset(argv, 0, sizeof(argv));
    for (i = 0;; i++) {
        if (i >= MAXARG) goto bad;
        if (fetch_addr(uargv + sizeof(uint64_t) * i, (uint64_t *)&uarg) < 0) goto bad;
        if (uarg == 0) {
            argv[i] = 0;
            break;
        }
        argv[i] = kmalloc(MAXPATH);
        if (argv[i] == nullptr) goto bad;
        if (fetch_str(uarg, argv[i], MAXPATH) < 0) goto bad;
    }
    int ret = do_execve(path, argv);
    for (i = 0; i < MAXARG && argv[i] != nullptr; i++) kfree(argv[i]);
    return ret;
bad:
    for (i = 0; i < MAXARG && argv[i] != nullptr; i++) kfree(argv[i]);
    return -1;
}

uint64_t sys_kill(void){
    int pid;
    if(arg_int(0, &pid) < 0 )
        return -1;
    return do_kill(pid);
}

uint64_t sys_sbrk(void) {
    uint64_t store;
    arg_addr(0, &store);
    return do_brk((uintptr_t *)store);
}

uint64_t sys_sleep(void){
    long time;
    if(arg_long(0, &time) < 0)
        return -1;
    return do_sleep((ulong)time);
}

uint64_t sys_gettime(void) {
    return ticks;
}

uint64_t sys_get_free_page_size(void){
    return nr_free_pages();
}

uint64_t sys_get_slab_allocated_size(void){
    return slab_allocated();
}