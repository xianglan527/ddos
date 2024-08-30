#ifndef __CONFIG_SYSDEF_H__
#define __CONFIG_SYSDEF_H__

#define SYS_fork 1
#define SYS_exit 2
#define SYS_waitpid 3
#define SYS_pipe 4
#define SYS_read 5
#define SYS_kill 6
#define SYS_exec 7
#define SYS_fstat 8
#define SYS_chdir 9
#define SYS_dup 10
#define SYS_getpid 11
#define SYS_sbrk 12
#define SYS_sleep 13
#define SYS_uptime 14
#define SYS_open 15
#define SYS_write 16
#define SYS_mknod 17
#define SYS_unlink 18
#define SYS_link 19
#define SYS_mkdir 20
#define SYS_close 21
#define SYS_sti 22
#define SYS_cli 23
#define SYS_yield 24
#define SYS_gettime 25
#define SYS_get_free_page_size 26
#define SYS_get_slab_allocated_size 27
#define SYS_clone 28
#define SYS_exit_thread 29
#define SYS_mmap 30
#define SYS_munmap 31
#define SYS_shmem 32

#define CLONE_VM 0x00000100
#define CLONE_THREAD 0x00000200

#define MMAP_WRITE 0x00000100
#define MMAP_STACK 0x00000200
#endif