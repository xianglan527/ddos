#include "platform.h"
#include "stdio.h"
#include "uart.h"

__attribute__((aligned(16))) char stack0[4096 * MAXNUM_CPU];

void test(void) {
    int version = 20240516;
    char *hello = "Hello, qemu and risc-v!";
    cprintf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
    cprintf("  version is: %d\n", version);
    cprintf("  version is: 0x%08x\n", version);
    cprintf("  pointer is: %p\n", &version);
    cprintf("  %s\n", hello);
    cprintf("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
}

void start_kernel(void) {
    uart_init();
    // uart_puts("Hello, DDOS!\n");
    cputs("Hello, DDOS!");
    test();
    while (1) {};  // stop here!
}
