#include "console.h"
#include "stdio.h"
#include "types.h"
#include "stdarg.h"
#include "error.h"
#include "assert.h"
#include "plic.h"
#include "trap.h"
#include "risv.h"

void test(void) {
    int version = 20240516;
    char *hello = "Hello, qemu and risc-v!";
    cprintf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
    cprintf("  version is: %d\n", version);
    cprintf("  version is: 0x%08x\n", version);
    cprintf("  pointer is: %p\n", &version);
    cprintf("  %s\n", hello);
    cprintf("  error is %e\n", E_UNSPECIFIED);
    cprintf("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");

    char *buf = readline("------>>");
    cprintf("------<<%s\n", buf);

    assert(1 == 1);
    warn("warning...");
    assert(1 != 1);
}

void main(){
    plic_init();
    plic_init_hart();
    trap_init_hart();
    intr_on();
    test();
    while(1);
}