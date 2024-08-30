#include "assert.h"
#include "panic.h"
#include "user.h"
#include "malloc.h"
#include "lock.h"
#include "sysdef.h"

extern void lock_fork(void);
extern void unlock_fork(void);

union header{
    struct{
        union header *ptr;
        size_t size;
        bool type;
    }s;
    uint64_t align[8];
};

typedef union header Header;

static Header base;
static Header *freep = nullptr;

static lock_t mem_lock = INIT_LOCK;

static void free_locked(void *ap);

static inline void lock_malloc(void){
    lock_fork();
    lock(&mem_lock);
}

static inline void unlock_malloc(void){
    unlock(&mem_lock);
    unlock_fork();
}

static bool morecore_brk_locked(size_t nu){
    static uintptr_t brk = 0;
    if(brk == 0){
        if(sbrk(&brk) != 0 || brk == 0)
            return false;
    }
    uintptr_t newbrk = brk + nu * sizeof(Header);
    if(sbrk(&newbrk) != 0 || newbrk <= brk)
        return false;
    Header *p = (Header *)brk;
    p->s.size = (newbrk - brk) / sizeof(Header);
    p->s.type = 0;
    free_locked((void *)(p + 1));
    brk = newbrk;
    return true;
}

static bool morecore_shmem_locked(size_t nu){
    size_t size = ((nu * sizeof(Header) + 0xfff) & (~0xfff));
    uintptr_t mem = 0;
    if(shmem(&mem, size, MMAP_WRITE) != 0 || mem == 0){
        return false;
    }
    Header *p = (Header *)mem;
    p->s.size = size / sizeof(Header);
    p->s.type = 1;
    free_locked((void *)(p + 1));
    return true;
}

static void *malloc_locked(size_t size, bool type){
    assert(sizeof(Header) == 0x40);
    Header *p, *prevp;
    size_t nunits;

    nunits = (size + sizeof(Header) + 1) / sizeof(Header) + 1;
    if((prevp = freep) == nullptr){
        base.s.ptr = freep = prevp = &base;
        base.s.size = 0;
    }
    for(p = prevp->s.ptr; ; prevp = p, p = p->s.ptr){
        if(p->s.type == type && p->s.size >= nunits){
            if(p->s.size == nunits){
                prevp->s.ptr = p->s.ptr;
            }else{
                Header *np = prevp->s.ptr = (p + nunits);
                np->s.ptr = p->s.ptr;
                np->s.size = p->s.size - nunits;
                np->s.type = type;
                p->s.size = nunits;
            }
            freep = prevp;
            return (void *)(p + 1);
        }
        if(p == freep){
            bool (*morecore_locked)(size_t nu);
            morecore_locked = (!type)? morecore_brk_locked : morecore_shmem_locked;
            if(!morecore_locked(nunits))
                return nullptr;
        }
    }
}

void free_locked(void *ap){
    Header *bp = ((Header *)ap) - 1, *p;
    for(p = freep; !(bp >p && bp < p->s.ptr); p = p->s.ptr){
        if(p >= p->s.ptr && (bp > p || bp < p->s.ptr))
            break;
    }
    if (bp->s.type == p->s.ptr->s.type && bp + bp->s.size == p->s.ptr) {
        bp->s.size += p->s.ptr->s.size;
        bp->s.ptr = p->s.ptr->s.ptr;
    } else {
        bp->s.ptr = p->s.ptr;
    }

    if (p->s.type == bp->s.type && p + p->s.size == bp) {
        p->s.size += bp->s.size;
        p->s.ptr = bp->s.ptr;
    } else {
        p->s.ptr = bp;
    }
    freep = p;
}

void *malloc(size_t size){
    void *ret;
    lock_malloc();
    ret = malloc_locked(size, 0);
    unlock_malloc();
    return ret;
}

void *shmem_malloc(size_t size) {
    void *ret;
    lock_malloc();
    ret = malloc_locked(size, 1);
    unlock_malloc();
    return ret;
}

void free(void *ap) {
    lock_malloc();
    free_locked(ap);
    unlock_malloc();
}
