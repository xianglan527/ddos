#include "assert.h"
#include "console.h"
#include "error.h"
#include "config.h"
#include "stdio.h"
#include "uart.h"
#include "riscv.h"
#include "memlayout.h"

void main();
void timerinit();

__attribute__((aligned(16))) char stack0[NCPU];

uint64_t timer_scratch[NCPU][5];

extern void timervec();

void start_kernel(void) {
    ulong x = r_mstatus();
    x &= ~MSTATUS_MPP_MASK;
    x |= MSTATUS_MPP_S;
    w_mstatus(x);

    w_mepc((uint64_t)main);

    w_satp(0);

    // uart_init();
    // uart_puts("Hello, DDOS!\n");
    // cputs("Hello, DDOS!");

    w_medeleg(0xffff);
    w_mideleg(0xffff);
    // intr_on();
    w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);

    timerinit();

    int id = r_mhartid();
    w_tp(id);
    asm volatile("mret");
}

void timerinit() {
    // each CPU has a separate source of timer interrupts.
    int id = r_mhartid();

    // ask the CLINT for a timer interrupt.
    int interval = TIME_INTERVAL; 
    *(uint64_t *)CLINT_MTIMECMP(id) = *(uint64_t *)CLINT_MTIME + interval;

    // prepare information in scratch[] for timervec.
    // scratch[0..2] : space for timervec to save registers.
    // scratch[3] : address of CLINT MTIMECMP register.
    // scratch[4] : desired interval (in cycles) between timer interrupts.
    uint64_t *scratch = &timer_scratch[id][0];
    scratch[3] = CLINT_MTIMECMP(id);
    scratch[4] = interval;
    w_mscratch((uint64_t)scratch);

    // set the machine-mode trap handler.
    w_mtvec((uint64_t)timervec);

    // enable machine-mode interrupts.
    w_mstatus(r_mstatus() | MSTATUS_MIE);

    // enable machine-mode timer interrupts.
    w_mie(r_mie() | MIE_MTIE);
}
