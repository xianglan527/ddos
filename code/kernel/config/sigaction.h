#ifndef __CONFIG_SIGACTION_H__
#define __CONFIG_SIGACTION_H__

#include "types.h"

#define SIGHUP 1
#define SIGINT 2
#define SIGQUIT 3
#define SIGILL 4
#define SIGTRAP 5
#define SIGABRT 6
#define SIGIOT 6
#define SIGUNUSED 7
#define SIGFPE 8
#define SIGKILL 9
#define SIGUSR1 10
#define SIGSEGV 11
#define SIGUSR2 12
#define SIGPIPE 13
#define SIGALRM 14
#define SIGTERM 15
#define SIGSTKFLT 16
#define SIGCHLD 17
#define SIGCONT 18
#define SIGSTOP 19
#define SIGTSTP 20
#define SIGTTIN 21
#define SIGTTOU 22

#define NSIG 32

#define SA_NOMASK 0x00000001
#define SA_ONESHOT 0x00000002

#define SIG_DFL (sighandler_t)(0)

typedef void (*sighandler_t)(void);
typedef uint64_t sigset_t;

typedef struct sigaction Sigaction;
struct sigaction{
    sighandler_t sa_handler;
    uint64_t    sa_flags;
    sigset_t    sa_mask;
};

#endif