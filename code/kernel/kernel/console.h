#ifndef __KERNEL_CONSOLE_H__
#define __KERNEL_CONSOLE_H__
#include "stdarg.h"
#include "types.h"

void uart_intr(void);
int cons_getc(void);
char *readline(const char *prompt);
#endif