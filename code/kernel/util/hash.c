#include "hash.h"

/* 2^63 + 2^61 - 2^46 + 2^31 - 2^28 - 2^16 + 1 */
#define GOLDEN_RATIO_PRIME_64 0x9e37fffffffc0001UL

uint64_t hash64(uint64_t val, unsigned int bits) {
    uint64_t hash = val * GOLDEN_RATIO_PRIME_64;
    return (hash >> (64 - bits));
}
