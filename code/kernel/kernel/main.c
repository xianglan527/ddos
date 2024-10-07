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
#include "sched.h"
#include "syn.h"
#include "atomic.h"

// volatile static int started = 0;
Atomic cpus_num;

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
        // timer_start_init();
        atomic_set(&cpus_num, 0);
        // initlock_info();
        console_init();
        cprintf("ddos kernel is booting\n");
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
        virtio_device_init();
        sync_init();
        swap_init();
        sched_init();
        os_main();
        atomic_set(&cpus_num, 1);
        // started = 1;
    }else{
        while (atomic_read(&cpus_num) == 0);
        cprintf("hart %d starting\n", cpuid());
        kvm_init_hart();
        plic_init_hart();
        trap_init_hart();
        sched_init();
        atomic_inc(&cpus_num);
    }
    while(atomic_read(&cpus_num) != CPUS);
    scheduler();
    // while(1);
}