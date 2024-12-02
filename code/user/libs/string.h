#ifndef __LIBS_STRING_H__
#define __LIBS_STRING_H__

#include "types.h"

void *memset(void *dst, int c, size_t n);
int memcmp(const void *v1, const void *v2, size_t n);
void *memmove(void *dst, const void *src, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
int strcmp(const char *p, const char *q);
int strncmp(const char *p, const char *q, size_t n);
char *strcpy(char *dst, const char *src);
char *strncpy(char *s, const char *t, size_t n);
char *safestrcpy(char *s, const char *t, size_t n);
size_t strlen(const char *s);
size_t strnlen(const char *s, size_t len);
char *strchr(const char *s, int c);
char *strfind(const char *s, int c);
long strtol(const char *s, char **endptr, int base);
char *strcat(char *dst, const char *src);
int atoi(const char *s);
#endif