#include "pmm.h"
#include "config.h"
#include "defaultPmm.h"
#include "buddyPmm.h"
#include "error.h"
#include "stdio.h"
#include "string.h"
#include "swap.h"

const Pmm_manager *pmm_manager;

extern char kernel_etext[];
extern char trampoline[];


Page *pages;

size_t npage = 0;

pagetable_t *kernel_pagetable;

static void page_remove_pte(pagetable_t *pagetable, uintptr_t va, pte_t *ptep);

static void __vm_pte_print(pagetable_t *pagetable, int level) {
    if (level > 3) return;
    for (int i = 0; i < 512; i++) {
        pte_t pte = pagetable[i];
        if(pte != 0){
            if (pte & PTE_V) {
                int64_t child = PTE2PA(pte);
                for (int j = 0; j < level; j++) cprintf(" ..");
                cprintf("%d: pte %p pa %p\n", i, pte, child);
                __vm_pte_print((pagetable_t *)child, level + 1);
            }
            else{
                assert(level == 3);
                for (int j = 0; j < level; j++) cprintf(" ..");
                cprintf("%d: pte %p entry %p\n", i, pte, swap_offset(pte));
            }
        }
    
    }
}
void vm_pte_print(pagetable_t *pagetable) {
    cprintf("page pte table %p\n", pagetable);
    __vm_pte_print(pagetable, 1);
    return;
}


static const char *perm2str(pte_t perm) {
    static char str[7];
    str[0] = (perm & PTE_V) ? 'V' : '-';
    str[1] = (perm & PTE_R) ? 'R' : '-';
    str[2] = (perm & PTE_W) ? 'W' : '-';
    str[3] = (perm & PTE_X) ? 'X' : '-';
    str[4] = (perm & PTE_U) ? 'U' : '-';
    str[5] = (perm & PTE_G) ? 'G' : '-';
    str[6] = '\0';
    return str;
}

static void __vmprint_map(pagetable_t *pagetable, int level, int pg_index[], size_t *index) {
    if (level > 3) return;
    for (int i = 0; i < 512; i++) {
        pte_t pte = pagetable[i];
        if(pte != 0){
            pg_index[level - 1] = i;
            if (pte & PTE_V) {
                int64_t child = PTE2PA(pte);
                if (level == 3)
                    cprintf("%ld: pte %p perm %s va %p -----> pa %p\n", (*index)++, pte, perm2str(pte),
                            pg_index[0] * PT1PGSIZE + pg_index[1] * PT2PGSIZE + pg_index[2] * PT3PGSIZE,
                            child);
                __vmprint_map((pagetable_t *)child, level + 1, pg_index, index);
            }
            else{
                assert(level == 3);
                cprintf("%ld: pte %p perm %s va %p -----> entry %p\n", (*index)++, pte, perm2str(pte),
                            pg_index[0] * PT1PGSIZE + pg_index[1] * PT2PGSIZE + pg_index[2] * PT3PGSIZE,
                            swap_offset(pte));   
            }
        }
        
    }
}

void vm_map_print(pagetable_t *pagetable) {
    cprintf("page map table %p\n", pagetable);
    int pg_index[3];
    size_t index = 0;
    __vmprint_map(pagetable, 1, pg_index, &index);
    return;
}

static Vm_print_enum vm_print_state;
static pte_t cur_perm;
static int cur_pg_index[3];

static void __vm_print(pagetable_t *pagetable, int level, int pg_index[2][3], size_t *index) {
    if (level > 3) return;
    for (int i = 0; i < 512; i++) {
        pte_t pte = pagetable[i];
        if (pte & PTE_V) {
            int64_t child = PTE2PA(pte);
            if (vm_print_state == VM_PRINT_OUT) {
                pg_index[0][level - 1] = i;
                vm_print_state = VM_PRINT_START;
            }
            else if (vm_print_state == VM_PRINT_START) {
                pg_index[0][level - 1] = i;
            } 
            else if (vm_print_state == VM_PRINT_IN) {
                pg_index[1][level - 1] = i;
            }
            if (vm_print_state == VM_PRINT_START && level == 3) {
                cprintf("%ld: pte %p perm %s va_start %p -----> pa %p\n", (*index)++, pte, perm2str(pte),
                        pg_index[0][0] * PT1PGSIZE + pg_index[0][1] * PT2PGSIZE + pg_index[0][2] * PT3PGSIZE,
                        child);
                vm_print_state = VM_PRINT_IN;
                cur_pg_index[0] = pg_index[1][0] = pg_index[0][0];
                cur_pg_index[1] = pg_index[1][1] = pg_index[0][1];
                cur_pg_index[2] = pg_index[1][2] = pg_index[0][2];
                cur_perm = pte;
            } else if (vm_print_state == VM_PRINT_IN && level == 3) {
                uint64_t new_addr = pg_index[1][0] * PTNUM * PTNUM + pg_index[1][1] * PTNUM + pg_index[1][2];
                uint64_t old_addr =
                    cur_pg_index[0] * PTNUM * PTNUM + cur_pg_index[1] * PTNUM + cur_pg_index[2];

                if (new_addr == PTNUM * PTNUM * PTNUM + PTNUM * PTNUM + PTNUM &&
                    (pte & PTE_MASK) == (cur_perm & PTE_MASK)) {
                    cprintf("%ld: pte %p perm %s va_end %p -----> pa %p\n\n\n", (*index)++, pte,
                            perm2str(pte), PTNUM * PT1PGSIZE + PTNUM * PT2PGSIZE + PTNUM * PT3PGSIZE, child);
                    return;
                }
                if (new_addr - old_addr == 1 && (pte & PTE_MASK) == (cur_perm & PTE_MASK)) {
                    cur_pg_index[0] = pg_index[1][0];
                    cur_pg_index[1] = pg_index[1][1];
                    cur_pg_index[2] = pg_index[1][2];
                    cur_perm = pte;
                } else {
                    cprintf("%ld: pte %p perm %s va_end %p -----> pa %p\n\n\n", (*index)++, cur_perm,
                            perm2str(cur_perm),
                            cur_pg_index[0] * PT1PGSIZE + cur_pg_index[1] * PT2PGSIZE +
                                cur_pg_index[2] * PT3PGSIZE,
                            PTE2PA(cur_perm));

                    cprintf(
                        "%ld: pte %p perm %s va_start %p -----> pa %p\n", (*index)++, pte, perm2str(pte),
                        pg_index[1][0] * PT1PGSIZE + pg_index[1][1] * PT2PGSIZE + pg_index[1][2] * PT3PGSIZE,
                        child);
                    vm_print_state = VM_PRINT_IN;
                    cur_pg_index[0] = pg_index[1][0];
                    cur_pg_index[1] = pg_index[1][1];
                    cur_pg_index[2] = pg_index[1][2];
                    cur_perm = pte;
                }
            }
            __vm_print((pagetable_t *)child, level + 1, pg_index, index);
        } else {
            if (vm_print_state == VM_PRINT_IN) {
                cprintf(
                    "%ld: pte %p perm %s va_end %p -----> pa %p\n\n", (*index)++, cur_perm,
                    perm2str(cur_perm),
                    cur_pg_index[0] * PT1PGSIZE + cur_pg_index[1] * PT2PGSIZE + cur_pg_index[2] * PT3PGSIZE,
                    PTE2PA(cur_perm));
            }
            if(pte != 0){
                cprintf(
                    "%ld: pte %p perm %s va %p -----> entry %p\n", (*index)++, pte, perm2str(pte),
                    pg_index[0][0] * PT1PGSIZE + pg_index[0][1] * PT2PGSIZE + i * PT3PGSIZE,
                    swap_offset(pte));
            }
            vm_print_state = VM_PRINT_OUT;
        }
    }
}

void vm_print(pagetable_t *pagetable) {
    cprintf("page vm table %p\n", pagetable);
    int pg_index[2][3];
    memset(pg_index, 0, sizeof(pg_index));
    size_t index = 0;
    vm_print_state = VM_PRINT_OUT;
    cur_perm = 0;
    __vm_print(pagetable, 1, pg_index, &index);
    return;
}

static void init_pmm_manager(void) {
    // pmm_manager = &default_pmm_manager;
    pmm_manager = &buddy_pmm_manager;
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}

static void init_memmap(Page *base, size_t n) { pmm_manager->init_memmap(base, n); }

static void page_init(void) {
    npage = (PHYSTOP - PAGE_START) / PGSIZE;
    pages = (Page *)PAGE_START;
    for (int i = 0; i < npage; i++) SetPageReserved(pages + i);

    uintptr_t freemem = PGROUNDUP((uintptr_t)pages + sizeof(Page) * npage);
    uintptr_t endmem = PGROUNDDOWN(PHYSTOP);
    assert(freemem <= endmem);
    init_memmap(pa2page(freemem), (endmem - freemem) / PGSIZE);
}

Page *alloc_pages(size_t n) { 
    return pmm_manager->alloc_pages(n); 
}

void free_pages(Page *base, size_t n) {
    pmm_manager->free_pages(base, n);
}

size_t nr_free_pages(void){
    return pmm_manager->nr_free_pages();
}

void unmap_range(pagetable_t *pagetable, uintptr_t start, uintptr_t end){
    assert(start % PGSIZE == 0 && end % PGSIZE == 0);
    do{
        pte_t *ptep = get_pte(pagetable, start, 0);
        if(ptep == nullptr){
            start = ROUNDDOWN(start + PT2PGSIZE, PT2PGSIZE);
            continue;
        }
        if(*ptep != 0){
            page_remove_pte(pagetable, start, ptep);
        }
        start += PGSIZE;
    }while(start != 0 && start < end);
}

void exit_range(pagetable_t *pagetable, uintptr_t start, uintptr_t end){
    assert(start % PGSIZE == 0 && end % PGSIZE == 0);
    start = ROUNDDOWN(start, PT2PGSIZE);
    uintptr_t pstart = ROUNDDOWN(start, PT1PGSIZE);
    while (start < end){
        int pde_idx1 = PX(2, start);

        pte_t pgt2 = pagetable[pde_idx1];
        if (pgt2 == 0) {
            start = ROUNDDOWN(start + PT1PGSIZE, PT1PGSIZE);
            continue;
        }
        int pde_idx2 = PX(1, start);

        pte_t pgt3 = ((pte_t *)PTE2PA(pgt2))[pde_idx2];
        if(pgt3 == 0){
            start = ROUNDDOWN(start + PT2PGSIZE, PT2PGSIZE);
            continue;
        }

        bool pg3_empty = true;
        for(int i = 0; i < PTNUM; i++){
            if(start >= end)
                goto delete_pagetable2;
            if(pg3_empty == true && ((pte_t *)PTE2PA(pgt3))[PX(0, start)] != 0){
                pg3_empty = false;
            }
            start += PGSIZE;
        }
        if(pg3_empty){
            ((pte_t *)PTE2PA(pgt2))[pde_idx2] = 0;
            FreePage(pa2page(PTE2PA(pgt3)));
        } 
    }
delete_pagetable2:
    start = pstart;
    while (start < end) {
        int pde_idx1 = PX(2, start);

        pte_t pgt2 = pagetable[pde_idx1];
        if (pgt2 == 0) {
            start = ROUNDDOWN(start + PT1PGSIZE, PT1PGSIZE);
            continue;
        }
        int pde_idx2 = PX(1, start);

        bool pg2_empty = true;
        for (int i = 0; i < PTNUM; i++) {
            if (start >= end) return;
            if (pg2_empty == true && ((pte_t *)PTE2PA(pgt2))[PX(1, start)] != 0) {
                pg2_empty = false;
            }
            start = ROUNDDOWN(start + PT2PGSIZE, PT2PGSIZE);
        }
        if (pg2_empty) {
            pagetable[pde_idx1] = 0;
            FreePage(pa2page(PTE2PA(pgt2)));
        }
    }
}

int copy_range(pagetable_t *to, pagetable_t *from, uintptr_t start, uintptr_t end, bool share){
    share = 0;
    assert(start % PGSIZE == 0 && end % PGSIZE == 0);
    do{
        pte_t *ptep = get_pte(from, start, 0), *nptep;
        if(ptep == nullptr){
            start = ROUNDDOWN(start + PT2PGSIZE, PT2PGSIZE);
            continue;
        }
        if(*ptep != 0){
            if((nptep = get_pte(to, start, 1)) == nullptr){
                return -E_NO_MEM;
            }
            int ret;
            Page *page, *newpage = AllocPage();
            pte_t pp1 = *ptep;
            pte_t pp2 = *nptep;
            assert(*ptep != 0 && *nptep == 0);
            if(*ptep & PTE_V){
                uint32_t perm = (*ptep & PTE_USER);
                if((page = newpage) == nullptr)
                    return -E_NO_MEM;
                newpage = nullptr;
                memcpy((void *)page2va(page), (void *)page2va(pte2page(*ptep)), PGSIZE);
                ret = page_insert(to, page, start, perm);
                assert(ret == 0);
            }else{
                swap_entry_t entry;
                if(swap_copy_entry(*ptep, &entry) != 0){
                    return -E_NO_MEM;
                }
                swap_duplicate(entry);
                *nptep = entry;
            }
            if(newpage != nullptr){
                FreePage(newpage);
            }
        }
        start += PGSIZE;
    } while (start != 0 && start < end);
    return 0;
}

static void check_alloc_page(void) {
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}

pte_t *get_pte(pagetable_t *pagetable, uint64_t va, int alloc) {
    if (va >= MAXVA) panic("get_pte");
    for (int level = 2; level > 0; level--) {
        pte_t *pte = &pagetable[PX(level, va)];
        if (*pte & PTE_V) {
            pagetable = (pagetable_t *)PTE2PA(*pte);
        } else {
            Page *page;
            if (!alloc || (page = AllocPage()) == 0) return nullptr;
            set_page_ref(page, 1);
            pagetable = (pde_t *)page2pa(page);
            memset(pagetable, 0, PGSIZE);
            *pte = PA2PTE(pagetable) | PTE_V;
        }
    }
    return &pagetable[PX(0, va)];
}

uintptr_t va2pa(pagetable_t *pagetable, uint64_t va){
    pte_t *pte = get_pte(pagetable, va, 0);
    assert(pte != nullptr && (*pte & PTE_V));
    return (uintptr_t)PTE2PA(*pte) + (uintptr_t)PGOFF(va);
}

static uint64_t page_va2pa(pagetable_t *pagetable, uint64_t va){
    pte_t *pte;
    uint64_t pa;
    assert(va < MAXVA);
    pte = get_pte(pagetable, va, 0);
    assert((pte != nullptr) && (*pte & PTE_V));
    pa = PTE2PA(*pte);
    return pa;
}

void copy_from_user2kernel(pagetable_t *pagetable, char *dst, uint64_t srcva, uint64_t len){
    uint64_t n, va0, pa0;
    while(len > 0){
        va0 = PGROUNDDOWN(srcva);
        pa0 = page_va2pa(pagetable, va0);
        n = PGSIZE - (srcva - va0);
        if(n > len)
            n = len;
        memmove(dst, (void*)(pa0 + (srcva - va0)), n);
        len -= n;
        dst += n;
        srcva = va0 + PGSIZE;
    }
    return;
}

void print_va2pa(pagetable_t *pagetable, uint64_t va) {
    uint64_t pa = page_va2pa(pagetable, va);
    cprintf("va is -----0x%lx\n pa is -----0x%lx\n", va, pa + PGOFF(va));
}

Page *get_page(pagetable_t *pagetable, uintptr_t va, pte_t **ptep_store) {
    pte_t *ptep = get_pte(pagetable, va, 0);
    if (ptep_store != nullptr) { *ptep_store = ptep; }
    if (ptep != nullptr && *ptep & PTE_V) { return pa2page(*ptep); }
    return nullptr;
}

static void page_remove_pte(pagetable_t *pagetable, uintptr_t va, pte_t *ptep) {
    if (*ptep & PTE_V) {
        Page *page = pte2page(*ptep);
        if(!PageSwap(page)){
            if (page_ref_dec(page) == 0) FreePage(page);
        }
        else{
            if(*ptep & PTE_D)
                SetPageDirty(page);
            page_ref_dec(page);
        }
        *ptep = 0;
        tlb_invalidate(pagetable, va);
    }else if(*ptep != 0){
        swap_remove_entry(*ptep);
        *ptep = 0;
    }
}

int page_insert(pagetable_t *pagetable, Page *page, uintptr_t va, uint32_t perm) {
    pte_t *ptep = get_pte(pagetable, va, 1);
    if (ptep == nullptr) return -E_NO_MEM;
    page_ref_inc(page);
    if(*ptep != 0){
        if((*ptep & PTE_V) && pte2page(*ptep) == page){
           page_ref_dec(page); 
           goto out;
        }
        page_remove_pte(pagetable, va, ptep);
    }
out:
    *ptep = PA2PTE(page2pa(page)) | PTE_V | perm;
    tlb_invalidate(pagetable, va);
    return 0; 
}

void page_remove(pagetable_t *pagetable, uintptr_t va) {
    pte_t *ptep = get_pte(pagetable, va, 0);
    if (ptep != nullptr) page_remove_pte(pagetable, va, ptep);
}

Page *pagetable_alloc_page(pagetable_t *pagetable, uintptr_t va, uint32_t perm){
    Page *page = AllocPage();
    if(page != nullptr){
        if(page_insert(pagetable, page, va, perm) != 0){
            FreePage(page);
            return nullptr;
        }
    }
    return page;
}

void free_pagetable(pagetable_t *pagetable, int level) {
    if (level > 3) return;
    assert(page_ref(pa2page((uintptr_t)pagetable)) == 1);
    for (int i = 0; i < 512; i++) {
        pte_t pte = pagetable[i];
        if (pte & PTE_V) {
            uint64_t child = PTE2PA(pte);
            free_pagetable((pagetable_t *)child, level + 1);
        }
    }
    FreePage(pa2page((uintptr_t)pagetable));
}


static void check_pagetable(void) {
    size_t pnr = pmm_manager->nr_free_pages();
    kernel_pagetable = alloc_pagetable();
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
    // vm_map_print(kernel_pagetable);
    // vm_print(kernel_pagetable);
    // while(1);
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
    size_t nr = pmm_manager->nr_free_pages();
    assert(pnr = nr);
    cprintf("check_pagetable() succeeded!\n");
}

pagetable_t *alloc_pagetable() {
    pagetable_t *pagetable = (pagetable_t *)page2pa(AllocPage());
    if (pagetable == nullptr) return nullptr;
    memset(pagetable, 0, PGSIZE);
    page_ref_inc(pa2page((uintptr_t)pagetable));
    return pagetable;
}

void mappages(pagetable_t *pagetable, uint64_t va, uint64_t size, uint64_t pa, int perm) {
    uint64_t a, last;
    pte_t *pte;

    a = PGROUNDDOWN(va);
    last = PGROUNDDOWN(va + size - 1);
    while (1) {
        pte = get_pte(pagetable, a, 1);
        assert(pte != nullptr);
        assert((*pte & PTE_V) == 0);
        *pte = PA2PTE(pa) | perm | PTE_V;
        if (a == last) return;
        a += PGSIZE;
        pa += PGSIZE;
    }
}

void kvmmap(uint64_t va, uint64_t pa, uint64_t sz, int perm) { mappages(kernel_pagetable, va, sz, pa, perm); }

void uvm_unmap(pagetable_t *pagetable, uint64_t va, uint64_t npages, int free_page) {
    uint64_t a;
    pte_t *pte;
    assert((va % PGSIZE) != 0);
    for (a = va; a < va + npages * PGSIZE; a += PGSIZE) {
        assert((pte = get_pte(pagetable, a, 0)) != 0);
        assert(*pte & PTE_V);
        if (PTE_FLAGS(*pte) == PTE_V) panic("uvm_unmap: not a leaf");
        if (free_page) {
            Page *page = pte2page(*pte);
            if (page_ref_dec(page) == 0) FreePage(page);
        }
        *pte = 0;
    }
}

void uvm_free(pagetable_t *pagetable, uint64_t sz) {
    if (sz > 0) uvm_unmap(pagetable, 0, PGROUNDDOWN(sz) / PGSIZE, 1);
    FreePagetable(pagetable);
}

static void check_kernel_pagetable(void) {
    uintptr_t start = KERNBASE;
    pte_t *ptep;

    kernel_pagetable = alloc_pagetable();
    kvmmap(UART0, UART0, PGSIZE, PTE_R | PTE_W);
    kvmmap(VIRTIO_START_ADDR, VIRTIO_START_ADDR, PGSIZE, PTE_R | PTE_W);
    kvmmap(CLINT, CLINT, 0x10000, PTE_R | PTE_W);
    kvmmap(PLIC, PLIC, 0x400000, PTE_R | PTE_W);
    // kvmmap(KERNBASE, KERNBASE, PHYMEMSIZE, PTE_R | PTE_W);
    kvmmap(KERNBASE, KERNBASE, (uint64_t)kernel_etext - KERNBASE, PTE_R | PTE_X);

    kvmmap((uint64_t)kernel_etext, (uint64_t)kernel_etext, PHYSTOP - (uint64_t)kernel_etext, PTE_R | PTE_W);

    w_satp(MAKE_SATP(kernel_pagetable));
    sfence_vma();

    for (; start < PHYSTOP; start += PGSIZE) {
        ptep = get_pte(kernel_pagetable, start, 0);
        assert(PTE2PA(*ptep) == start);
    }

    Page *p;
    p = AllocPage();
    assert(page_insert(kernel_pagetable, p, 0x100, PTE_R | PTE_W) == 0);
    assert(page_ref(p) == 1);
    assert(page_insert(kernel_pagetable, p, 0x100 + PGSIZE, PTE_R | PTE_W) == 0);
    assert(page_ref(p) == 2);

    // vmprint(kernel_pagetable);

    const char *str = "ddos: hello world!";
    strcpy((void *)0x100, str);
    assert(strcmp((void *)0x100, (void *)(0x100 + PGSIZE)) == 0);

    *(char *)(page2pa(p) + 0x100) = '\0';
    assert(strlen((const char *)0x100) == 0);
    FreePagetable(kernel_pagetable);
    cprintf("check_kernel_pagetable() succeeded!\n");
    w_satp(0);
    sfence_vma();
}

void kvm_init_hart() {
    w_satp(MAKE_SATP(kernel_pagetable));
    sfence_vma();
}

static void check_print_pagetable(void) {
    kernel_pagetable = alloc_pagetable();
    Page *p1, *p2, *p3;
    p1 = AllocPage();
    p2 = AllocPage();
    p3 = AllocPage();
    assert(page_insert(kernel_pagetable, p1, 0x0, 0) == 0);
    assert(page_insert(kernel_pagetable, p2, 513 * PGSIZE, 0) == 0);
    assert(page_insert(kernel_pagetable, p3, 2 * PGSIZE, 0) == 0);
    kvmmap(UART0, UART0, PGSIZE, PTE_R | PTE_W);
    kvmmap(VIRTIO_START_ADDR, VIRTIO_START_ADDR, PGSIZE, PTE_R | PTE_W);
    kvmmap(KERNBASE, KERNBASE, (uint64_t)kernel_etext - KERNBASE, PTE_R | PTE_X | PTE_G);
    vm_map_print(kernel_pagetable);
    vm_print(kernel_pagetable);
    while (1);
    cprintf("------------------------\n");
    page_remove(kernel_pagetable, 513 * PGSIZE);
    vm_map_print(kernel_pagetable);
    vm_print(kernel_pagetable);
    cprintf("------------------------\n");
    page_remove(kernel_pagetable, 0x0);
    page_remove(kernel_pagetable, 2 * PGSIZE);
    FreePagetable(kernel_pagetable);
    while (1);
}

void init_kernel_pagetable(void){
    kernel_pagetable = alloc_pagetable();
    kvmmap(UART0, UART0, PGSIZE, PTE_R | PTE_W);
    kvmmap(VIRTIO_START_ADDR, VIRTIO_START_ADDR, VIRTIO_DEVICE_NUM * PGSIZE, PTE_R | PTE_W);
    kvmmap(CLINT, CLINT, 0x10000, PTE_R | PTE_W);
    kvmmap(PLIC, PLIC, 0x400000, PTE_R | PTE_W);
    // kvmmap(KERNBASE, KERNBASE, PHYMEMSIZE, PTE_R | PTE_W);
    kvmmap(KERNBASE, KERNBASE, (uint64_t)kernel_etext - KERNBASE, PTE_R | PTE_X);

    kvmmap((uint64_t)kernel_etext, (uint64_t)kernel_etext, PHYSTOP - (uint64_t)kernel_etext, PTE_R | PTE_W);
    kvmmap(TRAMPOLINE, (uint64_t)trampoline, PGSIZE, PTE_R | PTE_X);
}

void pmm_init(void) {
    init_pmm_manager();
    page_init();
#ifdef PRINT_MM_TEST
    check_alloc_page();
    // check_print_pagetable();
    check_pagetable();
    check_kernel_pagetable();
#endif
    init_kernel_pagetable();
    // while (1);
}
