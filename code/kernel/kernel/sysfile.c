#include "error.h"
#include "file.h"
#include "iobuf.h"
#include "mbox.h"
#include "proc.h"
#include "riscv.h"
#include "signal.h"
#include "slab.h"
#include "spinlock.h"
#include "stat.h"
#include "stdio.h"
#include "string.h"
#include "syscall.h"
#include "vfs.h"

#define IOBUF_SIZE 4096

extern uint64_t ticks;
extern struct {
    Spinlock lock;
    int locking;
} pr;

// uint64_t sys_write(void) {
//     int n;
//     uint64_t p;
//     char str[MAXPATH];
//     if (arg_int(2, &n) < 0 || arg_str(1, str, MAXPATH) < 0) return -1;
//     assert(n <= MAXPATH);
//     cprintf(str);
//     return n;
// }

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

uint64_t sys_kill(void) {
    int pid;
    if (arg_int(0, &pid) < 0) return -1;
    return do_kill(pid);
}

uint64_t sys_sbrk(void) {
    uint64_t store;
    arg_addr(0, &store);
    return do_brk((uintptr_t *)store);
}

uint64_t sys_sleep(void) {
    long time;
    if (arg_long(0, &time) < 0) return -1;
    return do_sleep((ulong)time);
}

uint64_t sys_gettime(void) { return ticks; }

uint64_t sys_get_free_page_size(void) { return nr_free_pages(); }

uint64_t sys_get_slab_allocated_size(void) { 
    // sinode_dump_struct_lock();
    return slab_allocated(); 
}

uint64_t sys_clone(void) {
    int32_t clone_flags;
    uintptr_t stack;
    arg_int(0, &clone_flags);
    arg_addr(1, &stack);
    return do_fork((uint32_t)clone_flags, stack);
}

uint64_t sys_exit_thread(void) {
    int error_code;
    arg_int(0, &error_code);
    do_exit_thread(error_code);
    return 0;
}

uint64_t sys_mmap(void) {
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

uint64_t sys_sem_init(void) {
    int value;
    arg_int(0, &value);
    return ipc_sem_init(value);
}

uint64_t sys_sem_post(void) {
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

uint64_t sys_sem_get_value(void) {
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
    return ipc_mbox_send(id, (Mboxbuf *)buf, timeout);
}

uint64_t sys_mbox_recv(void) {
    int id;
    arg_int(0, &id);
    uintptr_t buf;
    arg_addr(1, &buf);
    long timeout;
    arg_long(2, &timeout);
    return ipc_mbox_recv(id, (Mboxbuf *)buf, timeout);
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

uint64_t sys_sigreturn(void) { return ipc_sigreturn(); }

uint64_t sys_setpriority(void) {
    int pid;
    arg_int(0, &pid);
    int priority;
    arg_int(1, &priority);
    Proc *proc = find_proc(pid);
    if (proc == nullptr) { return -E_INVAL; }
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

uint64_t sys_open(void) {
    int ret;
    char path[MAXPATH];
    if (arg_str(0, path, MAXPATH) < 0) return -E_INVAL;
    long open_flags;
    arg_long(1, &open_flags);
    ret = file_open(path, (uint32_t)open_flags);
    return ret;
}

uint64_t sys_close(void) {
    int ret;
    File *file;
    int fd;
    arg_int(0, &fd);
    if ((ret = fd2file(fd, &file)) != 0) { return ret; }
    filemap_close(file);
    return 0;
}

#define read_max_onece  (SFS_MAXOPBLOCKS * SFS_BSIZE)

uint64_t sys_read(void) {
    int ret;
    struct file *file;
    int fd;
    arg_int(0, &fd);
    long __base;
    arg_long(1, &__base);
    void *base = (void *)__base;
    long __len;
    arg_long(2, &__len);
    size_t len = (size_t)__len;
    Mm_struct *mm = myproc()->mm;
    if (len == 0) { return 0; }
    if (!file_testfd(fd, 1, 0)) { return -E_INVAL; }
    void *buffer;
    if ((buffer = kmalloc(read_max_onece)) == nullptr) { return -E_NO_MEM; }
    ret = 0;
    size_t copied = 0, alen;
    while (len != 0) {
        if ((alen = read_max_onece) > len) { alen = len; }
        ret = file_read(fd, buffer, alen, &alen);
        // cprintf("444444 fd is %d len is %d alen is %d\n\n", fd, len, alen);
        if (alen != 0) {
            lock_mm(mm);
            {
                copy_kernel2user(mm->pagetable, (uintptr_t)base, (char *)buffer, alen);
                assert(len >= alen);
                base += alen, len -= alen, copied += alen;
            }
            unlock_mm(mm);
        }
        if (ret != 0 || alen == 0) { break; }
    }
    kfree(buffer);
    if (copied != 0) { 
        return copied; 
    }
    return ret;
}

#define write_max_onece  (((SFS_MAXOPBLOCKS - 1 - 1 - 1 - 2) / 2) * SFS_BSIZE)

uint64_t sys_write(void) {
    int ret;
    int fd;
    arg_int(0, &fd);
    long __base;
    arg_long(1, &__base);
    void *base = (void *)__base;
    long __len;
    arg_long(2, &__len);
    size_t len = (size_t)__len;
    Mm_struct *mm = myproc()->mm;
    if (len == 0) { return 0; }
    if (!file_testfd(fd, 0, 1)) { return -E_INVAL; }
    void *buffer;
    // write a few blocks at a time to avoid exceeding
    // the maximum log transaction size, including
    // i-node, 2 indirect block, allocation blocks,
    // and 2 blocks of slop for non-aligned writes.
    if ((buffer = kmalloc(write_max_onece)) == nullptr) { return -E_NO_MEM; }
    ret = 0;
    size_t copied = 0, alen;
    while (len != 0) {
        if ((alen = write_max_onece) > len) { alen = len; }
        lock_mm(mm);
        copy_user2kernel(mm->pagetable, (char *)buffer, (uintptr_t)base, alen);
        unlock_mm(mm);
        begin_op();
        ret = file_write(fd, buffer, alen, &alen);
        if (alen != 0) {
            assert(len >= alen);
            base += alen, len -= alen, copied += alen;
        }
        end_op();
        if (ret != 0 || alen == 0) { break; }
    }
    kfree(buffer);
    if (copied != 0) { return copied; }
    return ret;
}

uint64_t sys_fstat(void) {
    int ret;
    int fd;
    arg_int(0, &fd);
    long __stat;
    arg_long(1, &__stat);
    Stat *stat = (Stat *)__stat;
    Mm_struct *mm = myproc()->mm;
    Stat __local_stat, *local_stat = &__local_stat;
    if ((ret = file_fstat(fd, local_stat)) != 0) { return ret; }
    lock_mm(mm);
    { copy_kernel2user(mm->pagetable, (uintptr_t)stat, (char *)local_stat, sizeof(Stat)); }
    unlock_mm(mm);
    return 0;
}

uint64_t sys_dup(void) {
    int fd1, fd2;
    arg_int(0, &fd1);
    arg_int(1, &fd2);
    return file_dup(fd1, fd2);
}

uint64_t sys_mkpipe(void) {
    long __fd_store;
    arg_long(0, &__fd_store);
    int *fd_store = (int *)__fd_store;
    if(fd_store == nullptr) return -E_INVAL;
    int ret, fd[2];
    Mm_struct *mm = myproc()->mm;
    if ((ret = file_pipe(fd)) == 0) {
        lock_mm(mm);
        copy_kernel2user(mm->pagetable, (uintptr_t)fd_store, (char *)fd, sizeof(fd));
        unlock_mm(mm);
    } else {
        file_close(fd[0]), file_close(fd[1]);
    }
    return ret;
}

uint64_t sys_mkfifo(void) {
    int ret;
    char name[MAXPATH];
    if (arg_str(0, name, MAXPATH) < 0) return -E_INVAL;
    long open_flags;
    arg_long(1, &open_flags);
    ret = file_mkfifo(name, (uint32_t)open_flags);
    return ret;
}

uint64_t sys_seek(void){
    int fd;
    arg_int(0, &fd);
    long pos;
    arg_long(1, &pos);
    int whence;
    arg_int(2, &whence);
    return file_seek(fd, (off_t)pos, whence);
}

uint64_t sys_fsync(void){
    int fd;
    arg_int(0, &fd);
    begin_op();
    int ret = file_fsync(fd);
    end_op();
    return ret;
}

uint64_t sys_chdir(void){
    char path[MAXPATH];
    if (arg_str(0, path, MAXPATH) < 0) return -E_INVAL;
    return vfs_chdir(path);
}

uint64_t sys_getcwd(void){
    int ret;
    long __buf;
    arg_long(0, &__buf);
    char *buf = (char *)__buf;
    long __len;
    arg_long(1, &__len);
    size_t len = (size_t)__len;
    Mm_struct *mm = myproc()->mm;
    char *buffer;
    if ((buffer = kmalloc(len)) == nullptr) { return -E_NO_MEM; }
    Iobuf __iob, *iob = iobuf_init(&__iob, buffer, len, 0);
    ret = vfs_getcwd(iob);
    if(ret < 0){
        kfree(buffer);
        return ret;
    }
    lock_mm(mm);
    copy_kernel2user(mm->pagetable, (uintptr_t)buf, (char *)buffer, iobuf_used(iob)); 
    unlock_mm(mm);
    kfree(buffer);
    return 0;
}
uint64_t sys_getdirentry(void){
    int ret;
    int fd;
    arg_int(0, &fd);
    long __dirent;
    arg_long(1, &__dirent);
    Dirent *dirent = (Dirent *)__dirent;
    Mm_struct *mm = myproc()->mm;
    Dirent local_dirent;
    lock_mm(mm);
    copy_user2kernel(mm->pagetable, (char *)&local_dirent, (uint64_t)dirent, sizeof(*dirent));
    unlock_mm(mm);
    ret = file_getdirentry(fd, &local_dirent);
    if(ret < 0) return ret;
    lock_mm(mm);
    copy_kernel2user(mm->pagetable, (uint64_t)dirent, (char *)&local_dirent, sizeof(*dirent));
    unlock_mm(mm);
    return 0;
}

uint64_t sys_link(void){
    char old_path[MAXPATH];
    if (arg_str(0, old_path, MAXPATH) < 0) return -E_INVAL;
    char new_path[MAXPATH];
    if (arg_str(1, new_path, MAXPATH) < 0) return -E_INVAL;
    return vfs_link(old_path, new_path);
}

uint64_t sys_unlink(void){
    char path[MAXPATH];
    if (arg_str(0, path, MAXPATH) < 0) return -E_INVAL;
    return vfs_unlink(path);
}

uint64_t sys_mkdir(void) {
    char path[MAXPATH];
    if (arg_str(0, path, MAXPATH) < 0) return -E_INVAL;
    return vfs_mkdir(path);
}

uint64_t sys_symlink(void) {
    char old_path[MAXPATH];
    if (arg_str(0, old_path, MAXPATH) < 0) return -E_INVAL;
    char new_path[MAXPATH];
    if (arg_str(1, new_path, MAXPATH) < 0) return -E_INVAL;
    return vfs_symlink(old_path, new_path);
}