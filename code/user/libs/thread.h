#ifndef __LIBS_THREAD_H__
#define __LIBS_THREAD_H__

#include "stdarg.h"
#include "types.h"
#include "user.h"

typedef struct thread Thread;
struct thread{
    int pid;
    void *stack;
};

#define THREAD_STACKSIZE (4096 * 8)

int thread(int (*fn)(void *), void *arg, Thread *tidp);
int thread_wait(Thread *tidp, int *exit_code);
int thread_kill(Thread *tidp);

#endif
