#ifndef __LIBS_PANIC_H__
#define __LIBS_PANIC_H__

#include "stdarg.h"
#include "types.h"

void __panic(const char *file, int line, const char *fmt, ...);
void __warn(const char *file, int line, const char *fmt, ...);
bool is_kernel_panic(void);
bool set_kernel_panic(int panic_flag);
#endif
