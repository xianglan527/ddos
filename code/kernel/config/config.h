#ifndef __CONFIG_CONFIG_H__
#define __CONFIG_CONFIG_H__

// #define PRINT_KERNEL_INFO
#define __PRINT_TRICKS 0
#define __PRINT_UART_CHAR 0
#define __PRINT_TRAP_EXCEPTION 0
#define __PRINT_VIRTIO_DEVICE_INFO 1
#define __PRINT_VIRTIO_DEVICE_TEST 0
#define __PRINT_MM_TEST 1

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

#if defined(PRINT_KERNEL_INFO) && (__PRINT_MM_TEST)
#define PRINT_MM_TEST
#endif

#define CONSOLE_BUF_SIZE 1024
#define NPROC 2000
#define NCPU 8
#define STACK_SIZE 4096
#define TIME_INTERVAL 10000
#define SLAB_ALIGN 16
#define RB_MIN_MAP_COUNT 32  // If the count of vma >32 then redblack tree link is used
#define DELAY 1000
#define MAXARG  32
#define MAXPATH 128
#define CLEAN_PROC_EXIT_NUM 32

#define lock_info_lens 100
#define lock_info_nums 16000
#define lock_info_nest 25

#define MAX_TIME_SLICE 200;
#define MIN_TIME_SLICE 5

#define MAX_MSG_SLOTS 0x1000
#define MAX_MSG_BYTES 0x10000

#endif  // __CONFIG_CONFIG_H__
