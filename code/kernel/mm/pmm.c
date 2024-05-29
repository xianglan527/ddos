#include "pmm.h"
#include "stdio.h"
#include "defaultPmm.h"

const Pmm_manager *pmm_manager;

Page *pages;

size_t npage = 0;

extern char end[];

// extern const struct pmm_manager default_pmm_manager;

static void init_pmm_manager(void){
    pmm_manager = &default_pmm_manager;
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}

static void init_memmap(Page *base, size_t n){
    pmm_manager->init_memmap(base, n);
}

static void page_init(void){
    npage = PHYMEMSIZE / PGSIZE;
    pages = (Page *)ROUNDUP((void *)end, PGSIZE);
    for(int i = 0; i < npage; i++)
        SetPageReserved(pages + i);
    
    uintptr_t freemem = ROUNDUP((uintptr_t)pages + sizeof(Page) *npage, PGSIZE);
    uintptr_t endmem = ROUNDDOWN(PHYSTOP, PGSIZE);
    assert(freemem <= endmem);
    init_memmap(pa2page(freemem), (endmem - freemem) / PGSIZE);
}

Page *alloc_pages(size_t n){
    return pmm_manager->alloc_pages(n);
}

void free_pages(Page *base, size_t n){
    pmm_manager->free_pages(base, n);
}

static void check_alloc_page(void){
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}

void pmm_init(void){
    init_pmm_manager();
    page_init();
    check_alloc_page();
}
