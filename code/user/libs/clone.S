#include "sysdef.h"

.section .text
.globl __clone
__clone:                        # __clone(clone_flags, stack, fn, arg)
    addi sp, sp, -8*4           # allocate stack space (saving 4 registers)
    sd ra, 24(sp)                
    sd s0, 16(sp)               
    sd s1, 8(sp)              
    sd s2, 0(sp)     

    mv s1, a2
    mv s2, a3       

    addi  s0,sp,32              

    li a7, SYS_clone            # load SYS_clone
    ecall                       # syscall

    bnez a0, parent             # if pid != 0, go to parent

    # child
    mv a0, s2                  # move arg to a0 (argument register)
    jalr s1                   # call fn(arg)

    mv a0, a0                   # move return value (exit code) to a0
    li a7, SYS_exit_thread      # load SYS_exit_thread
    ecall                       # syscall

spin:
    j spin                      # error handling loop

parent:
    ld ra, 24(sp)               # restore ra
    ld s0, 16(sp)               # restore s0
    ld s1, 8(sp)                # restore s1
    ld s2, 0(sp)                # restore s2
    addi sp, sp, 8*4            # deallocate stack space
    jr ra                       # return to caller
