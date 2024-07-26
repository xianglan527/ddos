#include "user.h"
#include "printf.h"
#include "assert.h"
#include "panic.h"
#include "usertests.h"

int main(void){
    printf("welcome user space\n");
    test_main();
    return 0;
}