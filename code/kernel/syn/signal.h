#ifndef __SYN_SIGNAL_H__
#define __SYN_SIGNAL_H__
#include "list.h"
#include "spinlock.h"
#include "stdarg.h"
#include "types.h"
#include "atomic.h"
#include "sigaction.h"
#include "riscv.h"


typedef struct siginfo Siginfo;
struct siginfo{
    int sig;
    int pid;
    List_entry siginfo_link;
};
#define le2siginfo(le, member) to_struct((le), Siginfo, member)

typedef struct signal Signal;
struct signal{
    // Atomic count;
    Sigaction action[NSIG];
    Spinlock signal_lock;
};

typedef struct trapframe Trapframe;

typedef struct sigframe Sigframe;
struct sigframe{
    int sig;
    Trapframe saved_trapframe;
    char ret_code[8];
};

void signal_init(Signal *signal);
void sigaction_copy(Signal *to, Signal *from);
int ipc_set_sigaction(int sig, Sigaction *sa);
int ipc_send_signal(int pid, int sig);
void do_signal();
int ipc_sigreturn(void);
#endif