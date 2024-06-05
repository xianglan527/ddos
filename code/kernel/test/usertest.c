#include "usertest.h"

#include "atomic.h"
#include "proc.h"
#include "riscv.h"
#include "uprintf.h"

Atomic share;
bool tf0 = 0, tf1 = 0, tf2 = 0, tf3 = 0;

#define __COUNT_ 10000000
#define DELAY 1000

// void user_task0() {
//     for (int i = 0; i < __COUNT_; i++)
//         atomic_inc(&share);
//         // share.counter++;
//     tf0 = 1;
//     while(1);
// }

// void user_task1() {
//     for (int i = 0; i < __COUNT_; i++)
//         atomic_inc(&share);
//         // share.counter++;
//     tf1 = 1;
//     while (1);
// }

// void user_task2() {
//     for (int i = 0; i < __COUNT_; i++)
//         atomic_inc(&share);
//         // share.counter++;
//     tf2 = 1;
//     while (1);
// }

// void user_task3() {
//     for (int i = 0; i < __COUNT_; i++) 
//         atomic_inc(&share);
//         // share.counter++;
//     tf3 = 1;
//     while (1);
// }

// void result() {
//     while(1){
//         if(tf0 && tf1 && tf2 && tf3){
//             printf("share is :%d   __COUNT_ * 4  is %d\n", atomic_read(&share), __COUNT_ * 4);
//             break;
//         }      
//     }
//     while(1);
// }

// /* NOTICE: DON'T LOOP INFINITELY IN main() */
// void os_main(void) {
//     atomic_set(&share, 0);
//     user_init(user_task0);
//     user_init(user_task1);
//     user_init(user_task2);
//     user_init(user_task3);
//     user_init(result);
// }

void user_task0() {
    Proc *p = myproc();
    // release(&p->lock);
    // cprintf("process name is :%s Created!\n", p->name);
    while (1) {
        printf("%s running 0\n", p->name);
        task_delay(DELAY);
        // asm volatile("ecall");
        // bool intr = intr_get();
        // yield();
    }
}

void user_task1() {
    Proc *p = myproc();
    // release(&p->lock);
    // cprintf("process name is :%s Created!\n", p->name);
    while (1) {
        printf("%s running 1\n", p->name);
        task_delay(DELAY);
        // asm volatile("ecall");
        // bool intr = intr_get();
        // yield();
    }
}

void user_task2() {
    Proc *p = myproc();
    // release(&p->lock);
    // cprintf("process name is :%s Created!\n", p->name);
    while (1) {
        printf("%s running 2\n", p->name);
        task_delay(DELAY);
        // asm volatile("ecall");
        // bool intr = intr_get();
        // yield();
    }
}

void user_task3() {
    Proc *p = myproc();
    // release(&p->lock);
    // cprintf("process name is :%s Created!\n", p->name);
    while (1) {
        printf("%s running 3\n", p->name);
        task_delay(DELAY);
        // asm volatile("ecall");
        // bool intr = intr_get();
        // yield();
    }
}

/* NOTICE: DON'T LOOP INFINITELY IN main() */
void os_main(void) {
    user_init(user_task0);
    user_init(user_task1);
    user_init(user_task2);
    user_init(user_task3);
}
