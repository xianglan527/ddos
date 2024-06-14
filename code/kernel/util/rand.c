#include "rand.h"
#include "virtio-rng.h"
#include "stdio.h"
/* *
 * rand - returns a pseudo-random integer
 *
 * The rand() function return a value in the range [0, RAND_MAX].
 * */

long rand(void) {
    uint32_t buf[4] = {0};
    virtio_rng_read((uint8_t *)buf, sizeof(buf));
    uint64_t ret = ((uint64_t)buf[1] << 32 | buf[0]) %(RAND_MAX + 1);
    // cprintf("rand ret is %016ld\n", ret);
    return ret;
}


static unsigned long next = 1;

void srand(unsigned long seed) { next = seed; }

long simulate_rand(void) {
    next = next * 1103515245 + 12345;
    return (long)((next / 65536) % (RAND_MAX + 1));
}

void rand_test(void){
    for (int p = 0; p < 10; p++) { cprintf(" rand num is : %08d\n", rand()); }
}
