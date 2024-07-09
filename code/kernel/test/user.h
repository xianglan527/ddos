#ifndef __TEST_USER_H__
#define __TEST_USER_H__

int write(int, const void*, int);
int sti();
int cli();
int getpid(void);
int fork(void);
int exit(int) __attribute__((noreturn));
int waitpid(int, int*);
int yield(void);

int wait(void);

#endif