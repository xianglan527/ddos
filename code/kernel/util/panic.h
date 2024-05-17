#ifndef __UTIL_PANIC_H__
#define __UTIL_PANIC_H__

#include "types.h"
#include "stdarg.h"
#include "stdio.h"

void __panic(const char *file, int line, const char *fmt, ...);
void __warn(const char *file, int line, const char *fmt, ...);
bool is_kernel_panic(void);
bool set_kernel_panic(int panic_flag);
#endif
