#ifndef __LIBS_USER_H__
#define __LIBS_USER_H__
#include "mboxbuf.h"
#include "sigaction.h"
#include "stat.h"
#include "types.h"

int sti();
int cli();
int getpid(void);
int fork(void);
int exit(int) __attribute__((noreturn));
int wait(void);
void putc(int fd, char c);
void puts(int fd, char *str);
int waitpid(int, int *);
int yield(void);
int exec(char *path, char **argv);
int kill(int);
int sbrk(uintptr_t *);
int sleep(ulong);
uint64_t gettime(void);
uint64_t get_free_page_size(void);
uint64_t get_slab_allocated_size(void);
int mmap(uintptr_t *addr_store, size_t len, uint32_t mmap_flags);
int munmap(uintptr_t addr, size_t len);
int shmem(uintptr_t *addr_store, size_t len, uint32_t mmap_flags);
int clone(uint32_t clone_flags, uintptr_t stack, int (*fn)(void *), void *arg);
long sem_init(int value);
int sem_post(sem_t sem_id);
int sem_wait(sem_t sem_id);
int sem_wait_timeout(sem_t sem_id, ulong timeout);
int sem_free(sem_t sem_id);
int sem_get_value(sem_t sem_id, int *value_store);
int event_send(int pid, int event_num);
int event_send_timeout(int pid, int event_num, ulong timeout);
int event_recv(int *pid_store, int *event_num_store);
int event_recv_timeout(int *pid_store, int *event_num_store, ulong timeout);
int mbox_init(size_t max_slots);
int mbox_send(int id, Mboxbuf *buf);
int mbox_send_timeout(int id, Mboxbuf *buf, ulong timeout);
int mbox_recv(int id, Mboxbuf *buf);
int mbox_recv_timeout(int id, Mboxbuf *buf, ulong timeout);
int mbox_free(int id);
int mbox_info(int id, Mboxinfo *info);
int set_sigaction(int sig, Sigaction *sa);
int send_signal(int pid, int sig);
int setpriority(int pid, int priority);
int getpriority(int pid);
uint64_t get_proc_runticks(int pid);
int set_proc_cpu(int pid, int cpuid);
int clear_proc_setcpu(int pid);
int open(char *path, uint32_t open_flags);
long write(int fd, const void *base, size_t len);
long read(int fd, const void *c, size_t len);
int fstat(int fd, Stat *stat);
int dup(int fd);
int dup2(int fd1, int fd2);
int close(int fd);
#endif