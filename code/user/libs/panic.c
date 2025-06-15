#include "panic.h"
#include "config.h"
#include "printf.h"


volatile bool is_panic = 0;

#define PGSIZE 4096

#define PGROUNDDOWN(a)                    \
    ({                                    \
        size_t __a = (size_t)(a);         \
        (typeof(a))(__a & ~(PGSIZE - 1)); \
    })

#define PGROUNDUP(a)                                     \
    ({                                                   \
        size_t __a = (size_t)(a);                        \
        (typeof(a))((__a + PGSIZE - 1) & ~(PGSIZE - 1)); \
    })

static inline uint64_t r_fp() {
    uint64_t x;
    asm volatile("mv %0, s0" : "=r"(x));
    return x;
}

// #define USTACKADDR PGSIZE
// #define USTACKPAGE 8
// #define USTACKSIZE (USTACKPAGE * PGSIZE)
// void backtrace(void) {
//     uint64_t fp_address;
//     asm volatile("mv %0, fp" : "=r"(fp_address));
//     uint64_t stackTop = USTACKADDR + USTACKSIZE;
//     while (fp_address != stackTop) {
//         printf("%p\n", *(uint64_t *)(fp_address - 8));
//         fp_address = *(uint64_t *)(fp_address - 16);
//     }
// }

void __panic(const char *file, int line, const char *fmt, ...) {
    if (is_panic) goto panic_dead;
    va_list ap;
    va_start(ap, fmt);
    fprintf(2,"panic at %s:%d:\n", file, line);
    vcprintf(fmt, ap);
    printf("\n");
    va_end(ap);
    // printf("backtrace......:\n");
    // backtrace();
    // printf("end of backtrace......:\n");
    printf("The program has crashed. Please force shutdown!!!\n ");
panic_dead:
    is_panic = 1;
    while (1);
}

void __warn(const char *file, int line, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(2,"warning at %s:%d:\n    ", file, line);
    vcprintf(fmt, ap);
    printf("\n");
    va_end(ap);
}

bool is_kernel_panic(void) { return is_panic; }

bool set_kernel_panic(int panic_flag) {
    bool old_panic_flag = is_panic;
    is_panic = panic_flag;
    return old_panic_flag;
}