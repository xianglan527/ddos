#include "uart.h"
#include "stdio.h"
#include "printf.h"

static void cputch(int c, int *cnt){
    uart_putc(c);
    (*cnt)++;
}

int vcprintf(const char *fmt, va_list ap){
    int cnt = 0;
    vprintfmt((void (*)(int, void *))cputch, &cnt, fmt, ap);
    return cnt;
}

int cprintf(const char *fmt, ...){
    va_list ap;
    int cnt;
    va_start(ap, fmt);
    cnt = vcprintf(fmt, ap);
    va_end(ap);
    return cnt;
}

void cputchar(int c){
    uart_putc(c);
}

int cputs(const char *str){
    int cnt = 0;
    char c;
    while((c = *str++) != '\0')
        cputch(c, &cnt);
    cputch('\n', &cnt);
    return cnt;
}