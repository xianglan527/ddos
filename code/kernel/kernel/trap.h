#ifndef __KERNEL_TRAP_H__
#define __KERNEL_TRAP_H__
#include "stdarg.h"
#include "types.h"

typedef enum trap_enum Trap_eum;

enum trap_enum{
    TRAP_INT, 
    TRAP_SOFT_INT,
    TRAP_EXCEPTION,
    TRAP_OTHER,
};

typedef enum {
    EXC_INST_ADDR_MISALIGNED = 0,
    EXC_INST_ACCESS_FAULT,
    EXC_ILLEGAL_INST,
    EXC_BREAKPOINT,
    EXC_LOAD_ADDR_MISALIGNED,
    EXC_LOAD_ACCESS_FAULT,
    EXC_STORE_AMO_ADDR_MISALIGNED,
    EXC_STORE_AMO_ACCESS_FAULT,
    EXC_ECALL_UMODE,
    EXC_ECALL_SMODE,
    EXC_RESERVED_10,
    EXC_ECALL_MMODE,
    EXC_INST_PAGE_FAULT,
    EXC_LOAD_PAGE_FAULT,
    EXC_RESERVED_14,
    EXC_STORE_AMO_PAGE_FAULT,
    EXC_MAX,
} riscv_exception_t;

static const char* exception_msg[EXC_MAX] = {
    [EXC_INST_ADDR_MISALIGNED] = "0: Instruction address misaligned",
    [EXC_INST_ACCESS_FAULT] = "1: Instruction access fault",
    [EXC_ILLEGAL_INST] = "2: Illegal instruction",
    [EXC_BREAKPOINT] = "3: Breakpoint",
    [EXC_LOAD_ADDR_MISALIGNED] = "4: Load address misaligned",
    [EXC_LOAD_ACCESS_FAULT] = "5: Load access fault",
    [EXC_STORE_AMO_ADDR_MISALIGNED] = "6: Store/AMO address misaligned",
    [EXC_STORE_AMO_ACCESS_FAULT] = "7: Store/AMO access fault",
    [EXC_ECALL_UMODE] = "8: Environment call from U-mode",
    [EXC_ECALL_SMODE] = "9: Environment call from S-mode",
    [EXC_RESERVED_10] = "10: Reserved",
    [EXC_ECALL_MMODE] = "11: Environment call from M-mode",
    [EXC_INST_PAGE_FAULT] = "12: Instruction page fault",
    [EXC_LOAD_PAGE_FAULT] = "13: Load page fault",
    [EXC_RESERVED_14] = "14: Reserved for future standard use",
    [EXC_STORE_AMO_PAGE_FAULT] = "15: Store/AMO page fault",
};

typedef enum {
    IPI_RESCHEDULE = 0,   /* Request the target CPU to reschedule */
    IPI_CALL_FUNCTION,    /* Execute a function on the target CPU */
    IPI_CPU_STOP,         /* Request the target CPU to stop (hotplug) */
    IPI_CPU_CRASH_STOP,   /* Force-stop other CPUs on panic */
    IPI_IRQ_WORK,         /* Trigger irq_work queue on the target CPU */
    IPI_REMOTE_TLB_FLUSH, /* Remote TLB flush (SFENCE.VMA) */
    IPI_REMOTE_FENCE_I,   /* Remote instruction cache flush (FENCE.I) */
    IPI_TIMER,            /* Timer tick soft interrupt */
    NR_RISCV_IPI          /* Total number of IPI types */
} ipi_message_type_t;

static const char* ipi_msg[NR_RISCV_IPI] = {
    [IPI_RESCHEDULE] = "0: IPI_RESCHEDULE - Request the target CPU to reschedule",
    [IPI_CALL_FUNCTION] = "1: IPI_CALL_FUNCTION - Execute a function on the target CPU",
    [IPI_CPU_STOP] = "2: IPI_CPU_STOP - Request the target CPU to stop (hotplug)",
    [IPI_CPU_CRASH_STOP] = "3: IPI_CPU_CRASH_STOP - Force-stop other CPUs on panic",
    [IPI_IRQ_WORK] = "4: IPI_IRQ_WORK - Trigger irq_work queue on the target CPU",
    [IPI_REMOTE_TLB_FLUSH] = "5: IPI_REMOTE_TLB_FLUSH - Remote TLB flush (SFENCE.VMA)",
    [IPI_REMOTE_FENCE_I] = "6: IPI_REMOTE_FENCE_I - Remote instruction cache flush (FENCE.I)",
    [IPI_TIMER] = "7: IPI_TIMER - Timer tick soft interrupt",
};

void trap_init_hart(void);
void kerneltrap();
void user_trap_ret();
void trap_tick_init(void);
void usertrap(void);
#endif