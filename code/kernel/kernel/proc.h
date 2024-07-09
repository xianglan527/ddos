#ifndef __KERNEL_PROC_H__
#define __KERNEL_PROC_H__
#include "config.h"
#include "pmm.h"
#include "spinlock.h"
#include "stdarg.h"
#include "types.h"
#include "vmm.h"

typedef enum proc_state Proc_state;
enum proc_state { UNUSED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// typedef struct spinlock Spinlock;
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

typedef struct context Context;
struct context {
    uint64_t ra;
    uint64_t sp;
    uint64_t a0;
    uint64_t a1;
    uint64_t a2;
    uint64_t a3;
    uint64_t a4;
    uint64_t a5;
    uint64_t a6;
    uint64_t a7;

    // callee-saved
    uint64_t s0;
    uint64_t s1;
    uint64_t s2;
    uint64_t s3;
    uint64_t s4;
    uint64_t s5;
    uint64_t s6;
    uint64_t s7;
    uint64_t s8;
    uint64_t s9;
    uint64_t s10;
    uint64_t s11;
};

#define PROC_NAME_LEN 64
#define MAX_PID (NPROC * 2)
#define USTACKPAGE 2
#define USTACKSIZE (USTACKPAGE * PGSIZE)
#define USTACKADDR  PGSIZE

typedef struct proc Proc;
struct proc {
    bool kernel_proc;
    char name[PROC_NAME_LEN + 1];
    Spinlock lock;
    Proc_state state;
    void *chan;
    Context context;
    Trapframe *trapframe;
    int pid;
    ulong runs;
    // pagetable_t *pagetable;
    uintptr_t kstack;
    Mm_struct *mm;
    Proc *parent;
    uint32_t flags;
    List_entry list_link;
    List_entry hash_link;
    int exit_code;
    uint32_t wait_state;
    Proc *cptr, *yptr, *optr;
};

#define PF_EXITING 0x00000001

#define WT_INTERAUPTED 0x80000000
#define WT_CHILD (0x00000001 | WT_INTERAUPTED)

#define le2proc(le, member) to_struct((le), Proc, member);

extern List_entry proc_list;
extern Proc *initproc;

typedef struct cpu Cpu;
struct cpu {
    Proc *proc;       // The process running on this cpu, or null.
    Context context;  // swtch() here to enter scheduler().
    int noff;         // Depth of push_off() nesting.
    int intena;       // Were interrupts enabled before push_off()?
};

extern Cpu cpus[NCPU];

int cpuid();
void scheduler(void);
void task_delay(volatile int count);
void do_yield(void);
void sched(void);
Proc *myproc(void);
Cpu *mycpu(void);
Proc *alloc_proc(void);
void proc_init(void);
void user_init(void (*start_routin)(void));
void kernel_thread_init(void (*start_roution)(void *), void *arg);
Proc *find_proc(int pid);
void either_copy_user2kernel(void *dst, int user_src, uint64_t src, uint64_t len);
void do_sleep(void *chan, Spinlock *lk);
void do_wakeup(void *chan);
int do_fork(uint32_t clone_flags);
void do_exit(int error_code);
int do_wait(int pid, int *code_store);
#endif