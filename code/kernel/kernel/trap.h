#ifndef __KERNEL_TRAP_H__
#define __KERNEL_TRAP_H__
#include "stdarg.h"
#include "types.h"

typedef enum trap_enum Trap_eum;

enum trap_enum{
    TRAP_INT, 
    TRAP_SOFT_INT,
    TRAP_EXCEPTION,
    TRAP_OTHER,
};

void trap_init_hart(void);
void kerneltrap();
void user_trap_ret();
void trap_tick_init(void);
void usertrap(void);
#endif