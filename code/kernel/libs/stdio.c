#include "stdio.h"

#include "console.h"
#include "printf.h"
#include "spinlock.h"
#include "uart.h"
#include "proc.h"
struct {
    Spinlock lock;
    int locking;
} pr;

static void cputch(int c, int *cnt) {
    uart_putc(c);
    (*cnt)++;
}

int vcprintf(const char *fmt, va_list ap) {
    int cnt = 0;
    vprintfmt((void (*)(int, void *))cputch, &cnt, fmt, ap);
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
    while ((c = *str++) != '\0') cputch(c, &cnt);
    cputch('\n', &cnt);
    return cnt;
}

int getchar(void) {
    int c;
    while ((c = cons_getc()) == 0);
    return c;
}

void printf_init(void) {
    initlock(&pr.lock, "pr");
    pr.locking = 1;
}