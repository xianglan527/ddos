#include "usertest.h"

#include "atomic.h"
#include "kerneltest.h"
#include "proc.h"
#include "riscv.h"
#include "uprintf.h"
#include "user.h"
#include "assert.h"

Atomic share;
bool tf0 = 0, tf1 = 0, tf2 = 0, tf3 = 0;

#define __COUNT_ 10000000


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


static inline uint64_t get_cpuid() {
    uint64_t x;
    asm volatile("mv %0, tp" : "=r"(x));
    return x;
}

void user_task1() {
    printf("cpu: %d pid: %d running user_task1\n", get_cpuid(), getpid());
    char *args[] = {"usermain", 0};
    exec("usermain", args);
}

void user_task2() {
    while (1) {
        printf("cpu: %d pid: %d running user_task2\n", get_cpuid(), getpid());
        yield();
        task_delay(DELAY);
    }
}

void user_task3() {
    while (1) {
        printf("cpu: %d pid: %d running user_task3\n", get_cpuid(), getpid());
        yield();
        task_delay(DELAY);
    }
}

void os_main(void) {
    // user_init(user_task0);
    user_init(user_task1);
    user_init(user_task2);
    user_init(user_task3);
    kernel_thread_init(kernel_init, "kernel_init");
}
