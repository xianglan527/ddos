#ifndef __KERNEL_SPINLOCK_H__
#define __KERNEL_SPINLOCK_H__
#include "stdarg.h"
#include "types.h"
#include "atomic.h"
#include "config.h"
typedef struct cpu Cpu;
typedef struct spinlock Spinlock;
struct spinlock{
    volatile bool locked;
    char *name;
    Cpu *cpu;
    char file[lock_info_lens];
    long line;
    char func[lock_info_lens];
};
void acquire_with_info(Spinlock *lk,const char *file, int line, const char *func);
void release_with_info(Spinlock *lk, const char *file, int line, const char *func);

#define acquire(lk) acquire_with_info((lk), __FILE__, __LINE__, __func__)
#define release(lk) release_with_info((lk), __FILE__, __LINE__, __func__)

void initlock(Spinlock *lk, char *name);
int holding(Spinlock *lk);
void push_off(void);
void pop_off(void);
// void acquire(Spinlock *lk);
// void release(struct spinlock *lk);
void initlock_info(void);
#endif