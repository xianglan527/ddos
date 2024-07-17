#include "syscall.h"
#include "assert.h"
#include "config.h"
#include "proc.h"
#include "stdio.h"
#include "string.h"

// void syscall(void) {
//     Proc *p = myproc();
//     cprintf("%s running \n", p->name);
//     task_delay(DELAY);
// }
#define NELEM(x) (sizeof(x) / sizeof((x)[0]))

extern uint64_t sys_write(void);
extern uint64_t sys_sti(void);
extern uint64_t sys_cli(void);
extern uint64_t sys_getpid(void);
extern uint64_t sys_fork(void);
extern uint64_t sys_exit(void);
extern uint64_t sys_wait(void);
extern uint64_t sys_yield(void);
extern uint64_t sys_exec(void);

int fetch_addr(uint64_t addr, uint64_t *ip){
    Proc *current = myproc();
    if(user_mem_check(current->mm, addr, sizeof(*ip), true) == 0)
        return -1;
    copy_user2kernel(current->mm->pagetable, (char *)ip, addr, sizeof(*ip));
    return 0;
}

int fetch_str(uint64_t addr, char *buf, int max){
    Proc *current = myproc();
    int err = copystr_user2kernel(current->mm->pagetable, buf, addr, max);
    if(err < 0)
        return err;
    return strlen(buf);
}

static uint64_t arg_raw(int n) {
    Proc *p = myproc();
    switch (n) {
        case 0: return p->trapframe->a0;
        case 1: return p->trapframe->a1;
        case 2: return p->trapframe->a2;
        case 3: return p->trapframe->a3;
        case 4: return p->trapframe->a4;
        case 5: return p->trapframe->a5;
    }
    panic("arg_raw");
    return -1;
}

int arg_int(int n, int *ip){
    *ip = arg_raw(n);
    return 0;
}

int arg_addr(int n, uint64_t *ip){
    *ip = arg_raw(n);
    return 0;
}

int arg_str(int n, char *buf, int max){
    uint64_t addr;
    if(arg_addr(n, &addr) < 0)
        return -1;
    return fetch_str(addr, buf, max);
}

static uint64_t (*syscalls[])(void) = {
    [SYS_getpid] = sys_getpid,
    [SYS_write] = sys_write,
    [SYS_sti] = sys_sti,
    [SYS_cli] = sys_cli,
    [SYS_fork] = sys_fork,
    [SYS_exit] = sys_exit,
    [SYS_waitpid] = sys_wait,
    [SYS_exec] = sys_exec,
    [SYS_yield] = sys_yield,
};

void syscall(void) {
    Proc *p = myproc();
    int num = p->trapframe->a7;
    if (num > 0 && num < NELEM(syscalls) && syscalls[num]) {
        p->trapframe->a0 = syscalls[num]();
    } else {
        cprintf("%d %s: unknown sys call %d\n", p->pid, p->name, num);
        p->trapframe->a0 = -1;
    }
}