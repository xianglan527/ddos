#ifndef __LIBS_RAND_H__
#define __LIBS_RAND_H__

#include "types.h"
#define RAND_MAX 2147483647UL  // 通常定义为 2^31 - 1


long simulate_rand(void);
void srand(unsigned long seed);
#endif