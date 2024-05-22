#include "stdio.h"
#include "proc.h"
#include "stdio.h"
#include "usertest.h"
#include "risv.h"
#define DELAY 1000

void user_task0(Proc p) {
    cprintf("process name is :%s Created!\n", p.name);
    while (1) {
        int bool = intr_get();
        cprintf("%s running \n", p.name);
        task_delay(DELAY);
        // yield();
    }
}

void user_task1(Proc p) {
    cprintf("process name is :%s Created!\n", p.name);
    while (1) {
        int bool = intr_get();
        cprintf("%s running \n", p.name);
        task_delay(DELAY);
        // yield();
    }
}

/* NOTICE: DON'T LOOP INFINITELY IN main() */
void os_main(void) {
    task_create(user_task0);
    task_create(user_task1);
}