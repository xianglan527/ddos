#include "usertest.h"
#include "spinlock.h"
#include "proc.h"
#include "riscv.h"
#include "stdio.h"
#define DELAY 1000

void user_task0(Proc *p) {
    release(&p->lock);
    cprintf("process name is :%s Created!\n", p->name);
    while (1) {
        cprintf("%s running \n", p->name);
        task_delay(DELAY);
        bool intr = intr_get();
        // yield();
    }
}

void user_task1(Proc *p) {
    release(&p->lock);
    cprintf("process name is :%s Created!\n", p->name);
    while (1) {
        cprintf("%s running \n", p->name);
        task_delay(DELAY);
        bool intr = intr_get();
        // yield();
    }
}

void user_task2(Proc *p) {
    release(&p->lock);
    cprintf("process name is :%s Created!\n", p->name);
    while (1) {
        cprintf("%s running \n", p->name);
        task_delay(DELAY);
        bool intr = intr_get();
        // yield();
    }
}

void user_task3(Proc *p) {
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
    task_create(user_task0);
    task_create(user_task1);
    task_create(user_task2);
    task_create(user_task3);
}
