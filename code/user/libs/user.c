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

int wait(void) {
    int ret;
    ret = waitpid(0, (void *)0); 
    return ret;
}
void putc(int fd, char c) { write(fd, &c, 1); };

void puts(int fd, char *str) {write(fd, str, strlen(str) + 1);}

