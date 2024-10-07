#include "console.h"
#include "proc.h"
#include "riscv.h"
#include "slab.h"
#include "spinlock.h"
#include "string.h"
#include "syscall.h"
#include "stdio.h"
#include "mbox.h"
#include "signal.h"
#include "error.h"

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

uint64_t sys_fork(void) { return do_fork(0, 0); }

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

uint64_t sys_clone(void){
    int32_t clone_flags;
    uintptr_t stack;
    arg_int(0, &clone_flags);
    arg_addr(1, &stack);
    return do_fork((uint32_t)clone_flags, stack);
}

uint64_t sys_exit_thread(void){
    int error_code;
    arg_int(0, &error_code);
    do_exit_thread(error_code);
    return 0;
}

uint64_t sys_mmap(void){
    uintptr_t addr_store;
    long len;
    int mmap_flags;
    arg_addr(0, &addr_store);
    arg_long(1, &len);
    arg_int(2, &mmap_flags);
    return do_mmap((uintptr_t *)addr_store, (size_t)len, (uint32_t)mmap_flags);
}

uint64_t sys_munmap(void) {
    uintptr_t addr;
    long len;
    arg_addr(0, &addr);
    arg_long(1, &len);
    return do_munmap(addr, (size_t)len);
}

uint64_t sys_shmem(void) {
    uintptr_t addr_store;
    long len;
    int mmap_flags;
    arg_addr(0, &addr_store);
    arg_long(1, &len);
    arg_int(2, &mmap_flags);
    return do_shmem((uintptr_t *)addr_store, (size_t)len, (uint32_t)mmap_flags);
}

uint64_t sys_sem_init(void){
    int value;
    arg_int(0, &value);
    return ipc_sem_init(value);
}

uint64_t sys_sem_post(void){
    long sem_id;
    arg_long(0, &sem_id);
    return ipc_sem_post((sem_t)sem_id);
}

uint64_t sys_sem_wait(void) {
    long sem_id;
    arg_long(0, &sem_id);
    long timeout;
    arg_long(1, &timeout);
    return ipc_sem_wait((sem_t)sem_id, (ulong)timeout);
}

uint64_t sys_sem_free(void) {
    long sem_id;
    arg_long(0, &sem_id);
    return ipc_sem_free((sem_t)sem_id);
}

uint64_t sys_sem_get_value(void){
    long sem_id;
    arg_long(0, &sem_id);
    uintptr_t value_store;
    arg_addr(1, &value_store);
    return ipc_sem_get_value((sem_t)sem_id, (int *)value_store);
}

uint64_t sys_event_send(void) {
    int pid;
    arg_int(0, &pid);
    int event_num;
    arg_int(1, &event_num);
    long timeout;
    arg_long(2, &timeout);
    return ipc_event_send(pid, event_num, (ulong)timeout);
}

uint64_t sys_event_recv(void) {
    uint64_t pid_store;
    uint64_t event_num_store;
    arg_addr(0, &pid_store);
    arg_addr(1, &event_num_store);
    long timeout;
    arg_long(2, &timeout);
    return ipc_event_recv((int *)pid_store, (int *)event_num_store, (ulong)timeout);
}

uint64_t sys_mbox_init(void) {
    long max_slots;
    arg_long(0, &max_slots);
    return ipc_mbox_init((size_t)max_slots);
}

uint64_t sys_mbox_send(void) {
    int id;
    arg_int(0, &id);
    uintptr_t buf;
    arg_addr(1, &buf);
    long timeout;
    arg_long(2, &timeout);
    return ipc_mbox_send(id, (Mboxbuf *)buf, (ulong)timeout);
}

uint64_t sys_mbox_recv(void) {
    int id;
    arg_int(0, &id);
    uintptr_t buf;
    arg_addr(1, &buf);
    long timeout;
    arg_long(2, &timeout);
    return ipc_mbox_recv(id, (Mboxbuf *)buf, (ulong)timeout);
}

uint64_t sys_mbox_free(void) {
    int id;
    arg_int(0, &id);
    return ipc_mbox_free(id);
}

uint64_t sys_mbox_info(void) {
    int id;
    arg_int(0, &id);
    uintptr_t info;
    arg_addr(1, &info);
    return ipc_mbox_info(id, (Mboxinfo *)info);
}

uint64_t sys_set_sigaction(void) {
    int sig;
    arg_int(0, &sig);
    uintptr_t sa;
    arg_addr(1, &sa);
    return ipc_set_sigaction(sig, (Sigaction *)sa);
}

uint64_t sys_send_signal(void) {
    int pid;
    arg_int(0, &pid);
    int sig;
    arg_int(1, &sig);
    return ipc_send_signal(pid, sig);
}

uint64_t sys_sigreturn(void){
    return ipc_sigreturn();
}

uint64_t sys_setpriority(void){
    int pid;
    arg_int(0, &pid);
    int priority;
    arg_int(1, &priority);
    Proc *proc = find_proc(pid);
    if(proc == nullptr){
        return -E_INVAL;
    }
    acquire(&proc->lock);
    proc->priority = priority;
    release(&proc->lock);
    return 0;
}

uint64_t sys_getpriority(void) {
    int pid;
    arg_int(0, &pid);
    Proc *proc = find_proc(pid);
    int ret = -E_INVAL;
    if (proc == nullptr) { return ret; }
    acquire(&proc->lock);
    ret = proc->priority;
    release(&proc->lock);
    return ret;
}

uint64_t sys_get_proc_runticks(void) {
    int pid;
    arg_int(0, &pid);
    Proc *proc = find_proc(pid);
    assert(proc != nullptr);
    ulong ret;
    acquire(&proc->lock);
    ret = proc->runs;
    release(&proc->lock);
    return ret;
}

uint64_t sys_set_proc_cpu(void) {
    int pid;
    arg_int(0, &pid);
    int cpuid;
    arg_int(1, &cpuid);
    Proc *proc = find_proc(pid);
    if (proc == nullptr) { return -E_INVAL; }
    if (cpuid < 0 || cpuid >= CPUS) { return -E_INVAL; }
    // acquire(&proc->lock);
    set_proc_cpu(proc, &cpus[cpuid]);
    // release(&proc->lock);
    return 0;
}

uint64_t sys_clear_proc_setcpu(void) {
    int pid;
    arg_int(0, &pid);
    Proc *proc = find_proc(pid);
    if (proc == nullptr) { return -E_INVAL; }
    clear_proc_setcpu(proc);
    return 0;
}