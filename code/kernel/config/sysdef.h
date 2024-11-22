#ifndef __CONFIG_SYSDEF_H__
#define __CONFIG_SYSDEF_H__

#define SYS_fork 1
#define SYS_exit 2
#define SYS_waitpid 3
#define SYS_mkpipe 4
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
#define SYS_sem_init 33
#define SYS_sem_post 34
#define SYS_sem_wait 35
#define SYS_sem_free 36
#define SYS_sem_get_value 37
#define SYS_event_send 38
#define SYS_event_recv 39
#define SYS_mbox_init 40
#define SYS_mbox_send 41
#define SYS_mbox_recv 42
#define SYS_mbox_free 43
#define SYS_mbox_info 44
#define SYS_set_sigaction 45
#define SYS_send_signal 46
#define SYS_sigreturn 47
#define SYS_setpriority 48
#define SYS_getpriority 49
#define SYS_get_proc_runticks 50
#define SYS_set_proc_cpu 51
#define SYS_clear_proc_setcpu 52
#define SYS_mkfifo 53
#define SYS_seek 54
#define SYS_fsync 55
#define SYS_getcwd 56
#define SYS_getdirentry 57
#define SYS_symlink 58

#define CLONE_VM 0x00000100
#define CLONE_THREAD 0x00000200
#define CLONE_SEM 0x00000400
#define CLONE_SIGACTION 0x00000800
#define CLONE_FS 0x000001000

#define MMAP_WRITE 0x00000100
#define MMAP_STACK 0x00000200

/* VFS flags */
// flags for open: choose one of these
#define O_RDONLY 0  // open for reading only
#define O_WRONLY 1  // open for writing only
#define O_RDWR 2    // open for reading and writing
// then or in any of these:
#define O_CREAT 0x00000004   // create file if it does not exist
#define O_EXCL 0x00000008    // error if O_CREAT and the file exists
#define O_TRUNC 0x00000010   // truncate file upon open
#define O_APPEND 0x00000020  // append on each write
#define O_NOFOLLOW 0x00000040   //
// additonal related definition
#define O_ACCMODE 3  // mask for O_RDONLY / O_WRONLY / O_RDWR

#define NO_FD -0x9527  // invalid fd

/* lseek codes */
#define LSEEK_SET 0  // seek relative to beginning of file
#define LSEEK_CUR 1  // seek relative to current position in file
#define LSEEK_END 2  // seek relative to end of file

// #define FS_MAX_DNAME_LEN 31
// #define FS_MAX_FNAME_LEN 255
// #define FS_MAX_FPATH_LEN 4095
#endif