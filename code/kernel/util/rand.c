#include "rand.h"



static unsigned long next = 1;

/* *
 * srand - seed the random number generator with the given number
 * @seed:   the required seed number
 * */
void srand(unsigned long seed) { next = seed; }

/* *
 * rand - returns a pseudo-random integer
 *
 * The rand() function return a value in the range [0, RAND_MAX].
 * */
long rand(void) {
    next = next * 1103515245 + 12345;
    return (long)((next / 65536) % (RAND_MAX + 1));
}
