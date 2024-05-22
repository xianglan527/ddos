#include "stdio.h"
#include "proc.h"
#include "stdio.h"
#include "usertest.h"
#define DELAY 1000

void user_task0(void) {
    cprintf("Task 0: Created!\n");
    while (1) {
        cprintf("Task 0: Running...\n");
        task_delay(DELAY);
        yield();
    }
}

void user_task1(void) {
    cprintf("Task 1: Created!\n");
    while (1) {
        cprintf("Task 1: Running...\n");
        task_delay(DELAY);
        yield();
    }
}

/* NOTICE: DON'T LOOP INFINITELY IN main() */
void os_main(void) {
    task_create(user_task0);
    task_create(user_task1);
}