#include "assert.h"
#include "virtio_device.h"
#include "console.h"
#include "error.h"
#include "plic.h"
#include "pmm.h"
#include "proc.h"
#include "riscv.h"
#include "slab.h"
#include "stdarg.h"
#include "stdio.h"
#include "trap.h"
#include "types.h"
#include "usertest.h"
#include "vmm.h"
#include "config.h"
#include "rand.h"
#include "swap.h"

volatile static int started = 0;

void basic_test(void) {
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

    assert(1 == 1);
    warn("warning...");
    // int i = 1 / 0;
    // *(char *)0 = 4;

    // assert(1 != 1);
}


void main(){
    if(cpuid() == 0){
        console_init();
        cprintf("xv6 kernel is booting\n");
        pmm_init();
        slab_init();
        kvm_init_hart();
        printf_init();
        trap_tick_init();
        plic_init();
        plic_init_hart();
        trap_init_hart();
        intr_on();
        vmm_init();
        proc_init();
        // basic_test();
        user_lock_init();
        virtio_device_init();
        __sync_synchronize();
        swap_init();
        os_main();
        started = 1;
    }else{
        while (started == 0);
        __sync_synchronize();
        cprintf("hart %d starting\n", cpuid());
        kvm_init_hart();
        plic_init_hart();
        trap_init_hart();
    }
    scheduler();
    // while(1);
}