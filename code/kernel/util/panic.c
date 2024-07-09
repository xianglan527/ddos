#include "panic.h"
#include "spinlock.h"
#include "riscv.h"

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
    pr.locking = 0;
    is_panic = 1;
    va_list ap;
    va_start(ap, fmt);
    cprintf("kernel panic at %s:%d:\n", file, line);
    vcprintf(fmt, ap);
    cprintf("\n");
    va_end(ap);

panic_dead:
    // backtrace();
    cprintf("The kernel has crashed. Please force shutdown!!!\n ");
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