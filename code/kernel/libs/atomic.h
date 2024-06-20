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

static inline void set_bit(int nr, volatile void *addr) __attribute__((always_inline));
static inline void clear_bit(int nr, volatile void *addr) __attribute__((always_inline));
static inline void change_bit(int nr, volatile void *addr) __attribute__((always_inline));
static inline bool test_and_set_bit(int nr, volatile void *addr) __attribute__((always_inline));
static inline bool test_and_clear_bit(int nr, volatile void *addr) __attribute__((always_inline));
static inline bool test_and_change_bit(int nr, volatile void *addr) __attribute__((always_inline));
static inline bool test_bit(int nr, volatile void *addr) __attribute__((always_inline));

/* *
 * atomic_read - read atomic variable
 * @v:  pointer of type Atomic
 *
 * Atomically reads the value of @v.
 * */
static inline long atomic_read(const Atomic *v) { return __atomic_load_n(&v->counter, __ATOMIC_SEQ_CST); }

/* *
 * atomic_set - set atomic variable
 * @v:  pointer of type Atomic
 * @i:  required value
 *
 * Atomically sets the value of @v to @i.
 * */
static inline void atomic_set(Atomic *v, long i) { __atomic_store_n(&v->counter, i, __ATOMIC_SEQ_CST); }

/* *
 * atomic_add - add integer to atomic variable
 * @v:  pointer of type Atomic
 * @i:  integer value to add
 *
 * Atomically adds @i to @v.
 * */
static inline void atomic_add(Atomic *v, long i) { __atomic_fetch_add(&v->counter, i, __ATOMIC_SEQ_CST); }

/* *
 * atomic_sub - subtract integer from atomic variable
 * @v:  pointer of type Atomic
 * @i:  integer value to subtract
 *
 * Atomically subtracts @i from @v.
 * */
static inline void atomic_sub(Atomic *v, long i) { __atomic_fetch_sub(&v->counter, i, __ATOMIC_SEQ_CST); }

/* *
 * atomic_sub_test_zero - subtract value from variable and test result
 * @v:  pointer of type Atomic
 * @i:  integer value to subtract
 *
 * Atomically subtracts @i from @v and
 * returns true if the result is zero, or false for all other cases.
 * */
static inline bool atomic_sub_test_zero(Atomic *v, long i) {
    return __atomic_sub_fetch(&v->counter, i, __ATOMIC_SEQ_CST) == 0;
}

/* *
 * atomic_inc - increment atomic variable
 * @v:  pointer of type Atomic
 *
 * Atomically increments @v by 1.
 * */
static inline void atomic_inc(Atomic *v) { __atomic_fetch_add(&v->counter, 1, __ATOMIC_SEQ_CST); }

/* *
 * atomic_dec - decrement atomic variable
 * @v:  pointer of type Atomic
 *
 * Atomically decrements @v by 1.
 * */
static inline void atomic_dec(Atomic *v) { __atomic_fetch_sub(&v->counter, 1, __ATOMIC_SEQ_CST); }

/* *
 * atomic_inc_test_zero - increment and test
 * @v:  pointer of type Atomic
 *
 * Atomically increments @v by 1 and
 * returns true if the result is zero, or false for all other cases.
 * */
static inline bool atomic_inc_test_zero(Atomic *v) {
    return __atomic_add_fetch(&v->counter, 1, __ATOMIC_SEQ_CST) == 0;
}

/* *
 * atomic_dec_test_zero - decrement and test
 * @v:  pointer of type Atomic
 *
 * Atomically decrements @v by 1 and
 * returns true if the result is 0, or false for all other cases.
 * */
static inline bool atomic_dec_test_zero(Atomic *v) {
    return __atomic_sub_fetch(&v->counter, 1, __ATOMIC_SEQ_CST) == 0;
}

/* *
 * atomic_add_return - add integer and return
 * @i:  integer value to add
 * @v:  pointer of type Atomic
 *
 * Atomically adds @i to @v and returns @i + @v
 * */
static inline long atomic_add_return(Atomic *v, long i) {
    return __atomic_add_fetch(&v->counter, i, __ATOMIC_SEQ_CST);
}

/* *
 * atomic_sub_return - subtract integer and return
 * @v:  pointer of type Atomic
 * @i:  integer value to subtract
 *
 * Atomically subtracts @i from @v and returns @v - @i
 * */
static inline long atomic_sub_return(Atomic *v, long i) {
    return __atomic_sub_fetch(&v->counter, i, __ATOMIC_SEQ_CST);
}

/* *
 * set_bit - Atomically set a bit in memory
 * @nr:     the bit to set
 * @addr:   the address to start counting from
 *
 * Note that @nr may be almost arbitrarily large; this function is not
 * restricted to acting on a single-word quantity.
 * */
static inline void set_bit(int nr, volatile void *addr) {
    __atomic_fetch_or((volatile unsigned long *)addr + (nr / (sizeof(unsigned long) * 8)),
                      1UL << (nr % (sizeof(unsigned long) * 8)), __ATOMIC_SEQ_CST);
}

/* *
 * clear_bit - Atomically clears a bit in memory
 * @nr:     the bit to clear
 * @addr:   the address to start counting from
 * */
static inline void clear_bit(int nr, volatile void *addr) {
    __atomic_fetch_and((volatile unsigned long *)addr + (nr / (sizeof(unsigned long) * 8)),
                       ~(1UL << (nr % (sizeof(unsigned long) * 8))), __ATOMIC_SEQ_CST);
}

/* *
 * change_bit - Atomically toggle a bit in memory
 * @nr:     the bit to change
 * @addr:   the address to start counting from
 * */
static inline void change_bit(int nr, volatile void *addr) {
    __atomic_fetch_xor((volatile unsigned long *)addr + (nr / (sizeof(unsigned long) * 8)),
                       1UL << (nr % (sizeof(unsigned long) * 8)), __ATOMIC_SEQ_CST);
}

/* *
 * test_and_set_bit - Atomically set a bit and return its old value
 * @nr:     the bit to set
 * @addr:   the address to count from
 * */
static inline bool test_and_set_bit(int nr, volatile void *addr) {
    unsigned long mask = 1UL << (nr % (sizeof(unsigned long) * 8));
    volatile unsigned long *p = (volatile unsigned long *)addr + (nr / (sizeof(unsigned long) * 8));
    return __atomic_fetch_or(p, mask, __ATOMIC_SEQ_CST) & mask;
}

/* *
 * test_and_clear_bit - Atomically clear a bit and return its old value
 * @nr:     the bit to clear
 * @addr:   the address to count from
 * */
static inline bool test_and_clear_bit(int nr, volatile void *addr) {
    unsigned long mask = 1UL << (nr % (sizeof(unsigned long) * 8));
    volatile unsigned long *p = (volatile unsigned long *)addr + (nr / (sizeof(unsigned long) * 8));
    return __atomic_fetch_and(p, ~mask, __ATOMIC_SEQ_CST) & mask;
}

/* *
 * test_and_change_bit - Atomically change a bit and return its old value
 * @nr:     the bit to change
 * @addr:   the address to count from
 * */
static inline bool test_and_change_bit(int nr, volatile void *addr) {
    unsigned long mask = 1UL << (nr % (sizeof(unsigned long) * 8));
    volatile unsigned long *p = (volatile unsigned long *)addr + (nr / (sizeof(unsigned long) * 8));
    return __atomic_fetch_xor(p, mask, __ATOMIC_SEQ_CST) & mask;
}

/* *
 * test_bit - Determine whether a bit is set
 * @nr:     the bit to test
 * @addr:   the address to count from
 * */
static inline bool test_bit(int nr, volatile void *addr) {
    unsigned long mask = 1UL << (nr % (sizeof(unsigned long) * 8));
    volatile unsigned long *p = (volatile unsigned long *)addr + (nr / (sizeof(unsigned long) * 8));
    return __atomic_load_n(p, __ATOMIC_SEQ_CST) & mask;
}

#endif /* !__LIBS_ATOMIC_H__ */
