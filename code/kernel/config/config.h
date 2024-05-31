#ifndef __CONFIG_CONFIG_H__
#define __CONFIG_CONFIG_H__

// #define PRINT_KERNEL_INFO
#define __PRINT_TRICKS 0
#define __PRINT_UART_CHAR 0
#define __PRINT_TRAP_EXCEPTION 1
#define KERNEL_TEST 1

#if defined(PRINT_KERNEL_INFO) && (__PRINT_TRICKS)
#define PRINT_TRICKS
#endif

#if defined(PRINT_KERNEL_INFO) && (__PRINT_UART_CHAR)
#define PRINT_UART_CHAR
#endif

#if defined(PRINT_KERNEL_INFO) && (__PRINT_TRAP_EXCEPTION)
#define PRINT_TRAP_EXCEPTION
#endif

#define CONSOLE_BUF_SIZE 1024
#define NPROC 64
#define NCPU 8
#define STACK_SIZE 4096
#define TIME_INTERVAL 1000000
#define DELAY 1000

#endif  // __CONFIG_CONFIG_H__
