#include "spinlock.h"

#include "assert.h"
#include "proc.h"
#include "riscv.h"
#include "spinlock.h"
#include "stdio.h"
#include "string.h"

struct {
    char file[lock_info_lens];
    int line;
    char func[lock_info_lens];
} lock_lock_info[lock_info_nums][lock_info_nest];

extern struct {
    Spinlock lock;
    int locking;
} pr;

static Atomic lock_info_index;

void initlock_info(void) { atomic_set(&lock_info_index, 0); }

void initlock(Spinlock *lk, char *name) {
    lk->name = name;
    lk->locked = 0;
    lk->cpu = 0;
    lk->info_index = atomic_add_return(&lock_info_index, 1);
    lk->info_nest = 0;
    assert(lk->info_index < lock_info_nums);
}



int holding(Spinlock *lk) {
    int r;
    r = (lk->locked && lk->cpu == mycpu());
    return r;
}

void push_off(void) {
    int old = intr_get();
    intr_off();
    if (mycpu()->noff == 0) mycpu()->intena = old;
    mycpu()->noff += 1;
}

void pop_off(void) {
    Cpu *c = mycpu();
    if (intr_get()) 
        panic("pop_off - interruptible");
    if (c->noff < 1) panic("pop_off");
    c->noff -= 1;
    if (c->noff == 0 && c->intena) intr_on();
}

static void push_spinlock_info(int index, int nest, const char *file, int line, const char *func) {
    assert(nest < lock_info_nest - 1);
    strncpy(lock_lock_info[index][nest].file, file, lock_info_lens);
    lock_lock_info[index][nest].line = line;
    strncpy(lock_lock_info[index][nest].func, func, lock_info_lens);
}

void acquire_with_info(Spinlock *lk, const char *file, int line, const char *func) {
    push_off();
    if (holding(lk)) {
        pr.locking = 0;
        int i = 0;
        for (i = 0; i < lk->info_nest; i++) {
            cprintf("%d : file %s line %d func %s\n", i, lock_lock_info[lk->info_index][i].file,
                    lock_lock_info[lk->info_index][i].line, lock_lock_info[lk->info_index][i].func);
        }
        cprintf("%d : file %s line %d func %s\n", i, file, line, func);
        panic("acquire");
    }

    while (__sync_lock_test_and_set(&lk->locked, 1) != 0);
    __sync_synchronize();
    lk->cpu = mycpu();
    push_spinlock_info(lk->info_index, lk->info_nest, file, line, func);
    lk->info_nest++;
}

void release_with_info(Spinlock *lk, const char *file, int line, const char *func) {
    if (!holding(lk)) {
        pr.locking = 0;
        int i = 0;
        for (i = 0; i < lk->info_nest; i++) { 
            cprintf("%d : file %s line %d func %s\n", i, lock_lock_info[lk->info_index][i].file,
                    lock_lock_info[lk->info_index][i].line, lock_lock_info[lk->info_index][i].func);
        }
        cprintf("%d : file %s line %d func %s\n", i, file, line, func);
        panic("release");
    }
    lk->cpu = 0;
    __sync_synchronize();
    __sync_lock_release(&lk->locked);
    pop_off();
    lk->info_nest--;
}