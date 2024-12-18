#ifndef __CONFIG_CONFIG_H__
#define __CONFIG_CONFIG_H__

// #define PRINT_KERNEL_INFO
#define __PRINT_TRICKS 0
#define __PRINT_UART_CHAR 0
#define __PRINT_TRAP_EXCEPTION 0
#define __PRINT_VIRTIO_DEVICE_INFO 1
#define __PRINT_VIRTIO_DEVICE_TEST 0
#define __PRINT_MM_TEST 1
#define __PRINT_NET_TEST 1

#if defined(PRINT_KERNEL_INFO) && (__PRINT_TRICKS)
#define PRINT_TRICKS
#endif

#if defined(PRINT_KERNEL_INFO) && (__PRINT_UART_CHAR)
#define PRINT_UART_CHAR
#endif

#if defined(PRINT_KERNEL_INFO) && (__PRINT_TRAP_EXCEPTION)
#define PRINT_TRAP_EXCEPTION
#endif

#if defined(PRINT_KERNEL_INFO) && (__PRINT_VIRTIO_DEVICE_INFO)
#define PRINT_VIRTIO_DEVICE_INFO
#endif

#if defined(PRINT_KERNEL_INFO) && (__PRINT_VIRTIO_DEVICE_TEST)
#define PRINT_VIRTIO_DEVICE_TEST
#endif

#if defined(PRINT_KERNEL_INFO) && (__PRINT_NET_TEST)
#define PRINT_NET_TEST
#endif

#if defined(PRINT_KERNEL_INFO) && (__PRINT_MM_TEST)
#define PRINT_MM_TEST
#endif

#define NPROC 2000
#define NCPU 8
#define STACK_SIZE 4096
#define TIME_INTERVAL 10000
#define SLAB_ALIGN 16
#define RB_MIN_MAP_COUNT 32  // If the count of vma >32 then redblack tree link is used
#define DELAY 1000
#define MAXARG  32
// #define MAXPATH 128
#define CLEAN_PROC_EXIT_NUM 32

#define lock_info_lens 100
// #define lock_info_nums 16000
// #define lock_info_nest 25

#define MAX_TIME_SLICE 200;
#define MIN_TIME_SLICE 5

#define MAX_MSG_SLOTS 0x1000
#define MAX_MSG_BYTES 0x10000

#define CPU_LOAD_IDX_MAX 5

#define load_balance_proc_num_threshold 10
#define load_balance_diff_threshold 4

#define MAX_LOCK_NODE 1000000

#define MAX_INODE_COUNT 0x10000

#define FS_MAX_DNAME_LEN 31
#define FS_MAX_FNAME_LEN 255
// #define FS_MAX_FPATH_LEN 4095
#define MAXPATH 4096

#define STDIN_BUFSIZE 4096

#define FS_STRUCT_TOTAL_SIZE (4 * 4096)

#define PIPE_STATE_TOTAL_SIZE (4 * 4096)


//fs conifg
#define SFS_MAGIC   0x20230822
#define SFS_BSIZE 4096
// #define SFS_MAX_FNAME_LEN FS_MAX_FNAME_LEN
#define SFS_NDIRECT 11
#define SFS_NINODES 1000
#define SFS_DISKSIZE  (1024 * 1024 * 1024) //1024MB

#define SFS_MAXOPBLOCKS 30        //max #of blocks any FS op writes
#define SFS_LOGSIZE (SFS_MAXOPBLOCKS * 10)  // max data blocks in on-disk log
#define SFS_NBUF (SFS_MAXOPBLOCKS * 10)     // size of disk block cache

#define SFS_PWD_LEN 4096

#define SFS_MAX_SYMLINK_CYCLE 10
#endif  // __CONFIG_CONFIG_H__
