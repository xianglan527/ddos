#ifndef __KERNEL_PROC_H__
#define __KERNEL_PROC_H__
#include "config.h"
#include "pmm.h"
#include "spinlock.h"
#include "stdarg.h"
#include "types.h"
#include "vmm.h"
#include "sched.h"
#include "sem.h"
#include "event.h"
#include "signal.h"
#include "riscv.h"
#include "rbtree.h"
#include "fs.h"

typedef enum proc_state Proc_state;
enum proc_state { UNUSED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE};

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
#define KSTACKPAGE 8
#define KSTACKSIZE (KSTACKPAGE * PGSIZE)
#define USTACKPAGE 8
#define USTACKSIZE (USTACKPAGE * PGSIZE)
#define USTACKADDR  PGSIZE

// map kernel stacks beneath the trampoline,
// each surrounded by invalid guard pages.
#define KSTACK(p) (TRAMPOLINE - ((p) + 1) * KSTACKSIZE)
#define KSTACK2INDEX(addr) ((TRAMPOLINE - (addr)) / KSTACKSIZE - 1)

typedef struct proc Proc;
struct proc {
    char name[PROC_NAME_LEN + 1];
    bool kernel_proc;
    Spinlock lock;
    Proc_state state;
    void *chan;
    Context context;
    Trapframe *trapframe;
    int pid;
    ulong runs;
    uintptr_t kstack;
    Mm_struct *mm;
    Proc *parent;
    uint32_t flags;
    List_entry list_link;
    List_entry hash_link;
    int exit_code;
    uint32_t wait_state;
    Proc *cptr, *yptr, *optr;
    Cpu *cpu;
    Cpu *set_cpu;
    List_entry thread_group;
    int mm_index;
    List_entry run_link;
    ulong time_slice;
    ulong alloc_time_slice;
    bool need_resched;
    Sem_queue *sem_queue;
    Event event;
    uint64_t sig_blocked;
    List_entry siginfo_list;
    Signal signal;
    int priority;    //-20 ~ 19
    uint64_t vruntime; 
    Rb_node rb_link;
    Fs_struct *fs_struct;
};

#define rbn2proc(node)  (to_struct(node, Proc, rb_link))

static inline int proc_vruntime_compare_node(Rb_node *node1, Rb_node *node2){
    if (rbn2proc(node1) ->vruntime > rbn2proc(node2)->vruntime){
        return 1;
    }else if(rbn2proc(node1) ->vruntime < rbn2proc(node2)->vruntime){
        return -1;
    }else{
        return 0;
    }
}

static inline int proc_vruntime_compare_value(Rb_node *node1, void *value) {
    if (rbn2proc(node1)->vruntime > (uint64_t)value) {
        return 1;
    } else if (rbn2proc(node1)->vruntime < (uint64_t)value) {
        return -1;
    } else {
        return 0;
    }
}

#define PF_EXITING 0x00000001

#define WT_INTERAUPTED 0x80000000
#define WT_CHILD (0x00000001 | WT_INTERAUPTED)
#define WT_TIMER (0x00000002 | WT_INTERAUPTED)
#define WT_KSWAPD 0x00000003
#define WT_KBD (0x00000004 | WT_INTERAUPTED)
#define WT_KSEM 0x00000100
#define WT_USEM (0x00000101 | WT_INTERAUPTED)
#define WT_EVENT_SEND (0x00000110 | WT_INTERAUPTED)
#define WT_EVENT_RECV (0x00000111 | WT_INTERAUPTED)
#define WT_MBOX_SEND (0x00000120 | WT_INTERAUPTED)
#define WT_MBOX_RECV (0x00000121 | WT_INTERAUPTED)
#define WT_PIPE (0x00000200 | WT_INTERAUPTED)
#define WT_DISK_BUF (0x00000201 | WT_INTERAUPTED)
#define WT_LOG (0x00000202 | WT_INTERAUPTED)

#define le2proc(le, member) to_struct((le), Proc, member);

extern List_entry proc_list;
extern Proc *initproc;
extern List_entry proc_mm_list;
extern Proc *daemonproc;

typedef struct cpu Cpu;
struct cpu {
    Proc *proc;       // The process running on this cpu, or null.
    Context context;  // swtch() here to enter scheduler().
    int noff;         // Depth of push_off() nesting.
    int intena;       // Were interrupts enabled before push_off()?
    Proc *prev;
    Proc *next;
    Run_queue rq;
    Sched_class *sc;
    Spinlock cpu_lock;
};

extern Cpu cpus[NCPU];

typedef struct timer Timer;
struct timer{
    ulong expires;
    Proc *proc;
    List_entry timer_link;
};

#define le2timer(le, member)    to_struct((le), Timer, member)

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
Proc *kernel_thread_init(void (*start_roution)(void *), void *arg);
Proc *find_proc(int pid);
void either_copy_user2kernel(void *dst, int user_src, uint64_t src, uint64_t len);
void either_copy_kernel2user(uint64_t dst, int user_dst, void *src, uint64_t len);
void sleeping(void *chan, Spinlock *lk);
void wakeup_proc(Proc *proc);
void do_wakeup(void *chan);
int do_fork(uint32_t clone_flags, uintptr_t stack);
void do_exit(int error_code);
int do_wait(int pid, int *code_store);
int do_execve(char *name, char **argv);
void proc_dump(void);
void proc_mm_dump(void);
int do_kill(int pid);
int do_brk(uintptr_t *brk_store);
void timer_start_init(void);
void run_timer_list(void);
void timer_dump(void);
int do_sleep(ulong time);
void may_killed(void);
void do_exit_thread(int error_code);
int do_mmap(uintptr_t *addr_store, size_t len, uint32_t mmap_flags);
int do_munmap(uintptr_t addr, size_t len);
int do_shmem(uintptr_t *addr_store, size_t len, uint32_t mmap_flags);
void set_proc_cpu(Proc *proc, Cpu *cpu);
void clear_proc_setcpu(Proc *proc);
void ipc_add_timer(Timer *timer);
void ipc_del_timer(Timer *timer);
Timer *ipc_timer_init(ulong timeout, ulong *saved_ticks, Timer *timer);
int ipc_check_timeout(ulong timeout, ulong saved_ticks);
void clean_kernel_proc(void);
#endif