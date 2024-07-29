#include "assert.h"
#include "panic.h"
#include "user.h"
#include "malloc.h"

union header{
    struct{
        union header *ptr;
        size_t size;
    }s;
    uint64_t align[8];
};

typedef union header Header;

static Header base;
static Header *freep = nullptr;

static bool morecore(size_t nu){
    assert(sizeof(Header) == 0x40);
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
    free((void *)(p + 1));
    brk = newbrk;
    return true;
}

void *malloc(size_t size){
    Header *p, *prevp;
    size_t nunits;
    nunits = (size + sizeof(Header) + 1) / sizeof(Header) + 1;
    if((prevp = freep) == nullptr){
        base.s.ptr = freep = prevp = &base;
        base.s.size = 0;
    }
    for(p = prevp->s.ptr; ; prevp = p, p = p->s.ptr){
        if(p->s.size >= nunits){
            if(p->s.size == nunits){
                prevp->s.ptr = p->s.ptr;
            }else{
                Header *np = prevp->s.ptr = (p + nunits);
                np->s.ptr = p->s.ptr;
                np->s.size = p->s.size - nunits;
                p->s.size = nunits;
            }
            freep = prevp;
            return (void *)(p + 1);
        }
        if(p == freep){
            if(!morecore(nunits))
                return nullptr;
        }
    }
}

void free(void *ap){
    Header *bp = ((Header *)ap) - 1, *p;
    for(p = freep; !(bp >p && bp < p->s.ptr); p = p->s.ptr){
        if(p >= p->s.ptr && (bp > p || bp < p->s.ptr))
            break;
    }
    if(bp + bp->s.size != p->s.ptr){
        bp->s.ptr = p->s.ptr;
    }else{
        bp->s.size += p->s.ptr->s.size;
        bp->s.ptr = p->s.ptr->s.ptr;
    }
    if(p + p->s.size != bp){
        p->s.ptr = bp;
    }else{
        p->s.size += bp->s.size;
        p->s.ptr = bp->s.ptr;
    }
    freep = p;
}
