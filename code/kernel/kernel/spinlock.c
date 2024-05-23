#include "spinlock.h"
#include "proc.h"
#include "riscv.h"
#include "spinlock.h"
#include "assert.h"

void initlock(Spinlock *lk, char *name){
    lk->name = name;
    lk->locked = 0;
    lk->cpu = 0;
}

int holding(Spinlock *lk){
    int r;
    r = (lk->locked && lk->cpu == mycpu());
    return r;
}

void push_off(void){
    int old = intr_get();
    intr_off();
    if(mycpu()->noff == 0)
        mycpu()->intena = old;
    mycpu()->noff += 1;
}

void pop_off(void){
    Cpu *c = mycpu();
    if(intr_get())
        panic("pop_off - interruptible");
    if(c->noff < 1)
        panic("pop_off");
    c->noff -= 1;
    if(c->noff == 0 && c->intena)
        intr_on();
}

void acquire(Spinlock *lk){
    push_off();
    if(holding(lk))
        panic("acquire");
    while(__sync_lock_test_and_set(&lk->locked, 1) != 0);
    __sync_synchronize();
    lk->cpu = mycpu();
}

void release(struct spinlock *lk) {
    if (!holding(lk)) 
        panic("release");
    lk->cpu = 0;
    __sync_synchronize();
    __sync_lock_release(&lk->locked);
    pop_off();
}