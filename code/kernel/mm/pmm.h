#ifndef __MM_PMM_H__
#define __MM_PMM_H__
#include "assert.h"
#include "atomic.h"
#include "list.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "stdarg.h"
#include "types.h"
#include "swap.h"



extern char kernel_end[];
typedef struct page Page;
struct page {
    Atomic ref;
    uint64_t flags;
    List_entry page_link;
    uint property;
    int zone_num;
    swap_entry_t index;
    List_entry swap_link;
};

#define PG_RESERVED 0
#define PG_PROPERTY 1
#define PG_SLAB     2
#define PG_DIRTY    3
#define PG_SWAP     4
#define PG_ACTIVE   5

#define SetPageReserved(page) set_bit(PG_RESERVED, &(page)->flags)
#define ClearPageReserved(page) clear_bit(PG_RESERVED, &(page)->flags)
#define PageReserved(page) test_bit(PG_RESERVED, &(page)->flags)

#define SetPageProperty(page) set_bit(PG_PROPERTY, &(page)->flags)
#define ClearPageProperty(page) clear_bit(PG_PROPERTY, &(page)->flags)
#define PageProperty(page) test_bit(PG_PROPERTY, &(page)->flags)

#define SetPageSlab(page) set_bit(PG_SLAB, &(page)->flags)
#define ClearPageSlab(page) clear_bit(PG_SLAB, &(page)->flags)
#define PageSlab(page) test_bit(PG_SLAB, &(page)->flags)

#define SetPageDirty(page) set_bit(PG_DIRTY, &((page)->flags))
#define ClearPageDirty(page) clear_bit(PG_DIRTY, &((page)->flags))
#define PageDirty(page) test_bit(PG_DIRTY, &((page)->flags))

#define SetPageSwap(page) set_bit(PG_SWAP, &((page)->flags))
#define ClearPageSwap(page) clear_bit(PG_SWAP, &((page)->flags))
#define PageSwap(page) test_bit(PG_SWAP, &((page)->flags))

#define SetPageActive(page) set_bit(PG_ACTIVE, &((page)->flags))
#define ClearPageActive(page) clear_bit(PG_ACTIVE, &((page)->flags))
#define PageActive(page) test_bit(PG_ACTIVE, &((page)->flags))

#define le2page(le, member) to_struct((le), Page, member)

typedef struct free_area Free_area;
struct free_area {
    List_entry free_list;
    size_t nr_free;
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
extern pagetable_t *kernel_pagetable;

static inline long page_ref(Page *page) { return atomic_read(&page->ref); }

static inline void set_page_ref(Page *page, int val) { atomic_set(&page->ref, val); }

static inline long page_ref_inc(Page *page) { return atomic_add_return(&page->ref, 1); }

static inline long page_ref_dec(Page *page) { return atomic_sub_return(&page->ref, 1); }

void free_pagetable(pagetable_t *pagetable, int level);

#define AllocPage() alloc_pages(1)
#define FreePage(page) free_pages(page, 1)
#define FreePagetable(pagetable) free_pagetable(pagetable, 1);

extern Page *pages;
extern size_t npage;

#define MAXVA (1L << (9 + 9 + 9 + 12 - 1))

#define PTE_V (1L << 0)
#define PTE_R (1L << 1)
#define PTE_W (1L << 2 | PTE_R)
#define PTE_X (1L << 3)
#define PTE_U (1L << 4)
#define PTE_G (1L << 5)
#define PTE_A (1L << 6)
#define PTE_D (1L << 7)
#define PTE_MASK ((1L << 6) - 1)
#define PTE_PW (1L << 2)

#define PTE_USER    (PTE_V | PTE_W | PTE_U | PTE_X)
#define PTE_SWAP    (PTE_V | PTE_A | PTE_D)

#define PA2PTE(pa) ((((uint64_t)(pa)) >> 12) << 10)

#define PTE2PA(pte) (((pte) >> 10) << 12)

#define PTE_FLAGS(pte) ((pte) & 0x3FF)

#define PGOFF(pa) (((uintptr_t)(pa)) & 0xFFF)

#define PXMASK 0x1FF
#define PXSHIFT(level) (PGSHIFT + (9 * (level)))
#define PX(level, va) ((((uint64_t)(va)) >> PXSHIFT(level)) & PXMASK)

#define PTNUM 512UL
#define PT1PGSIZE (PTNUM * PTNUM * PGSIZE)
#define PT2PGSIZE (PTNUM * PGSIZE)
#define PT3PGSIZE PGSIZE


#define PAGE_START (PGROUNDUP((uintptr_t)kernel_end))
// #define PPN_START (PAGE_START >> PGSHIFT)

#define PPN(la) (((uintptr_t)((la) - PAGE_START)) >> PGSHIFT)

typedef enum vm_print_enum Vm_print_enum;
enum vm_print_enum {
    VM_PRINT_OUT,    // the state when the three-level page table entry is not yet encountered as valid.
    VM_PRINT_START,  // the state encountered when the three-level page table entry is valid.
    VM_PRINT_IN,     // the state when in contiguous virtual space (with identical PTE attributes)
};

void pmm_init(void);
Page *alloc_pages(size_t n);
void free_pages(Page *base, size_t n);
size_t nr_free_pages(void);
void kvmmap(uint64_t va, uint64_t pa, uint64_t sz, int perm);
void kvm_init_hart();
pagetable_t *alloc_pagetable();
pte_t *get_pte(pagetable_t *pagetable, uint64_t va, int alloc);
void uvm_unmap(pagetable_t *pagetable, uint64_t va, uint64_t npages, int free_page);
void uvm_free(pagetable_t *pagetable, uint64_t sz);
void page_remove(pagetable_t *pagetable, uintptr_t va);
int page_insert(pagetable_t *pagetable, Page *page, uintptr_t va, uint32_t perm);
int pages_insert(pagetable_t *pagetable, Page *pages, uintptr_t va, uint32_t perm, int pages_num);
void print_va2pa(pagetable_t *pagetable, uint64_t va);
void vm_map_print(pagetable_t *pagetable);
void vm_pte_print(pagetable_t *pagetable);
void vm_print(pagetable_t *pagetable);
void mappages(pagetable_t *pagetable, uint64_t va, uint64_t size, uint64_t pa, int perm);
void copy_kernel2user(pagetable_t *pagetable, uint64_t dstva, char *src, uint64_t len);
void copy_user2kernel(pagetable_t *pagetable, char *dst, uint64_t srcva, uint64_t len);
int copystr_user2kernel(pagetable_t *pagetable, char *dst, uint64_t srcva, uint64_t max);
void init_kernel_pagetable(void);
Page *pagetable_alloc_page(pagetable_t *pagetable, uintptr_t va, uint32_t perm);
uintptr_t va2pa(pagetable_t *pagetable, uint64_t va);
void unmap_range(pde_t *pgdir, uintptr_t start, uintptr_t end);
void unmap_range_without_free_page(pagetable_t *pagetable, uintptr_t start, uintptr_t end);
void exit_range(pde_t *pgdir, uintptr_t start, uintptr_t end);
int copy_range(pde_t *to, pde_t *from, uintptr_t start, uintptr_t end, bool share);

static inline ppn_t page2ppn(Page *page) { return page - pages; }

static inline uintptr_t page2pa(Page *page) { return (page2ppn(page) << PGSHIFT) + PAGE_START; }
// cause kernel virtual address equal physical address see:
// kvmmap(KERNBASE, KERNBASE, (uint64_t)kernel_etext - KERNBASE, PTE_R |
// PTE_X);kvmmap((uint64_t)kernel_etext, (uint64_t)kernel_etext, PHYSTOP - (uint64_t)kernel_etext, PTE_R |
// PTE_W);
static inline uintptr_t page2kva(Page *page) { return page2pa(page); }

static inline Page *pa2page(uintptr_t pa) {
    if (PPN(pa) >= npage) 
        panic("pa2page called with invalid pa");
    return &pages[PPN(pa)];
}

static inline Page *pte2page(pte_t pte) {
    if (!(pte & PTE_V)) { panic("pte2page called with invalid pte"); }
    return pa2page(PTE2PA(pte));
}

static inline Page *kva2page(uintptr_t va) {
    pte_t *pte = get_pte(kernel_pagetable, va, 0);
    return pte2page(*pte);
}

static inline void tlb_invalidate(pagetable_t *pagetable, uintptr_t va) {
    if (r_satp() == MAKE_SATP(pagetable)) sfence_vma_addr((void *)va);
}
#endif