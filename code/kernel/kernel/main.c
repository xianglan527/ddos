#include "console.h"
#include "stdio.h"
#include "types.h"
#include "stdarg.h"
#include "error.h"
#include "assert.h"
#include "plic.h"
#include "trap.h"
#include "riscv.h"
#include "proc.h"
#include "usertest.h"
#include "pmm.h"

volatile static int started = 0;

void test(void) {
    int version = 20240516;
    char *hello = "Hello, qemu and risc-v!";
    cprintf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
    cprintf("  version is: %d\n", version);
    cprintf("  version is: %d        version is: 0x%08x\n", version, version);
    cprintf("  pointer is: %p\n", &version);
    cprintf("  %s\n", hello);
    cprintf("  error is %e\n", E_UNSPECIFIED);
    cprintf("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");

    char *buf = readline("------>>");
    cprintf("------<<%s\n", buf);

    // assert(1 == 1);
    // warn("warning...");
    // // int i = 1 / 0;
    // *(char *)0 = 4;
    // int i = *(char *)0;

    // assert(1 != 1);
}

void main(){
    if(cpuid() == 0){
        console_init();
        cprintf("xv6 kernel is booting\n");
        pmm_init();
        printf_init();
        trap_tick_init();
        plic_init();
        plic_init_hart();
        trap_init_hart();
        intr_on();
        proc_init();
        // test();
        os_main();
        started = 1;
        user_lock_init();
        __sync_synchronize();
    }else{
        while (started == 0);
        __sync_synchronize();
        cprintf("hart %d starting\n", cpuid());
        plic_init_hart();
        trap_init_hart();
    }
    scheduler();
    // while(1);
}