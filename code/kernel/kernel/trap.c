#include "trap.h"
#include "proc.h"
#include "assert.h"
#include "console.h"
#include "memlayout.h"
#include "plic.h"
#include "riscv.h"
#include "stdio.h"
#include "uart.h"
#include "config.h"


// 0: Instruction address misaligned
// 1: Instruction access fault
// 2: Illegal instruction
// 3: Breakpoint
// 4: Load address misaligned
// 5: Load access fault
// 6: Store/AMO address misaligned
// 7: Store/AMO access fault
// 8: Environment call from U-mode
// 9: Environment call from S-mode
// 10: Reserved
// 11: Environment call from M-mode
// 12: Instruction page fault
// 13: Load page fault
// 14: Reserved for future standard use
// 15: Store/AMO page fault

uint64_t ticks;

void kernelvec();

void trap_init_hart(void) { w_stvec((uint64_t)kernelvec); }

static const char* exception_msg[] = {
    "0: Instruction address misaligned",
    "1: Instruction access fault",
    "2: Illegal instruction",
    "3: Breakpoint",
    "4: Load address misaligned",
    "5: Load access fault",
    "6: Store/AMO address misaligned",
    "7: Store/AMO access fault",
    "8: Environment call from U-mode",
    "9: Environment call from S-mode",
    "10: Reserved",
    "11: Environment call from M-mode",
    "12: Instruction page fault",
    "13: Load page fault",
    "14: Reserved for future standard use",
    "15: Store/AMO page fault",
};

void clockintr() {
    ticks++;
}

static void print_ticks() { cprintf("ticks is :%d\n", ticks); }

Trap_eum trap_work() {
    uint64_t scause = r_scause();

    if ((scause & 0x8000000000000000L) && (scause & 0xff) == 9) {
        int irq = plic_claim();

        if (irq == UART0_IRQ) {
        #ifdef PRINT_KERNEL_INFO
            // char c;
            // c = uart_getc();
            // cprintf(" \n serial num is:%03x : serial char is %c\n", c, c);
        #endif
            uart_intr();
        } else if (irq) {
            cprintf("unexpected interrupt irq=%d\n", irq);
        }
        if (irq) plic_complete(irq);
        return TRAP_INT;
    } else if (scause == 0x8000000000000001L) {
        if (cpuid() == 0)  clockintr();
        #ifdef PRINT_KERNEL_INFO
        // print_ticks();
        #endif
        w_sip(r_sip() & ~2);
        return TRAP_SOFT_INT;
    }
    else if ((scause & 0x8000000000000000L) == 0){
        int excep_code = scause & 0xff;
        #ifdef PRINT_KERNEL_INFO
            cprintf("excpetion : %s\n", exception_msg[excep_code]);
        #endif
        uint64_t epc= r_sepc();
        epc += 4;
        w_sepc(epc);
        return TRAP_EXCEPTION;
    }
    else
        return TRAP_OTHER;
}

void kerneltrap() {
    Trap_eum trap_enum;
    uint64_t sepc = r_sepc();
    uint64_t sstatus = r_sstatus();
    uint64_t scause = r_scause();
    if ((sstatus & SSTATUS_SPP) == 0) panic("kerneltrap: not from supervisor mode");
    int bool = intr_get();
    if (intr_get() != 0) panic("kerneltrap: interrupts enabled");
    if ((trap_enum = trap_work()) == TRAP_OTHER) {
        cprintf("scause %p\n", scause);
        cprintf("sepc=%p stval=%p\n", sepc, r_stval());
        panic("kerneltrap");
    }
    if(trap_enum == TRAP_SOFT_INT && myproc() != 0 && myproc()->state == RUNNING){
        intr_on();  // Since interrupts are disabled when entering the trap, enable them again here.
        yield();
    }
        
    // the yield() may have caused some traps to occur,
    // so restore trap registers for use by kernelvec.S's sepc instruction.
    w_sepc(sepc);
    w_sstatus(sstatus);
    // intr_on();
}