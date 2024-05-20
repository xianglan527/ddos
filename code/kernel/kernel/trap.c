#include "trap.h"
#include "risv.h"
#include "stdio.h"

void kernelvec();

void trap_init_hart(void){
    w_stvec((uint64_t)kernelvec);
}

void kerneltrap(){
     cprintf("get here kerneltrap\n");
}