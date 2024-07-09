#include "trap.h"
#include "assert.h"
#include "config.h"
#include "console.h"
#include "memlayout.h"
#include "plic.h"
#include "proc.h"
#include "riscv.h"
#include "stdio.h"
#include "syscall.h"
#include "uart.h"
#include "virtio_device.h"
#include "vmm.h"

Spinlock tickslock;
uint64_t ticks;
extern char trampoline[], uservec[], userret[];
extern pagetable_t* kernel_pagetable;

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

static int pagetable_handler(bool write) {
    extern Mm_struct* check_mm_struct;
    Mm_struct *mm;
    if (check_mm_struct != nullptr) { 
        assert(myproc() == nullptr);
        mm = check_mm_struct;
        
    }
    else{
        assert(myproc() != nullptr && myproc()->kernel_proc == 0);
        mm = myproc()->mm;   
    }
    return do_pagatable_fault(mm, r_stval(), write);
}

Trap_eum trap_work() {
    uint64_t scause = r_scause();
    int ret;
    if ((scause & 0x8000000000000000L) && (scause & 0xff) == 9) {
        int irq = plic_claim();
        if(!irq)
            return TRAP_INT;

        switch (irq) {
            case 1 ... VIRTIO_DEVICE_NUM: {
                virtio_device_intr_handler(irq);
            } break;
            case UART0_IRQ: {
#ifdef PRINT_UART_CHAR
                char c;
                c = uart_getc();
                cprintf(" \n serial num is:%03x : serial char is %c\n", c, c);
#endif
                uart_intr();
            } break;

            default: cprintf("unexpected interrupt irq=%d\n", irq); break;
        }
        plic_complete(irq);
        return TRAP_INT;
    } else if (scause == 0x8000000000000001L) {
        if (cpuid() == 0) {
            clockintr();
#ifdef PRINT_TRICKS
            print_ticks();
#endif
        }
        w_sip(r_sip() & ~2);
        return TRAP_SOFT_INT;
    } else if ((scause & 0x8000000000000000L) == 0) {
        int excep_code = scause & 0xff;
#ifdef PRINT_TRAP_EXCEPTION
        cprintf("excpetion : %s\n", exception_msg[excep_code]);
        cprintf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
        if (myproc() != nullptr)
            cprintf("trap_work: exception scause %p pid=%d\n", r_scause(), myproc()->pid);
        else
            cprintf("trap_work: exception scause %p\n", r_scause());
#endif
        if (excep_code == 13 || excep_code == 15) {
            if(excep_code == 13)
                ret = pagetable_handler(false);
            else if(excep_code == 15)
                ret = pagetable_handler(true);
        }
        if(ret != 0 ){
            if(myproc() == nullptr)
                panic("handle pgfault failed. %e\n", ret);
            else
                panic("user pid%d: handle pgfault failed. %e\n", myproc()->pid, ret);
        }
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
    if (intr_get() != 0) panic("kerneltrap: interrupts enabled");
    if ((trap_enum = trap_work()) == TRAP_OTHER) {
        cprintf("scause %p\n", scause);
        cprintf("sepc=%p stval=%p\n", sepc, r_stval());
        panic("kerneltrap");
    }
    if (trap_enum == TRAP_SOFT_INT && myproc() != 0 && myproc()->state == RUNNING) {
        do_yield();
    }

    // the yield() may have caused some traps to occur,
    // so restore trap registers for use by kernelvec.S's sepc instruction.
    w_sepc(sepc);
    w_sstatus(sstatus);
}

void usertrap(void) {
    Trap_eum trap_enum = TRAP_OTHER;
    if ((r_sstatus() & SSTATUS_SPP) != 0) panic("usertrap: not from user mode");
    w_stvec((uint64_t)kernelvec);
    Proc* p = myproc();
    p->trapframe->epc = r_sepc();
    if (intr_get() != 0) panic("usertrap: interrupts enabled");
    if (r_scause() == 8) {
        p->trapframe->epc += 4;
        intr_on();
        syscall();
    } else if ((trap_enum = trap_work()) == TRAP_OTHER) {
        cprintf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
        cprintf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    } else if (trap_enum == TRAP_SOFT_INT && myproc() != 0 && myproc()->state == RUNNING) {
        do_yield();
    }
    user_trap_ret();
}

void user_trap_ret() {
    Proc* p = myproc();
    intr_off();
    w_stvec(TRAMPOLINE + (uservec - trampoline));

    p->trapframe->kernel_satp = r_satp();
    p->trapframe->kernel_sp = p->kstack + STACK_SIZE;
    p->trapframe->kernel_trap = (uint64_t)usertrap;
    p->trapframe->kernel_hartid = r_tp();
    p->trapframe->tp = r_tp();

    uint64_t x = r_sstatus();
    x &= ~SSTATUS_SPP;
    // x |= SSTATUS_SPP;
    x |= SSTATUS_SPIE;
    if (r_scause() == 8 && p->trapframe->a7 == SYS_cli) x &= ~SSTATUS_SPIE;
    w_sstatus(x);
    w_sepc(p->trapframe->epc);

    uint64_t satp = MAKE_SATP(p->mm->pagetable);
    uint64_t fn = TRAMPOLINE + (userret - trampoline);
    ((void (*)(uint64_t, uint64_t))fn)(TRAPFRAME, satp);
    cprintf("can get here\n");
}