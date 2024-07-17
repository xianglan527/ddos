#include "rand.h"
/* *
 * rand - returns a pseudo-random integer
 *
 * The rand() function return a value in the range [0, RAND_MAX].
 * */

static unsigned long next = 1;

void srand(unsigned long seed) { next = seed; }

long simulate_rand(void) {
    next = next * 1103515245 + 12345;
    return (long)((next / 65536) % (RAND_MAX + 1));
}

