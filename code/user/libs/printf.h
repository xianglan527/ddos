#ifndef __TEST_UPRINTF_H__
#define __TEST_UPRINTF_H__

#include "stdarg.h"
#include "types.h"

typedef struct sprintbuf Sprintbuf;
struct sprintbuf {
    char *buf;
    char *ebuf;
    int cnt;
};
int vcprintf(const char *fmt, va_list ap);
int printf(const char *fmt, ...);
int snprintf(char *str, size_t size, const char *fmt, ...);
#endif