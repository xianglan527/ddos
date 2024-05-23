#include "usertest.h"
#include "spinlock.h"
#include "proc.h"
#include "riscv.h"
#include "stdio.h"
#define DELAY 1000

void user_task0() {
    Proc *p = myproc();
    release(&p->lock);
    cprintf("process name is :%s Created!\n", p->name);
    while (1) {
        cprintf("%s running \n", p->name);
        task_delay(DELAY);
        bool intr = intr_get();
        // yield();
    }
}

void user_task1() {
    Proc *p = myproc();
    release(&p->lock);
    cprintf("process name is :%s Created!\n", p->name);
    while (1) {
        cprintf("%s running \n", p->name);
        task_delay(DELAY);
        bool intr = intr_get();
        // yield();
    }
}

void user_task2() {
    Proc *p = myproc();
    release(&p->lock);
    cprintf("process name is :%s Created!\n", p->name);
    while (1) {
        cprintf("%s running \n", p->name);
        task_delay(DELAY);
        bool intr = intr_get();
        // yield();
    }
}

void user_task3() {
    Proc *p = myproc();
    release(&p->lock);
    cprintf("process name is :%s Created!\n", p->name);
    while (1) {
        cprintf("%s running \n", p->name);
        task_delay(DELAY);
        bool intr = intr_get();
        // yield();
    }
}

/* NOTICE: DON'T LOOP INFINITELY IN main() */
void os_main(void) {
    alloc_proc(user_task0);
    alloc_proc(user_task1);
    alloc_proc(user_task2);
    alloc_proc(user_task3);
}
