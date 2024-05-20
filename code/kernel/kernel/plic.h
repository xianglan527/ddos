#ifndef __KERNEL_PLIC_H__
#define __KERNEL_PLIC_H__
#include "stdarg.h"
#include "types.h"

void plic_init(void);
void plic_init_hart(void);
int plic_claim(void);
void plic_complete(int irq);
#endif