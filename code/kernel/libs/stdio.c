#include "stdio.h"

#include "printf.h"
#include "spinlock.h"
#include "uart.h"
#include "proc.h"
#include "sysdef.h"
struct {
    Spinlock lock;
    int locking;
} pr;

static void cputch(int c, int *cnt, int fd) {
    // assert(fd == NO_FD);
    uart_putc(c);
    (*cnt)++;
}

int vcprintf(const char *fmt, va_list ap) {
    int cnt = 0;
    vprintfmt((void (*)(int, void *, int))cputch, NO_FD, &cnt, fmt, ap);
    return cnt;
}

int cprintf(const char *fmt, ...) {
    va_list ap;
    int cnt;
    int locking = pr.locking;
    if (locking) 
        acquire(&pr.lock);
    va_start(ap, fmt);
    cnt = vcprintf(fmt, ap);
    va_end(ap);
    if (locking) 
        release(&pr.lock);
    return cnt;
}

void cputchar(int c) { uart_putc(c); }

int cputs(const char *str) {
    int cnt = 0;
    char c;
    while ((c = *str++) != '\0') cputch(c, &cnt, NO_FD);
    cputch('\n', &cnt, NO_FD);
    return cnt;
}

extern int stdin_getchar(void);

int getchar(void) {
    // int c;
    // while ((c = cons_getc()) == 0);
    // return c;
    return stdin_getchar();
}

void printf_init(void) {
    initlock(&pr.lock, "pr");
    pr.locking = 1;
}