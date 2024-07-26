#ifndef __UTIL_BITMAP_H__
#define __UTIL_BITMAP_H__
#include "types.h"
#include "atomic.h"
#include "spinlock.h"
#include "string.h"

#define BITMAP_MASK 1
struct bitmap {
    size_t nr;
    size_t btmp_bytes_len;
    volatile uint8_t* bits;
    Spinlock bitmap_lock;
};

struct bitmap* bitmap_init(size_t nr);
bool bitmap_scan_test(struct bitmap* btmp, size_t bit_idx);
long bitmap_scan_set(struct bitmap* btmp, size_t cnt);
void bitmap_set(struct bitmap* btmp, size_t bit_idx, long value);
void bitmap_scan_clear(struct bitmap* btmp, size_t bit_idx, size_t cnt);
#endif