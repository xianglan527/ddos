#ifndef __KERNEL_TRAP_H__
#define __KERNEL_TRAP_H__
#include "stdarg.h"
#include "types.h"
void trap_init_hart(void);
void kerneltrap();
#endif