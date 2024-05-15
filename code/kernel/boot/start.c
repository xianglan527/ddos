#include "platform.h"
#include "uart.h"


__attribute__ ((aligned (16))) char stack0[4096 * MAXNUM_CPU];

void start_kernel(void) {
    uart_init();
    uart_puts("Hello, DDOS!\n");

    while (1) {};  // stop here!
}
