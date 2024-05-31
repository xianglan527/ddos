#ifndef __MM_PMM_H__
#define __MM_PMM_H__
#include "assert.h"
#include "atomic.h"
#include "list.h"
#include "memlayout.h"
#include "stdarg.h"
#include "types.h"
#include "spinlock.h"
#include "riscv.h"

extern char kernel_end[];
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
#define FreePagetable(pagetable) free_pagetable(pagetable, 0);

extern Page *pages;
extern size_t npage;

#define MAXVA (1L << (9 + 9 + 9 + 12 - 1))

#define PTE_V (1L << 0)
#define PTE_R (1L << 1)
#define PTE_W (1L << 2)
#define PTE_X (1L << 3)
#define PTE_U (1L << 4)
#define PTE_G (1L << 5)
#define PTE_A (1L << 6)
#define PTE_D (1L << 7)

#define PA2PTE(pa) ((((uint64_t)(pa)) >> 12) << 10)

#define PTE2PA(pte) (((pte) >> 10) << 12)

#define PTE_FLAGS(pte) ((pte) & 0x3FF)

#define PGOFF(pa) (((uintptr_t)(pa)) & 0xFFF)

#define PXMASK 0x1FF
#define PXSHIFT(level) (PGSHIFT + (9 * (level)))
#define PX(level, va) ((((uint64_t)(va)) >> PXSHIFT(level)) & PXMASK)

typedef uint64_t pte_t;
typedef uint64_t pde_t;
typedef uint64_t pagetable_t;

#define PAGE_START (PGROUNDUP((uintptr_t)kernel_end))
// #define PPN_START (PAGE_START >> PGSHIFT)

#define PPN(la) (((uintptr_t)((la) - PAGE_START)) >> PGSHIFT)

static inline ppn_t page2ppn(Page *page){
    return page - pages;
}

static inline uintptr_t page2pa(Page *page) { return (page2ppn(page) << PGSHIFT) + PAGE_START; }

static inline Page *pa2page(uintptr_t pa){
    if(PPN(pa) >= npage)
        panic("pa2page called with invalid pa");
    return &pages[PPN(pa)];
}

static inline Page *pte2page(pte_t pte) {
    if (!(pte & PTE_V)) { panic("pte2page called with invalid pte"); }
    return pa2page(PTE2PA(pte));
}

static inline void tlb_invalidate(pagetable_t *pagetable, uintptr_t va) {
    if (r_satp() == (uint64_t)pagetable) sfence_vma_addr((void *)va);
}

void pmm_init(void);
Page *alloc_pages(size_t n);
void free_pages(Page *base, size_t n);
#endif