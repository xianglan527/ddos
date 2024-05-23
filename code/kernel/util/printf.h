#ifndef __UTIL_STDIO_H__
#define __UTIL_STDIO_H__

#include "stdarg.h"
#include "types.h"


typedef struct sprintbuf Sprintbuf;
struct sprintbuf{
    char *buf;
    char *ebuf;
    int cnt;
};


void printfmt(void (*putch)(int, void *), void *putdat, const char *fmt, ...);
void vprintfmt(void (*putch)(int, void *), void *putdat, const char *fmt, va_list ap);
int snprintf(char *str, size_t size, const char *fmt, ...);
int vsnprintf(char *str, size_t size, const char *fmt, va_list ap);
#endif
