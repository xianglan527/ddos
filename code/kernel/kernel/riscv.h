#ifndef __KERNEL_RISCV_H__
#define __KERNEL_RISCV_H__
#include "types.h"
// #include "proc.h"
// #include "stdio.h"

typedef struct trapframe Trapframe;
struct trapframe {
    /*   0 */ uint64_t kernel_satp;    // kernel page table
    /*   8 */ uint64_t kernel_sp;      // top of process's kernel stack
    /*  16 */ uint64_t kernel_trap;    // usertrap()
    /*  24 */ uint64_t epc;            // saved user program counter
    /*  32 */ uint64_t kernel_hartid;  // saved kernel tp
    /*  40 */ uint64_t ra;
    /*  48 */ uint64_t sp;
    /*  56 */ uint64_t gp;
    /*  64 */ uint64_t tp;
    /*  72 */ uint64_t t0;
    /*  80 */ uint64_t t1;
    /*  88 */ uint64_t t2;
    /*  96 */ uint64_t s0;
    /* 104 */ uint64_t s1;
    /* 112 */ uint64_t a0;
    /* 120 */ uint64_t a1;
    /* 128 */ uint64_t a2;
    /* 136 */ uint64_t a3;
    /* 144 */ uint64_t a4;
    /* 152 */ uint64_t a5;
    /* 160 */ uint64_t a6;
    /* 168 */ uint64_t a7;
    /* 176 */ uint64_t s2;
    /* 184 */ uint64_t s3;
    /* 192 */ uint64_t s4;
    /* 200 */ uint64_t s5;
    /* 208 */ uint64_t s6;
    /* 216 */ uint64_t s7;
    /* 224 */ uint64_t s8;
    /* 232 */ uint64_t s9;
    /* 240 */ uint64_t s10;
    /* 248 */ uint64_t s11;
    /* 256 */ uint64_t t3;
    /* 264 */ uint64_t t4;
    /* 272 */ uint64_t t5;
    /* 280 */ uint64_t t6;
};

static inline uint64_t r_mhartid() {
    uint64_t x;
    asm volatile("csrr %0, mhartid" : "=r"(x));
    return x;
}

// Machine Status Register, mstatus

#define MSTATUS_MPP_MASK (3L << 11)  // previous mode.
#define MSTATUS_MPP_M (3L << 11)
#define MSTATUS_MPP_S (1L << 11)
#define MSTATUS_MPP_U (0L << 11)
#define MSTATUS_MIE (1L << 3)  // machine-mode interrupt enable.

static inline uint64_t r_mstatus() {
    uint64_t x;
    asm volatile("csrr %0, mstatus" : "=r"(x));
    return x;
}

static inline void w_mstatus(uint64_t x) { asm volatile("csrw mstatus, %0" : : "r"(x)); }

// machine exception program counter, holds the
// instruction address to which a return from
// exception will go.
static inline void w_mepc(uint64_t x) { asm volatile("csrw mepc, %0" : : "r"(x)); }

// Supervisor Status Register, sstatus

#define SSTATUS_SPP (1L << 8)   // Previous mode, 1=Supervisor, 0=User
#define SSTATUS_SPIE (1L << 5)  // Supervisor Previous Interrupt Enable
#define SSTATUS_UPIE (1L << 4)  // User Previous Interrupt Enable
#define SSTATUS_SIE (1L << 1)   // Supervisor Interrupt Enable
#define SSTATUS_UIE (1L << 0)   // User Interrupt Enable

static inline uint64_t r_sstatus() {
    uint64_t x;
    asm volatile("csrr %0, sstatus" : "=r"(x));
    return x;
}

static inline void w_sstatus(uint64_t x) { asm volatile("csrw sstatus, %0" : : "r"(x)); }


// Supervisor Interrupt Pending
static inline uint64_t r_sip() {
    uint64_t x;
    asm volatile("csrr %0, sip" : "=r"(x));
    return x;
}

static inline void w_sip(uint64_t x) { asm volatile("csrw sip, %0" : : "r"(x)); }

// Supervisor Interrupt Enable
#define SIE_SEIE (1L << 9)  // external
#define SIE_STIE (1L << 5)  // timer
#define SIE_SSIE (1L << 1)  // software
static inline uint64_t r_sie() {
    uint64_t x;
    asm volatile("csrr %0, sie" : "=r"(x));
    return x;
}

static inline void w_sie(uint64_t x) { asm volatile("csrw sie, %0" : : "r"(x)); }

// Machine-mode Interrupt Enable
#define MIE_MEIE (1L << 11)  // external
#define MIE_MTIE (1L << 7)   // timer
#define MIE_MSIE (1L << 3)   // software

#define SIP_SEIP (1L << 9)  // SIP external
#define SIP_STIP (1L << 5)  // SIP timer
#define SIP_SSIP (1L << 1)  // SIP software

static inline uint64_t r_mie() {
    uint64_t x;
    asm volatile("csrr %0, mie" : "=r"(x));
    return x;
}

static inline void w_mie(uint64_t x) { asm volatile("csrw mie, %0" : : "r"(x)); }

// machine exception program counter, holds the
// instruction address to which a return from
// exception will go.
static inline void w_sepc(uint64_t x) { asm volatile("csrw sepc, %0" : : "r"(x)); }

static inline uint64_t r_sepc() {
    uint64_t x;
    asm volatile("csrr %0, sepc" : "=r"(x));
    return x;
}

// Machine Exception Delegation
static inline uint64_t r_medeleg() {
    uint64_t x;
    asm volatile("csrr %0, medeleg" : "=r"(x));
    return x;
}

static inline void w_medeleg(uint64_t x) { asm volatile("csrw medeleg, %0" : : "r"(x)); }

// Machine Interrupt Delegation
static inline uint64_t r_mideleg() {
    uint64_t x;
    asm volatile("csrr %0, mideleg" : "=r"(x));
    return x;
}

static inline void w_mideleg(uint64_t x) { asm volatile("csrw mideleg, %0" : : "r"(x)); }

// Supervisor Trap-Vector Base Address
// low two bits are mode.
static inline void w_stvec(uint64_t x) { asm volatile("csrw stvec, %0" : : "r"(x)); }

static inline uint64_t r_stvec() {
    uint64_t x;
    asm volatile("csrr %0, stvec" : "=r"(x));
    return x;
}

// Machine-mode interrupt vector
static inline void w_mtvec(uint64_t x) { asm volatile("csrw mtvec, %0" : : "r"(x)); }

// use riscv's sv39 page table scheme.
#define SATP_SV39 (8L << 60)

#define MAKE_SATP(pagetable) (SATP_SV39 | (((uint64_t)pagetable) >> 12))

// supervisor address translation and protection;
// holds the address of the page table.
static inline void w_satp(uint64_t x) { asm volatile("csrw satp, %0" : : "r"(x)); }

static inline uint64_t r_satp() {
    uint64_t x;
    asm volatile("csrr %0, satp" : "=r"(x));
    return x;
}

// Supervisor Scratch register, for early trap handler in trampoline.S.
static inline void w_sscratch(uint64_t x) { asm volatile("csrw sscratch, %0" : : "r"(x)); }

static inline void w_mscratch(uint64_t x) { asm volatile("csrw mscratch, %0" : : "r"(x)); }

// Supervisor Trap Cause
static inline uint64_t r_scause() {
    uint64_t x;
    asm volatile("csrr %0, scause" : "=r"(x));
    return x;
}

// Supervisor Trap Value
static inline uint64_t r_stval() {
    uint64_t x;
    asm volatile("csrr %0, stval" : "=r"(x));
    return x;
}

// Machine-mode Counter-Enable
static inline void w_mcounteren(uint64_t x) { asm volatile("csrw mcounteren, %0" : : "r"(x)); }

static inline uint64_t r_mcounteren() {
    uint64_t x;
    asm volatile("csrr %0, mcounteren" : "=r"(x));
    return x;
}

// machine-mode cycle counter
static inline uint64_t r_time() {
    uint64_t x;
    asm volatile("csrr %0, time" : "=r"(x));
    return x;
}

// enable device interrupts
static inline void intr_on() {
    w_sstatus(r_sstatus() | SSTATUS_SIE);
}

// // enable device interrupts
// static inline void intr_on() {
//     w_mstatus(r_mstatus() | MSTATUS_MIE);
// }

// disable device interrupts
static inline void intr_off() {
    w_sstatus(r_sstatus() & ~SSTATUS_SIE);
}

// // disable device interrupts
// static inline void intr_off() {
//     w_mstatus(r_mstatus() | ~MSTATUS_MIE);
// }

// are device interrupts enabled?
static inline int intr_get() {
    uint64_t x = r_sstatus();
    return (x & SSTATUS_SIE) != 0;
}

// // are device interrupts enabled?
// static inline int intr_get() {
//     uint64_t x = r_mstatus();
//     return (x & MSTATUS_MIE) != 0;
// }

static inline uint64_t r_sp() {
    uint64_t x;
    asm volatile("mv %0, sp" : "=r"(x));
    return x;
}

// read and write tp, the thread pointer, which holds
// this core's hartid (core number), the index into cpus[].
static inline uint64_t r_tp() {
    uint64_t x;
    asm volatile("mv %0, tp" : "=r"(x));
    return x;
}

static inline uint64_t r_fp() {
    uint64_t x;
    asm volatile("mv %0, fp" : "=r"(x));
    return x;
}

static inline void w_tp(uint64_t x) { asm volatile("mv tp, %0" : : "r"(x)); }

static inline uint64_t r_ra() {
    uint64_t x;
    asm volatile("mv %0, ra" : "=r"(x));
    return x;
}

#define IRQ_M_SOFT 3
#define IRQ_M_TIMER 7
#define IRQ_M_EXT 11

#define IRQ_S_SOFT 1
#define IRQ_S_TIMER 5
#define IRQ_S_EXT 9

// flush the TLB.
static inline void sfence_vma() {
    // the zero, zero means flush all TLB entries.
    asm volatile("sfence.vma zero, zero");
}

static inline void sfence_vma_addr(void *addr) { asm volatile("sfence.vma %0" ::"r"(addr) : "memory"); }

static inline void dsb(void) { sfence_vma() ;asm volatile("fence iorw, iorw" : : : "memory"); }

#define PGSIZE 4096
#define PGSHIFT 12

// #define PGROUNDUP(sz) (((sz) + PGSIZE - 1) & ~(PGSIZE - 1))
// #define PGROUNDDOWN(a) (((a)) & ~(PGSIZE - 1))

#define PGROUNDDOWN(a)                    \
    ({                                    \
        size_t __a = (size_t)(a);         \
        (typeof(a))(__a & ~(PGSIZE - 1)); \
    })

#define PGROUNDUP(a)                                     \
    ({                                                   \
        size_t __a = (size_t)(a);                        \
        (typeof(a))((__a + PGSIZE - 1) & ~(PGSIZE - 1)); \
    })

#define NELEM(x) (sizeof(x) / sizeof((x)[0]))
#endif