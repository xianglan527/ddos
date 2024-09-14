#include "user.h"
#include "string.h"
#include "lock.h"

static lock_t fork_lock = INIT_LOCK;

void lock_fork(void){
    lock(&fork_lock);
}

void unlock_fork(void){
    unlock(&fork_lock);
}

int __write__(int, const void *, int);
int __sti__();
int __cli__();
int __getpid__(void);
int __fork__(void);
int __exit__(int) __attribute__((noreturn));
int __waitpid__(int, int *);
int __yield__(void);
int __exec__(char *path, char **argv);
int __kill__(int);
int __sbrk__(uintptr_t *);
int __sleep__(ulong);
uint64_t __gettime__(void);
uint64_t __get_free_page_size__(void);
uint64_t __get_slab_allocated_size__(void);
int __mmap__(uintptr_t *addr_store, size_t len, uint32_t mmap_flags);
int __munmap__(uintptr_t addr, size_t len);
int __shmem__(uintptr_t *addr_store, size_t len, uint32_t mmap_flags);
int __clone(uint32_t clone_flags, uintptr_t stack, int (*fn)(void *), void *arg);
long __sem_init__(int value);
int __sem_post__(sem_t sem_id);
int __sem_wait__(sem_t sem_id, ulong timeout);
int __sem_free__(sem_t sem_id);
int __sem_get_value__(sem_t sem_id, int *value_store);
int __event_send__(int pid, int event_num, ulong timeout);
int __event_recv__(int *pid_store, int *event_num_store, ulong timeout);
int __mbox_init__(size_t max_slots);
int __mbox_send__(int id, Mboxbuf *buf, ulong timeout);
int __mbox_recv__(int id, Mboxbuf *buf, ulong timeout);
int __mbox_free__(int id);
int __mbox_info__(int id, Mboxinfo *info);

int write(int fd, const void *c, int len){
    return __write__(fd, c ,len);
}

int sti(void){
    return __sti__();
}

int cli(void){
    return __cli__();
}

int getpid(void){
    return __getpid__();
}

int fork(void){
    int ret;
    lock_fork();
    ret = __fork__();
    unlock_fork();
    return ret;
}

int exit(int error_code){
    __exit__(error_code);
}

int wait(void) {
    int ret;
    ret = waitpid(0, (void *)0);
    return ret;
}
void putc(int fd, char c) { write(fd, &c, 1); };

void puts(int fd, char *str) { write(fd, str, strlen(str) + 1); }

int waitpid(int pid, int *exit_code){
    return __waitpid__(pid, exit_code);
}

int yield(void){
    return __yield__();
}

int exec(char *path, char **argv){
    return __exec__(path, argv);
}

int kill(int pid){
    return __kill__(pid);
}

int sbrk(uintptr_t *newbrk){
    return __sbrk__(newbrk);
}

int sleep(ulong time){
    return __sleep__(time);
}

uint64_t gettime(void){
    return __gettime__();
}

uint64_t get_free_page_size(void){
    return __get_free_page_size__();
}

uint64_t get_slab_allocated_size(void){
    return __get_slab_allocated_size__();
}

int mmap(uintptr_t *addr_store, size_t len, uint32_t mmap_flags){
    return __mmap__(addr_store, len, mmap_flags);
}

int munmap(uintptr_t addr, size_t len){
    return __munmap__(addr, len);
}

int shmem(uintptr_t *addr_store, size_t len, uint32_t mmap_flags){
    return __shmem__(addr_store, len, mmap_flags);
}

int clone(uint32_t clone_flags, uintptr_t stack, int (*fn)(void *), void *arg) {
    int ret;
    lock_fork();
    ret = __clone(clone_flags, stack, fn, arg);
    unlock_fork();
    return ret;
}

long sem_init(int value){
    return __sem_init__(value);
}

int sem_post(sem_t sem_id){
    return __sem_post__(sem_id);
}

int sem_wait(sem_t sem_id){
    return __sem_wait__(sem_id, 0);
}

int sem_wait_timeout(sem_t sem_id, ulong timeout){
    return __sem_wait__(sem_id, timeout);
}

int sem_free(sem_t sem_id){
    return __sem_free__(sem_id);
}

int sem_get_value(sem_t sem_id, int *value_store){
    return __sem_get_value__(sem_id, value_store);
}

int event_send(int pid, int event_num){
    return __event_send__(pid, event_num, 0);
}

int event_send_timeout(int pid, int event_num, ulong timeout){
    return __event_send__(pid, event_num, timeout);
}

int event_recv(int *pid_store, int *event_num_store){
    return __event_recv__(pid_store, event_num_store, 0);
}

int event_recv_timeout(int *pid_store, int *event_num_store, ulong timeout) {
    return __event_recv__(pid_store, event_num_store, timeout);
}

int mbox_init(size_t max_slots){
    return __mbox_init__(max_slots);
}

int mbox_send(int id, Mboxbuf *buf){
    return __mbox_send__(id, buf, 0);
}

int mbox_send_timeout(int id, Mboxbuf *buf,ulong timeout){
    return __mbox_send__(id, buf, timeout);
}

int mbox_recv(int id, Mboxbuf *buf){
    memset(buf->data, 0, buf->size);
    return __mbox_recv__(id, buf, 0);
}

int mbox_recv_timeout(int id, Mboxbuf *buf,ulong timeout){
    memset(buf->data, 0, buf->size);
    return __mbox_recv__(id, buf, timeout);
}

int mbox_free(int id){
    return __mbox_free__(id);
}

int mbox_info(int id, Mboxinfo *info){
    return __mbox_info__(id, info);
}
