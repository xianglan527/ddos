#include "thread.h"
#include "error.h"
#include "sysdef.h"
#include "assert.h"

int thread(int (*fn)(void *), void *arg, Thread *tidp){
    if(fn == nullptr || tidp == nullptr)
        return -E_INVAL;
    int ret;
    uintptr_t stack = 0;
    if((ret = mmap(&stack, THREAD_STACKSIZE, MMAP_WRITE | MMAP_STACK)) != 0){
        return ret;
    }
    assert(stack != 0);
    if((ret = clone((CLONE_VM | CLONE_THREAD), stack + THREAD_STACKSIZE, fn, arg)) < 0){
        munmap(stack, THREAD_STACKSIZE);
        return ret;
    }
    tidp->pid = ret;
    tidp->stack = (void *)stack;
    return 0;
}

int thread_wait(Thread *tidp, int *exit_code){
    int ret = -E_INVAL;
    if(tidp != nullptr){
        if((ret = waitpid(tidp->pid, exit_code)) == 0){
            munmap((uintptr_t)(tidp->stack), THREAD_STACKSIZE);
        }
    }
    return ret;
}

int thread_kill(Thread *tidp){
    if(tidp != nullptr){
        return kill(tidp->pid);
    }
    return -E_INVAL;
}