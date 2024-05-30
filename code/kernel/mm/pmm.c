#include "pmm.h"
#include "stdio.h"
#include "defaultPmm.h"
#include "string.h"
#include "error.h"

const Pmm_manager *pmm_manager;

Page *pages;

size_t npage = 0;

pagetable_t *kernel_pagetable;

static void init_pmm_manager(void){
    pmm_manager = &default_pmm_manager;
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}

static void init_memmap(Page *base, size_t n){
    pmm_manager->init_memmap(base, n);
}

static void page_init(void){
    npage = (PHYSTOP - PAGE_START) / PGSIZE;
    pages = (Page *)PAGE_START;
    for(int i = 0; i < npage; i++)
        SetPageReserved(pages + i);
    
    uintptr_t freemem = PGROUNDUP((uintptr_t)pages + sizeof(Page) *npage);
    uintptr_t endmem = PGROUNDDOWN(PHYSTOP);
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

pte_t *get_pte(pagetable_t *pagetable, uint64_t va, int alloc){
    if(va >= MAXVA)
        panic("get_pte");
    for(int level = 2; level > 0; level--){
        pte_t *pte = &pagetable[PX(level, va)];
        if(*pte & PTE_V){
            pagetable = (pagetable_t *)PTE2PA(*pte);
        }else{
            Page *page;
            if(!alloc || (page = AllocPage()) == 0)
                return nullptr;
            set_page_ref(page, 1);
            pagetable = (pde_t *)page2pa(page);
            memset(pagetable, 0, PGSIZE);
            *pte = PA2PTE(pagetable) | PTE_V;
            int i= 0;
        }
    }
    int qq = PX(0, va);
    pte_t *pte_pp = &pagetable[PX(0, va)];
    return &pagetable[PX(0, va)];
}

Page *get_page(pagetable_t *pagetable, uintptr_t va, pte_t **ptep_store){
    pte_t *ptep = get_pte(pagetable, va, 0);
    if(ptep_store != nullptr){
        *ptep_store = ptep;
    }
    if(ptep != nullptr && *ptep & PTE_V){
        return pa2page(*ptep);
    }
    return nullptr;
}


static void page_remove_pte(pagetable_t *pagetable, uintptr_t va, pte_t *ptep){
    if(*ptep & PTE_V){
        Page *page = pte2page(*ptep);
        if(page_ref_dec(page) == 0)
            FreePage(page);
        *ptep = 0;
        tlb_invalidate(pagetable, va);
    }
}

int page_insert(pagetable_t *pagetable, Page *page, uintptr_t va, uint32_t perm){
    pte_t *ptep = get_pte(pagetable, va, 1);
    if(ptep == nullptr)
        return -E_NO_MEM;
    page_ref_inc(page);
    if(*ptep & PTE_V){
        Page *p = pte2page(*ptep);
        if(p == page)
            page_ref_dec(page);
        else
            page_remove_pte(pagetable, va, ptep);
    }
    *ptep = PA2PTE(page2pa(page)) | PTE_V | perm;
    tlb_invalidate(pagetable, va);
    return 0;

}

void page_remove(pagetable_t *pagetable, uintptr_t va){
    pte_t *ptep = get_pte(pagetable, va, 0);
    if(ptep != nullptr)
        page_remove_pte(pagetable, va, ptep);
}

void free_pagetable(pagetable_t *pagetable, int level){
    if(level == 2) return;
    int pp = page_ref(pa2page((uintptr_t)pagetable));
    assert(page_ref(pa2page((uintptr_t)pagetable)) == 1);
    for(int i = 0; i < 512; i++){
        pte_t pte = pagetable[i];
        if(pte & PTE_V){
            uint64_t child = PTE2PA(pte);
            free_pagetable((pagetable_t *)child, level++);
        }
    }
    FreePage(pa2page((uintptr_t)pagetable));
}

static void check_pagetable(void) {
    assert(kernel_pagetable != nullptr && (uint64_t)PGOFF(kernel_pagetable) == 0);
    assert(get_page(kernel_pagetable, 0x0, nullptr) == nullptr);

    Page *p1, *p2;
    p1 = AllocPage();
    assert(page_insert(kernel_pagetable, p1, 0x0, 0) == 0);

    pte_t *ptep;
    assert((ptep = get_pte(kernel_pagetable, 0x0, 0)) != nullptr);
    assert(pa2page(PTE2PA(*ptep)) == p1);
    assert(page_ref(p1) == 1);

    p2 = AllocPage();
    assert(page_insert(kernel_pagetable, p2, PGSIZE, PTE_U | PTE_W) == 0);
    assert((ptep = get_pte(kernel_pagetable, PGSIZE, 0)) != nullptr);
    assert(*ptep & PTE_U);
    assert(*ptep & PTE_W);
    assert(page_ref(p2) == 1);

    assert(page_insert(kernel_pagetable, p1, PGSIZE, 0) == 0);
    assert(page_ref(p1) == 2);
    assert(page_ref(p2) == 0);
    assert((ptep = get_pte(kernel_pagetable, PGSIZE, 0)) != nullptr);
    assert(pa2page(PTE2PA(*ptep)) == p1);
    assert((*ptep & PTE_U) == 0);

    page_remove(kernel_pagetable, 0x0);
    assert(page_ref(p1) == 1);
    assert(page_ref(p2) == 0);

    page_remove(kernel_pagetable, PGSIZE);
    assert(page_ref(p1) == 0);
    assert(page_ref(p2) == 0);

    assert(page_ref(pa2page(PTE2PA(kernel_pagetable[0]))) == 1);
    FreePagetable(kernel_pagetable);
    cprintf("check_pagetable() succeeded!\n");
}

pagetable_t *alloc_pagetable(){
    pagetable_t *pagetable = (pagetable_t *)page2pa(AllocPage());
    memset(pagetable, 0, PGSIZE);
    page_ref_inc(pa2page((uintptr_t)pagetable));
    return pagetable;
}


void pmm_init(void){
    init_pmm_manager();
    page_init();
    check_alloc_page();
    kernel_pagetable = alloc_pagetable();
    check_pagetable();
}
