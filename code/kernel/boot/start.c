#include "assert.h"
#include "console.h"
#include "error.h"
#include "config.h"
#include "stdio.h"
#include "uart.h"
#include "risv.h"

void main();

__attribute__((aligned(16))) char stack0[4096 * MAXNUM_CPU];

void start_kernel(void) {
    ulong x = r_mstatus();
    x &= ~MSTATUS_MPP_MASK;
    x |= MSTATUS_MPP_S;
    w_mstatus(x);

    w_mepc((uint64_t)main);

    w_satp(0);

    uart_init();
    // uart_puts("Hello, DDOS!\n");
    cputs("Hello, DDOS!");

    w_medeleg(0xffff);
    w_mideleg(0xffff);
    // intr_on();
    w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);

    int id = r_mhartid();
    w_tp(id);
    asm volatile("mret");
}
