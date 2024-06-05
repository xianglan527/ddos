#ifndef __KERNEL_SPINLOCK_H__
#define __KERNEL_SPINLOCK_H__
#include "stdarg.h"
#include "types.h"
typedef struct cpu Cpu;
typedef struct spinlock Spinlock;
struct spinlock{
    bool locked;
    char *name;
    Cpu *cpu; 
};

void initlock(Spinlock *lk, char *name);
int holding(Spinlock *lk);
void push_off(void);
void pop_off(void);
void acquire(Spinlock *lk);
void release(struct spinlock *lk);
#endif