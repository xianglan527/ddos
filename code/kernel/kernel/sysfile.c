#include "console.h"
#include "spinlock.h"
#include "syscall.h"
#include "riscv.h"

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
