#ifndef __KERNEL_CLINT_H__
#define __KERNEL_CLINT_H__
#include "stdarg.h"
#include "types.h"

void clint_set_msip(uchar val, uint64_t hart_id);
void clint_set_ssip(bool val, uint64_t hart_id);
void clint_set_mtime(uint64_t val);
uint64_t clint_get_mtime(void);
void clint_set_mtimecmp(uint64_t val, uint64_t hart_id);
uint64_t clint_get_mtimecmp(uint64_t hart_id);
volatile uint64_t *clint_get_mtimecmp_ptr(uint64_t hart_id);
void clint_add_mtimecmp(uint64_t val, uint64_t hart_id);

#endif