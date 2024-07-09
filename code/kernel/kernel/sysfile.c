#include "console.h"
#include "spinlock.h"
#include "syscall.h"
#include "riscv.h"
#include "proc.h"

extern Spinlock user_lock;

uint64_t sys_write(void) {
    int n;
    uint64_t p;
    if (arg_int(2, &n) < 0 || arg_addr(1, &p) < 0) return -1;
    return console_write(true, p, n);
}

uint64_t sys_sti(void){
    return 0;
}

uint64_t sys_cli(void){
    return 0;
}

uint64_t sys_getpid(void){
    return myproc()->pid;
}

uint64_t sys_fork(void){
    return do_fork(0);
}

uint64_t sys_exit(void){
    int n;
    arg_int(0, &n);
    do_exit(n);
    return 0;
}

uint64_t sys_wait(void){
    int pid;
    uint64_t store;
    arg_int(0, &pid);
    arg_addr(1, &store);
    return do_wait(pid, (int *)store);
}

uint64_t sys_yield(void){
    do_yield();
    return 0;
}

