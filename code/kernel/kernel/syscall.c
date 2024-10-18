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

extern uint64_t sys_sti(void);
extern uint64_t sys_cli(void);
extern uint64_t sys_getpid(void);
extern uint64_t sys_fork(void);
extern uint64_t sys_exit(void);
extern uint64_t sys_wait(void);
extern uint64_t sys_yield(void);
extern uint64_t sys_exec(void);
extern uint64_t sys_kill(void);
extern uint64_t sys_sbrk(void);
extern uint64_t sys_sleep(void);
extern uint64_t sys_gettime(void);
extern uint64_t sys_get_free_page_size(void);
extern uint64_t sys_get_slab_allocated_size(void);
extern uint64_t sys_clone(void);
extern uint64_t sys_exit_thread(void);
extern uint64_t sys_mmap(void);
extern uint64_t sys_munmap(void);
extern uint64_t sys_shmem(void);
extern uint64_t sys_sem_init(void);
extern uint64_t sys_sem_post(void);
extern uint64_t sys_sem_wait(void);
extern uint64_t sys_sem_free(void);
extern uint64_t sys_sem_get_value(void);
extern uint64_t sys_event_send(void);
extern uint64_t sys_event_recv(void);
extern uint64_t sys_mbox_init(void);
extern uint64_t sys_mbox_send(void);
extern uint64_t sys_mbox_recv(void);
extern uint64_t sys_mbox_free(void);
extern uint64_t sys_mbox_info(void);
extern uint64_t sys_set_sigaction(void);
extern uint64_t sys_send_signal(void);
extern uint64_t sys_sigreturn(void);
extern uint64_t sys_setpriority(void);
extern uint64_t sys_getpriority(void);
extern uint64_t sys_get_proc_runticks(void);
extern uint64_t sys_set_proc_cpu(void);
extern uint64_t sys_clear_proc_setcpu(void);
extern uint64_t sys_open(void);
extern uint64_t sys_close(void);
extern uint64_t sys_read(void);
extern uint64_t sys_write(void);
extern uint64_t sys_fstat(void);
extern uint64_t sys_dup(void);

int fetch_addr(uint64_t addr, uint64_t *ip){
    Proc *current = myproc();
    lock_mm(current->mm);
    if(user_mem_check(current->mm, addr, sizeof(*ip), true) == 0){
        unlock_mm(current->mm);
        return -1;
    }
    copy_user2kernel(current->mm->pagetable, (char *)ip, addr, sizeof(*ip));
    unlock_mm(current->mm);
    return 0;
}

int fetch_str(uint64_t addr, char *buf, int max){
    Proc *current = myproc();
    lock_mm(current->mm);
    int err = copystr_user2kernel(current->mm->pagetable, buf, addr, max);
    if(err < 0){
        unlock_mm(current->mm);
        return err;
    }
    unlock_mm(current->mm);
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

int arg_long(int n, long *ip) {
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
    [SYS_sti] = sys_sti,
    [SYS_cli] = sys_cli,
    [SYS_fork] = sys_fork,
    [SYS_exit] = sys_exit,
    [SYS_waitpid] = sys_wait,
    [SYS_exec] = sys_exec,
    [SYS_yield] = sys_yield,
    [SYS_kill] = sys_kill,
    [SYS_sbrk] = sys_sbrk,
    [SYS_sleep] = sys_sleep,
    [SYS_gettime] = sys_gettime,
    [SYS_get_free_page_size] = sys_get_free_page_size,
    [SYS_get_slab_allocated_size] = sys_get_slab_allocated_size,
    [SYS_clone] = sys_clone,
    [SYS_exit_thread] = sys_exit_thread,
    [SYS_mmap] = sys_mmap,
    [SYS_munmap] = sys_munmap,
    [SYS_shmem] = sys_shmem,
    [SYS_sem_init] = sys_sem_init,
    [SYS_sem_post] = sys_sem_post,
    [SYS_sem_wait] = sys_sem_wait,
    [SYS_sem_free] = sys_sem_free,
    [SYS_sem_get_value] = sys_sem_get_value,
    [SYS_event_send] = sys_event_send,
    [SYS_event_recv] = sys_event_recv,
    [SYS_mbox_init] = sys_mbox_init,
    [SYS_mbox_send] = sys_mbox_send,
    [SYS_mbox_recv] = sys_mbox_recv,
    [SYS_mbox_free] = sys_mbox_free,
    [SYS_mbox_info] = sys_mbox_info,
    [SYS_set_sigaction] = sys_set_sigaction,
    [SYS_send_signal] = sys_send_signal,
    [SYS_sigreturn] = sys_sigreturn,
    [SYS_setpriority] = sys_setpriority,
    [SYS_getpriority] = sys_getpriority,
    [SYS_get_proc_runticks] = sys_get_proc_runticks,
    [SYS_set_proc_cpu] = sys_set_proc_cpu,
    [SYS_clear_proc_setcpu] = sys_clear_proc_setcpu,
    [SYS_open] = sys_open,
    [SYS_close] = sys_close,
    [SYS_write] = sys_write,
    [SYS_read] = sys_read,
    [SYS_fstat] = sys_fstat,
    [SYS_dup] = sys_dup,
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