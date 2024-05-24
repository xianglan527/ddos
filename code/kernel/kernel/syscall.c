#include "syscall.h"
#include "stdio.h"
#include "proc.h"
#include "config.h"

void syscall(void){
    Proc *p = myproc();
    cprintf("%s running \n", p->name);
    task_delay(DELAY);
}