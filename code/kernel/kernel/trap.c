#include "trap.h"

#include "assert.h"
#include "config.h"
#include "error.h"
#include "memlayout.h"
#include "plic.h"
#include "proc.h"
#include "riscv.h"
#include "stdio.h"
#include "syscall.h"
#include "uart.h"
#include "virtio_device.h"
#include "vmm.h"
#include "sched.h"
#include "signal.h"
#include "clint.h"

Spinlock tickslock;
uint64_t ticks;
extern char trampoline[], uservec[], userret[];
extern pagetable_t* kernel_pagetable;

extern Spinlock print_struct_lock;

void kernelvec();

void trap_init_hart(void) { w_stvec((uint64_t)kernelvec); }

void trap_tick_init(void) { initlock(&tickslock, "time"); }

void clockintr() {
    acquire(&tickslock);
    ticks++;
    release(&tickslock);
}

static void print_ticks() { cprintf("ticks is :%d\n", ticks); }

static int pagetable_handler(bool write) {
    extern Mm_struct* check_mm_struct;
    Mm_struct* mm;
    if (check_mm_struct != nullptr) {
        assert(myproc() == nullptr);
        mm = check_mm_struct;
    } else {
        if (myproc() == nullptr) {
            cprintf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
            proc_dump();
        }
        assert(myproc() != nullptr);
        mm = myproc()->mm;
    }
    return do_pagatable_fault(mm, r_stval(), write);
}

Trap_eum trap_work() {
    uint64_t scause = r_scause();
    int ret = 0;
    bool has_int = scause & 0x8000000000000000L;
    if (has_int) {
        if ((scause & 0xff) == IRQ_S_EXT) {
            int irq = plic_claim();
            if (!irq) return TRAP_INT;
            switch (irq) {
                case 1 ... VIRTIO_DEVICE_NUM: {
                    virtio_device_intr_handler(irq);
                } break;
                case UART0_IRQ: {
                    char c;
                    c = uart_getc();
#ifdef PRINT_UART_CHAR
                    cprintf(" \n serial num is:%03x : serial char is %c\n", c, c);
#endif
                    // uart_intr(c);
                    extern void dev_stdin_write(char c);
                    dev_stdin_write(c);
                } break;

                default: cprintf("unexpected interrupt irq=%d\n", irq); break;
            }
            plic_complete(irq);
            return TRAP_INT;
        } else if ((scause & 0xff) == IRQ_S_SOFT) {
            if (cpuid() == 0) {
                clockintr();
                run_timer_list();
#ifdef PRINT_TRICKS
                print_ticks();
#endif
            }
            clint_set_ssip(1, cpuid());
            w_sip(r_sip() & ~SIP_SSIP);
            return TRAP_SOFT_INT;
        } else  {
            return TRAP_OTHER;
        }
    } else {
        int excep_code = scause & 0xff;
#ifdef PRINT_TRAP_EXCEPTION
        cprintf("excpetion : %s\n", exception_msg[excep_code]);
        cprintf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
        if (myproc() != nullptr)
            cprintf("trap_work: exception scause %p pid=%d\n", r_scause(), myproc()->pid);
        else
            cprintf("trap_work: exception scause %p\n", r_scause());
#endif
        if (excep_code == EXC_INST_PAGE_FAULT || excep_code == EXC_LOAD_PAGE_FAULT ||
            excep_code == EXC_STORE_AMO_PAGE_FAULT) {
            if (excep_code == EXC_INST_PAGE_FAULT || excep_code == EXC_LOAD_PAGE_FAULT)
                ret = pagetable_handler(false);
            else if (excep_code == EXC_STORE_AMO_PAGE_FAULT)
                ret = pagetable_handler(true);
            if (ret != 0) {
                acquire(&print_struct_lock);
                // proc_mm_dump();
                proc_dump();
                // dump_proc_mm_list();
                cprintf("  excep_code %d  sepc=%p stval=%p\n", excep_code, r_sepc(), r_stval());
                // release(&print_struct_lock);
                if (myproc() == nullptr)
                    cprintf("handle pgfault failed. %e\n", -ret);
                else
                    cprintf("user pid%d: handle pgfault failed. %e\n", myproc()->pid, -ret);
                release(&print_struct_lock);
                panic("pagetable_handler error");
            }
        } else {
            acquire(&print_struct_lock);
            // proc_mm_dump();
            proc_dump();
            // dump_proc_mm_list();
            cprintf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
            // release(&print_struct_lock);
            if (myproc() == nullptr) {
                cprintf("no hangdle the exception function excep_code %d cpuid is:%d", excep_code, cpuid());
            } else {
                cprintf("no hangdle the exception function pid : %d excep_code %d cpuid is:%d", myproc()->pid,
                        excep_code, cpuid());
            }
            release(&print_struct_lock);
            panic("no hangdle the exception function");
        }
        return TRAP_EXCEPTION;
    }
    return TRAP_OTHER;
}

void kerneltrap() {
    Trap_eum trap_enum;
    uint64_t sepc = r_sepc();
    uint64_t sstatus = r_sstatus();
    uint64_t scause = r_scause();
    Proc *current = nullptr;
    uint64_t sp = 0;
    uintptr_t stackTop = 0;
    // if(myproc() != nullptr){
    //     current = myproc();
    //     sp = r_sp();
    //     stackTop = current->kstack + KSTACKSIZE - PGSIZE;
    //     if(stackTop - sp > PGSIZE){
    //         panic("too much nest");
    //     }
    // }
    if ((sstatus & SSTATUS_SPP) == 0) panic("kerneltrap: not from supervisor mode");
    if (intr_get() != 0) panic("kerneltrap: interrupts enabled");
    if ((trap_enum = trap_work()) == TRAP_OTHER) {
        cprintf("scause %p\n", scause);
        cprintf("sepc=%p stval=%p\n", sepc, r_stval());
        panic("kerneltrap");
    }
    // Switching tasks within a kernel interrupt can lead to excessive nesting, meaning that continuously
    // entering kernel interrupts will keep consuming stack space. To prevent this, a simple solution is to
    // disable task switching in this context. This might not be a perfect fix, but current test results
    // indicate that it effectively resolves the bug
    if (trap_enum == TRAP_SOFT_INT && myproc() != 0 && myproc()->state == RUNNING) {
        sched_class_proc_tick(myproc());
        if (myproc()->need_resched == true /*&& myproc()->kernel_proc == true*/) { 
            do_yield(); 
        }
        if(!list_empty(&myproc()->siginfo_list)){
            do_signal();
        }
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
        // may_killed();
    } else if ((trap_enum = trap_work()) == TRAP_OTHER) {
        p->flags |= PF_EXITING;
        p->exit_code = E_KILLED;
        cprintf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
        cprintf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    } else if (trap_enum == TRAP_SOFT_INT && myproc() != 0 && myproc()->state == RUNNING) {
        sched_class_proc_tick(myproc());
        if(myproc()->need_resched == true){
            do_yield();
        }
        if(!list_empty(&myproc()->siginfo_list)){
            do_signal();
        }
    }
    may_killed();
    user_trap_ret();
}

void user_trap_ret() {
    Proc* p = myproc();
    intr_off();
    w_stvec(TRAMPOLINE + (uservec - trampoline));

    p->trapframe->kernel_satp = r_satp();
    p->trapframe->kernel_sp = p->kstack + KSTACKSIZE - PGSIZE;
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
    ((void (*)(uint64_t, uint64_t))fn)(TRAPFRAME(p->mm_index), satp);
}

uint64_t g_sys_tick = 0;
extern Atomic cpus_num;
#define M_MODE_INFO 1
uint64_t handle_trap(uint64_t mcause, uint64_t mepc){
    bool has_int = mcause & 0x8000000000000000L;
    mcause = mcause & 0xFF;
    if (has_int) {
        switch (mcause) {
            case IRQ_M_SOFT:           // soft
                if (atomic_read(&cpus_num) == CPUS && M_MODE_INFO) {
                    cprintf("cpu %lu soft isr: mcause %d\n", r_mhartid(), mcause);
                    clint_set_ssip(1, 1);
                }
                clint_set_msip(0, r_mhartid());  // clear soft isr
                break;
            case IRQ_M_TIMER:  // mtime
                // clint_add_mtimecmp(TIME_INTERVAL, r_mhartid());
                clint_set_mtimecmp(clint_get_mtime() + TIME_INTERVAL, r_mhartid());
                ++g_sys_tick;
                // test only
                if ((g_sys_tick & 3) == 0) clint_set_msip(1, r_mhartid());
                // if ((g_sys_tick & 3) == 0) clint_set_msip(1, 1);
                // debug only
                if (atomic_read(&cpus_num) == CPUS && M_MODE_INFO) {
                    cprintf("cpu %lu mtime: %d\n", r_mhartid(), g_sys_tick);
                    clint_set_ssip(1, r_mhartid());
                }
                // if (atomic_read(&cpus_num) == CPUS){clint_set_ssip(1, r_mhartid());}
                // clint_set_ssip(1, r_mhartid());
                break;

            default:
                if (atomic_read(&cpus_num) == CPUS && M_MODE_INFO) { cprintf("unknow isr"); }
                break;
        }
    } else {
        if (atomic_read(&cpus_num) == CPUS && M_MODE_INFO) {
            cprintf("exception:\n");
            cprintf("mcause: 0x%016x\n", mcause);
            cprintf("mepc: 0x%016x\n", mepc);
            cprintf("mtval: 0x%016x\n", r_stval());
        }
    }
    return  mepc;
}