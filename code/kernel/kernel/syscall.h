#ifndef __KERNEL_SYSCALL_H__
#define __KERNEL_SYSCALL_H__
#include "stdarg.h"
#include "types.h"
#include "sysdef.h"
// System call numbers


int fetch_addr(uint64_t addr, uint64_t *ip);
int fetch_str(uint64_t addr, char *buf, int max);

int arg_int(int n, int *ip);
int arg_long(int n, long *ip); 
int arg_addr(int n, uint64_t *ip);
int arg_str(int n, char *buf, int max);
void syscall(void);

#endif