#include "shmem.h"

#include "error.h"
#include "pmm.h"
#include "shmem.h"
#include "slab.h"
#include "string.h"
#include "swap.h"

Shmem_struct *shmem_create(size_t len) {
    Shmem_struct *shmem = kmalloc(sizeof(*shmem));
    if (shmem != nullptr) {
        list_init(&shmem->shmn_list);
        shmem->len = len;
        shmem->shmem_page_size = (len + (PAGE_SHMN_LENS - 1)) / PAGE_SHMN_LENS;
        Page *page = alloc_pages(shmem->shmem_page_size);
        assert(page != nullptr);
        shmem->entry = (pte_t *)page2kva(page);
        set_shmem_ref(shmem, 0);
        initlock(&shmem->shmem_lock, "shmem_lock");
    }
    return shmem;
}

static inline void shmem_remove_entry_pte(pte_t *ptep) {
    assert(ptep != nullptr);
    if (*ptep & PTE_V) {
        Page *page = pte2page(*ptep);
        if (!PageSwap(page)) {
            if (page_ref_dec(page) == 0) { FreePage(page); }
        } else {
            if (*ptep & PTE_D) { SetPageDirty(page); }
            page_ref_dec(page);
        }
        *ptep = 0;
    } else if (*ptep != 0) {
        swap_remove_entry(*ptep);
        *ptep = 0;
    }
}

void shmem_destroy(Shmem_struct *shmem) {
    for (size_t i = 0; i < SHMN_NENTRY * shmem->shmem_page_size; i++)
        shmem_remove_entry_pte(shmem->entry + i);
    free_pages(kva2page((uintptr_t)shmem->entry), shmem->shmem_page_size);
    kfree(shmem);
}

pte_t *shmem_get_entry(Shmem_struct *shmem, uintptr_t addr, bool create) {
    assert(addr < shmem->len);
    addr = PGROUNDDOWN(addr);
    uintptr_t start = ROUNDDOWN(addr, shmem->shmem_page_size * PAGE_SHMN_LENS);
    size_t index = (addr - start) / PGSIZE;
    if (shmem->entry[index] == 0) {
        if (create) {
            Page *page = AllocPage();
            if (page != nullptr) {
                shmem->entry[index] = (PA2PTE(page2pa(page)) | PTE_V);
                page_ref_inc(page);
            }
        }
    }
    return shmem->entry + index;
}

int shmem_insert_entry(Shmem_struct *shmem, uintptr_t addr, pte_t entry) {
    pte_t *ptep = shmem_get_entry(shmem, addr, 1);
    if (ptep == nullptr) return -E_NO_MEM;
    if (*ptep != 0) shmem_remove_entry_pte(ptep);
    if (entry & PTE_V)
        page_ref_inc(pte2page(entry));
    else if (entry != 0)
        swap_duplicate(entry);
    *ptep = entry;
    return 0;
}

int shmem_remove_entry(Shmem_struct *shmem, uintptr_t addr) { return shmem_insert_entry(shmem, addr, 0); }
