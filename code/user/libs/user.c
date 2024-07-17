#include "user.h"
#include "string.h"
int wait(void) {
    int ret;
    ret = waitpid(0, (void *)0); 
    return ret;
}
void putc(int fd, char c) { write(fd, &c, 1); };

void puts(int fd, char *str) {write(fd, str, strlen(str) + 1);}

