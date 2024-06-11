#ifndef __UTIL_RAND_H__
#define __UTIL_RAND_H__

#include "types.h"
#define RAND_MAX 2147483647l  // 通常定义为 2^31 - 1

void srand(unsigned long seed);
long rand(void);
#endif