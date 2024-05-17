#ifndef __LIBS_STRING_H__
#define __LIBS_STRING_H__
#include "types.h"
#include "stdarg.h"

int vcprintf(const char *fmt, va_list ap);
int cprintf(const char *fmt, ...);
void cputchar(int c);
int cputs(const char *str);
int getchar(void);
#endif