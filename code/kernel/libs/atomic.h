#ifndef __LIBS_ATOMIC_H__
#define __LIBS_ATOMIC_H__

#include "stdarg.h"
#include "types.h"

/* Atomic operations that C can't guarantee us. Useful for resource counting etc.. */

typedef struct {
    volatile long counter;
} Atomic;

static inline long atomic_read(const Atomic *v) __attribute__((always_inline));
static inline void atomic_set(Atomic *v, long i) __attribute__((always_inline));
static inline void atomic_add(Atomic *v, long i) __attribute__((always_inline));
static inline void atomic_sub(Atomic *v, long i) __attribute__((always_inline));
static inline bool atomic_sub_test_zero(Atomic *v, long i) __attribute__((always_inline));
static inline void atomic_inc(Atomic *v) __attribute__((always_inline));
static inline void atomic_dec(Atomic *v) __attribute__((always_inline));
static inline bool atomic_inc_test_zero(Atomic *v) __attribute__((always_inline));
static inline bool atomic_dec_test_zero(Atomic *v) __attribute__((always_inline));
static inline long atomic_add_return(Atomic *v, long i) __attribute__((always_inline));
static inline long atomic_sub_return(Atomic *v, long i) __attribute__((always_inline));

static inline void set_bit(size_t nr, volatile void *addr) __attribute__((always_inline));
static inline void clear_bit(size_t nr, volatile void *addr) __attribute__((always_inline));
static inline void change_bit(size_t nr, volatile void *addr) __attribute__((always_inline));
static inline bool test_and_set_bit(size_t nr, volatile void *addr) __attribute__((always_inline));
static inline bool test_and_clear_bit(size_t nr, volatile void *addr) __attribute__((always_inline));
static inline bool test_and_change_bit(size_t nr, volatile void *addr) __attribute__((always_inline));
static inline bool test_bit(size_t nr, volatile void *addr) __attribute__((always_inline));


static inline long atomic_read(const Atomic *v) { return __atomic_load_n(&v->counter, __ATOMIC_SEQ_CST); }


static inline void atomic_set(Atomic *v, long i) { __atomic_store_n(&v->counter, i, __ATOMIC_SEQ_CST); }


static inline void atomic_add(Atomic *v, long i) { __atomic_fetch_add(&v->counter, i, __ATOMIC_SEQ_CST); }


static inline void atomic_sub(Atomic *v, long i) { __atomic_fetch_sub(&v->counter, i, __ATOMIC_SEQ_CST); }


static inline bool atomic_sub_test_zero(Atomic *v, long i) {
    return __atomic_sub_fetch(&v->counter, i, __ATOMIC_SEQ_CST) == 0;
}

static inline void atomic_inc(Atomic *v) { __atomic_fetch_add(&v->counter, 1, __ATOMIC_SEQ_CST); }


static inline void atomic_dec(Atomic *v) { __atomic_fetch_sub(&v->counter, 1, __ATOMIC_SEQ_CST); }


static inline bool atomic_inc_test_zero(Atomic *v) {
    return __atomic_add_fetch(&v->counter, 1, __ATOMIC_SEQ_CST) == 0;
}


static inline bool atomic_dec_test_zero(Atomic *v) {
    return __atomic_sub_fetch(&v->counter, 1, __ATOMIC_SEQ_CST) == 0;
}


static inline long atomic_add_return(Atomic *v, long i) {
    return __atomic_add_fetch(&v->counter, i, __ATOMIC_SEQ_CST);
}


static inline long atomic_sub_return(Atomic *v, long i) {
    return __atomic_sub_fetch(&v->counter, i, __ATOMIC_SEQ_CST);
}


static inline void set_bit(size_t nr, volatile void *addr) {
    __atomic_fetch_or((volatile unsigned long *)addr + (nr / (sizeof(unsigned long) * 8)),
                      1UL << (nr % (sizeof(unsigned long) * 8)), __ATOMIC_SEQ_CST);
}


static inline void clear_bit(size_t nr, volatile void *addr) {
    __atomic_fetch_and((volatile unsigned long *)addr + (nr / (sizeof(unsigned long) * 8)),
                       ~(1UL << (nr % (sizeof(unsigned long) * 8))), __ATOMIC_SEQ_CST);
}


static inline void change_bit(size_t nr, volatile void *addr) {
    __atomic_fetch_xor((volatile unsigned long *)addr + (nr / (sizeof(unsigned long) * 8)),
                       1UL << (nr % (sizeof(unsigned long) * 8)), __ATOMIC_SEQ_CST);
}


static inline bool test_and_set_bit(size_t nr, volatile void *addr) {
    unsigned long mask = 1UL << (nr % (sizeof(unsigned long) * 8));
    volatile unsigned long *p = (volatile unsigned long *)addr + (nr / (sizeof(unsigned long) * 8));
    return __atomic_fetch_or(p, mask, __ATOMIC_SEQ_CST) & mask;
}


static inline bool test_and_clear_bit(size_t nr, volatile void *addr) {
    unsigned long mask = 1UL << (nr % (sizeof(unsigned long) * 8));
    volatile unsigned long *p = (volatile unsigned long *)addr + (nr / (sizeof(unsigned long) * 8));
    return __atomic_fetch_and(p, ~mask, __ATOMIC_SEQ_CST) & mask;
}


static inline bool test_and_change_bit(size_t nr, volatile void *addr) {
    unsigned long mask = 1UL << (nr % (sizeof(unsigned long) * 8));
    volatile unsigned long *p = (volatile unsigned long *)addr + (nr / (sizeof(unsigned long) * 8));
    return __atomic_fetch_xor(p, mask, __ATOMIC_SEQ_CST) & mask;
}


static inline bool test_bit(size_t nr, volatile void *addr) {
    unsigned long mask = 1UL << (nr % (sizeof(unsigned long) * 8));
    volatile unsigned long *p = (volatile unsigned long *)addr + (nr / (sizeof(unsigned long) * 8));
    return __atomic_load_n(p, __ATOMIC_SEQ_CST) & mask;
}

#endif /* !__LIBS_ATOMIC_H__ */
