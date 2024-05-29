#ifndef __MM_PMM_H__
#define __MM_PMM_H__
#include "assert.h"
#include "atomic.h"
#include "list.h"
#include "memlayout.h"
#include "stdarg.h"
#include "types.h"
#include "spinlock.h"

typedef struct page Page;
struct page {
    Atomic ref;
    uint64_t flags;
    List_entry page_link;
};

#define PG_RESERVED 0

#define SetPageReserved(page) set_bit(PG_RESERVED, &(page)->flags)
#define ClearPageReserved(page) clear_bit(PG_RESERVED, &(page)->flags)
#define PageReserved(page) test_bit(PG_RESERVED, &(page)->flags)

#define le2page(le, member)     \
        to_struct((le), Page, member)

typedef struct free_area Free_area;
struct free_area{
    List_entry free_list;
    size_t nr_free;
    Spinlock lock;
};

typedef struct pmm_manager Pmm_manager;
struct pmm_manager {
    const char *name;
    void (*init)(void);
    void (*init_memmap)(Page *base, size_t n);
    Page *(*alloc_pages)(size_t n);
    void (*free_pages)(Page *base, size_t n);
    size_t (*nr_free_pages)(void);
    void (*check)(void);
};

extern const Pmm_manager *pmm_manager;

static inline int page_ref(Page *page){
    return atomic_read(&page->ref);
}

static inline void set_page_ref(Page *page, int val){
    atomic_set(&page->ref, val);
}

static inline int page_ref_inc(Page *page){
    return atomic_add_return(&page->ref, 1);
}

static inline int page_ref_dec(Page *page){
    return atomic_sub_return(&page->ref, 1);
}

#define AllocPage() alloc_pages(1)
#define FreePage(page) free_pages(page, 1)

extern Page *pages;
extern size_t npage;

#define PPN_START (KERNBASE >> PGSHIFT)
#define PPN(la) ((((uintptr_t)(la)) >> PGSHIFT) - PPN_START)

static inline ppn_t page2ppn(Page *page){
    return page - pages;
}

static inline uintptr_t page2pa(Page *page){
    return page2ppn(page) << PGSHIFT;
}

static inline Page *pa2page(uintptr_t pa){
    ulong pp = PPN(pa);
    ulong ff = PPN_START;
    if(PPN(pa) >= npage)
        panic("pa2page called with invalid pa");
    return &pages[PPN(pa)];
}

void pmm_init(void);
Page *alloc_pages(size_t n);
void free_pages(Page *base, size_t n);
#endif