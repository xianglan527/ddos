#ifndef __TEST_USER_H__
#define __TEST_USER_H__
#include "types.h"
int write(int, const void *, int);
int sti();
int cli();
int getpid(void);
int fork(void);
int exit(int) __attribute__((noreturn));
int waitpid(int, int *);
int yield(void);
int exec(char *path, char **argv);
int kill(int);
int sbrk(uintptr_t *);
int sleep(ulong);
uint64_t gettime(void);

int wait(void);
void putc(int fd, char c);
void puts(int fd, char *str);
#endif