#include "user.h"
#include "string.h"
#include "lock.h"
#include "sysdef.h"
#include "socket.h"

static lock_t fork_lock = INIT_LOCK;

void lock_fork(void){
    lock(&fork_lock);
}

void unlock_fork(void){
    unlock(&fork_lock);
}

static int initfd(int fd2, char *path, uint32_t open_flags) {
    int fd1, ret;
    if ((fd1 = open(path, open_flags)) < 0) { return fd1; }
    if (fd1 != fd2) {
        close(fd2);
        ret = dup2(fd1, fd2);
        close(fd1);
    }
    return ret;
}

static int openstderr(void){
    close(2);
    int ret = dup2(1, 2);
    return ret;
}

void prework(void) {
    int fd;
    if ((fd = initfd(0, "stdin:", O_RDONLY)) < 0) { while (1); }
    if ((fd = initfd(1, "stdout:", O_WRONLY)) < 0) { while (1); }
    if((fd = openstderr()) < 0){while (1);}
}

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
int __set_sigaction__(int sig, Sigaction *sa);
int __send_signal__(int pid, int sig);
int __sigreturn__(void);
int __setpriority__(int pid, int priority);
int __getpriority__(int pid);
uint64_t __get_proc_runticks__(int pid);
int __set_proc_cpu__(int pid, int cpuid);
int __clear_proc_setcpu__(int pid);
int __open__(char *path, uint32_t open_flags);
long __write__(int fd, const void * base, size_t len);
long __read__(int fd, const void *base, size_t len);
int __fstat__(int fd, Stat *stat);
int __dup__(int fd1, int fd2);
int __close__(int fd);
int __mkpipe__(int *fd_store);
int __mkfifo__(char *name, uint32_t open_flags);
int __seek__(int fd, off_t pos, int whence);
int __fsync__(int fd);
int __chdir__(char *path);
int __getcwd__(char *buf, size_t len);
int __getdirentry__(int fd, Dirent *dirent);
int __link__(char *oldpath, char *newpath);
int __unlink__(char *path);
int __mkdir__(char *path);
int __symlink__(char *oldpath, char *newpath);
int __socket__(int family, int type, int protocol);
ssize_t __sendto__(int sockfd, void *buf, size_t len, int flags, Sockaddr *dest, socklen_t dest_len);
ssize_t __recvfrom__(int sockfd, void *buf, size_t len, int flags, Sockaddr *dest, socklen_t *dest_len);
int __setsockopt__(int sockfd, int level, int optname, char *optval, int optlen);
int __closesocket__(int sockfd);
int __connect__(int sockfd, Sockaddr *addr, socklen_t len);
ssize_t __send__(int sockfd, void *buf, size_t len, int flags);
ssize_t __recv__(int sockfd, void *buf, size_t len, int flags);
int __bind__(int sockfd, Sockaddr *addr, socklen_t len);
int __accept__(int sockfd, Sockaddr *addr, socklen_t *len);
int __listen__(int sockfd, int backlog);
int __gethostbyname_r__(char *name, Hostent *ret, char *buf, size_t buflen, Hostent **result, int *h_errnop);

long write(int fd, const void *c, size_t len){
    return __write__(fd, c ,len);
}

int open(char *path, uint32_t open_flags){
    return __open__(path, open_flags);
}

long read(int fd, const void *c, size_t len){
    return __read__(fd, c ,len);
}

int fstat(int fd, Stat *stat){
    return __fstat__(fd, stat);
}

int dup(int fd){
    return __dup__(fd, NO_FD);
}

int dup2(int fd1, int fd2){
    return __dup__(fd1, fd2);
}

int close(int fd){
    return __close__(fd);
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

int set_sigaction(int sig, Sigaction *sa){
    return __set_sigaction__(sig, sa);
}

int send_signal(int pid, int sig){
    return __send_signal__(pid, sig);
}

// The function should not be called explicitly
int sigreturn(void){
    return __sigreturn__();
}

int setpriority(int pid, int priority){
    return __setpriority__(pid, priority);
}

int getpriority(int pid){
    return __getpriority__(pid);
}

uint64_t get_proc_runticks(int pid){
    return __get_proc_runticks__(pid);
}

int set_proc_cpu(int pid, int cpuid){
    return __set_proc_cpu__(pid, cpuid);
}

int clear_proc_setcpu(int pid){
    return __clear_proc_setcpu__(pid);
}

int mkpipe(int *fd_store){
    return __mkpipe__(fd_store);
}

int mkfifo(char *name, uint32_t open_flags){
    return __mkfifo__(name, open_flags);
}

int seek(int fd, off_t pos, int whence){
    return __seek__(fd, pos, whence);
}

int fsync(int fd){
    return __fsync__(fd);
}

int chdir(char *path){
    return __chdir__(path);
}

int getcwd(char *buf, size_t len){
    return __getcwd__(buf, len);
}

int getdirentry(int fd, Dirent *dirent){
    return __getdirentry__(fd, dirent);
}

int link(char *oldpath, char *newpath){
    return __link__(oldpath, newpath);
}

int unlink(char *path){
    return __unlink__(path);
}

int mkdir(char *path){
    return __mkdir__(path);
}

int symlink(char *oldpath, char *newpath){
    return __symlink__(oldpath, newpath);
}

int socket(int family, int type, int protocol){
    return __socket__(family, type, protocol);
}

ssize_t sendto(int sockfd, void *buf, size_t len, int flags, Sockaddr *dest, socklen_t dest_len){
    return __sendto__(sockfd, buf, len, flags, dest, dest_len);
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, Sockaddr *dest, socklen_t *dest_len) {
    return __recvfrom__(sockfd, buf, len, flags, dest, dest_len);
}

int setsockopt(int sockfd, int level, int optname, char *optval, int optlen){
    return __setsockopt__(sockfd, level, optname, optval, optlen);
}

int closesocket(int sockfd){
    return __closesocket__(sockfd);
}

int connect(int sockfd, Sockaddr *addr, socklen_t len){
    return __connect__(sockfd, addr, len);
}

ssize_t send(int sockfd, void *buf, size_t len, int flags){
    return __send__(sockfd, buf, len, flags);
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags){
    return __recv__(sockfd, buf, len, flags);
}

int bind(int sockfd, Sockaddr *addr, socklen_t len){
    return __bind__(sockfd, addr, len);
}

int accept(int sockfd, Sockaddr *addr, socklen_t *len){
    return __accept__(sockfd, addr, len);
}

int listen(int sockfd, int backlog){
    return __listen__(sockfd, backlog);
}

int gethostbyname_r(char *name, Hostent *ret, char *buf, size_t buflen, Hostent **result, int *h_errnop){
    int sys_ret =  __gethostbyname_r__(name, ret, buf, buflen, result, h_errnop);
    if(sys_ret < 0) return sys_ret;
    Hostent_extra *extra = (Hostent_extra *)buf;
    strncpy(extra->name, name, buflen - sizeof(Hostent_extra));
    buf[buflen - 1] = '\0';
    ret->h_name = extra->name;
    ret->h_aliases = (char **)0;
    ret->h_addrtype = AF_INET;  // IPv4地址
    ret->h_length = 4;          // IPv4，4字节地址
    ret->h_addr_list = (char **)extra->addr_tbl;
    ret->h_addr_list[0] = (char *)&extra->addr;
    ret->h_addr_list[1] = (char *)0;
    *result = ret;
    return sys_ret;
}