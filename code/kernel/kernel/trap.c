#include "trap.h"
#include "syscall.h"
#include "assert.h"
#include "config.h"
#include "console.h"
#include "memlayout.h"
#include "plic.h"
#include "proc.h"
#include "riscv.h"
#include "stdio.h"
#include "uart.h"

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

Spinlock tickslock;
uint64_t ticks;
extern char trampoline[], uservec[], userret[];

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

void trap_tick_init(void) { initlock(&tickslock, "time"); }

void clockintr() {
    acquire(&tickslock);
    ticks++;
    release(&tickslock);
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
        if (cpuid() == 0) clockintr();
#ifdef PRINT_KERNEL_INFO
// print_ticks();
#endif
        w_sip(r_sip() & ~2);
        return TRAP_SOFT_INT;
    } else if ((scause & 0x8000000000000000L) == 0) {
        int excep_code = scause & 0xff;
#ifdef PRINT_KERNEL_INFO
        cprintf("excpetion : %s\n", exception_msg[excep_code]);
        cprintf("trap_work: unexpected scause %p pid=%d\n", r_scause(), myproc()->pid);
        cprintf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
#endif
        uint64_t epc = r_sepc();
        epc += 4;
        w_sepc(epc);
        return TRAP_EXCEPTION;
    } else
        return TRAP_OTHER;
}

void kerneltrap() {
    Trap_eum trap_enum;
    uint64_t sepc = r_sepc();
    uint64_t sstatus = r_sstatus();
    uint64_t scause = r_scause();
    if ((sstatus & SSTATUS_SPP) == 0) panic("kerneltrap: not from supervisor mode");
    // int bool = intr_get();
    if (intr_get() != 0) panic("kerneltrap: interrupts enabled");
    if ((trap_enum = trap_work()) == TRAP_OTHER) {
        cprintf("scause %p\n", scause);
        cprintf("sepc=%p stval=%p\n", sepc, r_stval());
        panic("kerneltrap");
    }
    if (trap_enum == TRAP_SOFT_INT && myproc() != 0 && myproc()->state == RUNNING) {
        // intr_on();  // Since interrupts are disabled when entering the trap, enable them again here.
        yield();
    }

    // the yield() may have caused some traps to occur,
    // so restore trap registers for use by kernelvec.S's sepc instruction.
    w_sepc(sepc);
    w_sstatus(sstatus);
    // intr_on();
}

void usertrap(void) {
    Trap_eum trap_enum;
    if ((r_sstatus() & SSTATUS_SPP) != 0) panic("usertrap: not from user mode");
    w_stvec((uint64_t)kernelvec);
    Proc* p = myproc();
    p->trapframe->epc = r_sepc();
    if (intr_get() != 0) panic("usertrap: interrupts enabled");
    if(r_scause() == 8){
        p->trapframe->epc += 4;
        intr_on();
        syscall();
    }
    if ((trap_enum = trap_work()) == TRAP_OTHER) {
        cprintf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
        cprintf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    }
    if (trap_enum == TRAP_SOFT_INT && myproc() != 0 && myproc()->state == RUNNING) { yield(); }
    user_trap_ret();
}

void user_trap_ret() {
    Proc* p = myproc();
    intr_off();
    w_stvec((uint64_t)uservec);

    p->trapframe->kernel_satp = 0;  // not used now
    p->trapframe->kernel_sp = p->kstack + STACK_SIZE;
    p->trapframe->kernel_trap = (uint64_t)usertrap;
    p->trapframe->kernel_hartid = r_tp();

    uint64_t x = r_sstatus();
    x &= ~SSTATUS_SPP;
    x |= SSTATUS_SPIE;
    w_sstatus(x);

    w_sepc(p->trapframe->epc);

    uint64_t fn = (uint64_t)userret;
    ((void (*)(uint64_t, uint16_t))fn)((uint64_t)p->trapframe, 0);
}