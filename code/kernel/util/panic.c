#include "panic.h"
#include "spinlock.h"
#include "riscv.h"
#include "proc.h"

volatile bool is_panic = 0;
extern struct {
    Spinlock lock;
    int locking;
} pr;

void backtrace(void) {
    uint64_t fp_address = r_fp();
    while (fp_address != PGROUNDDOWN(fp_address)) {
        cprintf("%p\n", *(uint64_t *)(fp_address - 8));
        fp_address = *(uint64_t *)(fp_address - 16);
    }
}

void __panic(const char *file, int line, const char *fmt, ...){
    if(is_panic)
        goto panic_dead;
    va_list ap;
    va_start(ap, fmt);
    if(myproc() != nullptr)
        cprintf("cpuid :%d pid :%dkernel panic at %s:%d:\n", cpuid(), myproc()->pid, file, line);
    else
        cprintf("cpuid :%d kernel panic at %s:%d:\n", cpuid(), file, line);
    vcprintf(fmt, ap);
    cprintf("\n");
    va_end(ap);
panic_dead:
    cprintf("backtrace......:\n");
    backtrace();
    cprintf("end of backtrace......:\n");
    cprintf("The kernel has crashed. Please force shutdown!!!\n ");
    pr.locking = 0;
    is_panic = 1;
    while(1);
}

void __warn(const char *file, int line, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    cprintf("kernel warning at %s:%d:\n    ", file, line);
    vcprintf(fmt, ap);
    cprintf("\n");
    va_end(ap);
}

bool is_kernel_panic(void){
    return is_panic;
}

bool set_kernel_panic(int panic_flag){
    bool old_panic_flag = is_panic;
    is_panic = panic_flag;
    return old_panic_flag;
}