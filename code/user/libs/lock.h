#ifndef __LIBS_LOCK_H__
#define __LIBS_LOCK_H__

#include "stdarg.h"
#include "types.h"
#include "user.h"

#define INIT_LOCK   0

typedef volatile bool lock_t;

static inline void lock_init(lock_t *l){
    *l = 0;
}

static inline bool try_lock(lock_t *l){
    return __sync_lock_test_and_set(l, 1);
}

static inline void lock(lock_t *l){
    if(try_lock(l)){
        int step = 0;
        do{
            yield();
            if(++step == 100){
                step = 0;
                sleep(10);
            }
        }while(try_lock(l));
    }
    __sync_synchronize();
}

static inline void unlock(lock_t *l){
    __sync_synchronize();
    __sync_lock_release(l);
}

#endif
