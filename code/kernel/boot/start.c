#include "assert.h"
#include "clint.h"
#include "config.h"
#include "error.h"
#include "memlayout.h"
#include "riscv.h"
#include "stdio.h"
#include "uart.h"

void main();
void timerinit();
void m_mode_init();

__attribute__((aligned(16))) char stack0[STACK_SIZE * NCPU];

uint64_t timer_scratch[NCPU][5];

extern void timervec();
extern void m_mode_vec();

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
    w_mstatus(r_mstatus() | MSTATUS_MIE);
    timerinit();
    // m_mode_init();
    int id = r_mhartid();
    w_tp(id);
    asm volatile("mret");
}

void timerinit() {
    // each CPU has a separate source of timer interrupts.
    int id = r_mhartid();

    // ask the CLINT for a timer interrupt.
    int interval = TIME_INTERVAL;
    // *(uint64_t *)CLINT_MTIMECMP(id) = *(uint64_t *)CLINT_MTIME + interval;
    clint_set_mtimecmp(clint_get_mtime() + interval, id);
    // clint_add_mtimecmp(interval, id);

    // prepare information in scratch[] for timervec.
    // scratch[0..2] : space for timervec to save registers.
    // scratch[3] : address of CLINT MTIMECMP register.
    // scratch[4] : desired interval (in cycles) between timer interrupts.
    uint64_t *scratch = &timer_scratch[id][0];
    // scratch[3] = CLINT_MTIMECMP(id);
    scratch[3] = (uint64_t)clint_get_mtimecmp_ptr(id);
    scratch[4] = interval;
    w_mscratch((uint64_t)scratch);

    // set the machine-mode trap handler.
    w_mtvec((uint64_t)timervec);

    // enable machine-mode interrupts.
    w_mstatus(r_mstatus() | MSTATUS_MIE);

    // enable machine-mode timer interrupts.
    w_mie(r_mie() | MIE_MTIE);
}

void m_mode_init() {
    int id = r_mhartid();
    w_mtvec((uint64_t)m_mode_vec);
    // enable machine-mode interrupts.
    w_mstatus(r_mstatus() | MSTATUS_MIE);
    // enable machine-mode timer & soft interrupts.
    // w_mie(r_mie() | MIE_MTIE | MIE_MSIE);
    if (id == 0) {
        w_mie(r_mie() | MIE_MTIE | MIE_MSIE);
    } else {
        w_mie(r_mie() | MIE_MSIE);
    }
    // w_mie(r_mie() | MIE_MTIE);
    clint_set_mtimecmp(clint_get_mtime() + TIME_INTERVAL, id);
}
