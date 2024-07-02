#include "vmm.h"

#include "assert.h"
#include "config.h"
#include "error.h"
#include "slab.h"
#include "stdio.h"
#include "string.h"
#include "swap.h"
#include "trap.h"

static void check_vmm(void);
static void check_vma_struct(void);
static void check_pgfault(void);

Mm_struct *mm_create(void) {
    Mm_struct *mm = kmalloc(sizeof(Mm_struct));
    if (mm != nullptr) {
        list_init(&mm->mmap_list);
        mm->mmap_tree = nullptr;
        mm->mmap_cache = nullptr;
        mm->pagetable = nullptr;
        mm->map_count = 0;
        mm->swap_address = 0;
    }
    return mm;
}

Vma_struct *vma_create(uintptr_t vm_start, uintptr_t vm_end, uint32_t vm_flags) {
    Vma_struct *vma = kmalloc(sizeof(Vma_struct));
    if (vma != nullptr) {
        vma->vm_start = vm_start;
        vma->vm_end = vm_end;
        vma->vm_flags = vm_flags;
        vma->shmem = nullptr;
        vma->shmem_off = 0;
    }
    return vma;
}

static void vma_destroy(Vma_struct *vma) { 
    if(vma->vm_flags & VM_SHARE){
        if(shmem_ref_dec(vma->shmem) == 0)
            shmem_destroy(vma->shmem);
    }
    kfree(vma); 
}

static inline Vma_struct *find_vma_rb(Rb_tree *tree, uintptr_t addr) {
    Rb_node *node = rb_node_root(tree);
    Vma_struct *vma = nullptr, *tmp;
    while (node != nullptr) {
        tmp = rbn2vma(node, rb_link);
        if (tmp->vm_end > addr) {
            vma = tmp;
            if (tmp->vm_start <= addr) break;
            node = rb_node_left(tree, node);
        } else
            node = rb_node_right(tree, node);
    }
    return vma;
}

Vma_struct *find_vma(Mm_struct *mm, uintptr_t addr) {
    Vma_struct *vma = nullptr;
    if (mm != nullptr) {
        vma = mm->mmap_cache;
        if (!(vma != nullptr && vma->vm_start <= addr && vma->vm_end > addr)) {
            if (mm->mmap_tree != nullptr)
                vma = find_vma_rb(mm->mmap_tree, addr);
            else {
                bool found = 0;
                List_entry *list = &mm->mmap_list, *le = list;
                while ((le = list_next(le)) != list) {
                    vma = le2vma(le, list_link);
                    if (addr < vma->vm_end) {
                        found = 1;
                        break;
                    }
                }
                if (!found) vma = nullptr;
            }
        }
        if (vma != nullptr) mm->mmap_cache = vma;
    }
    return vma;
}

static inline int vma_compare(Rb_node *node1, Rb_node *node2) {
    Vma_struct *vma1 = rbn2vma(node1, rb_link);
    Vma_struct *vma2 = rbn2vma(node2, rb_link);
    uintptr_t start1 = vma1->vm_start, start2 = vma2->vm_start;
    return (start1 < start2) ? -1 : (start1 > start2) ? 1 : 0;
}

static inline void check_vma_overlap(Vma_struct *prev, Vma_struct *next) {
    assert(prev->vm_start < prev->vm_end);
    assert(prev->vm_end <= next->vm_start);
    assert(next->vm_start < next->vm_end);
}

static inline void insert_vma_rb(Rb_tree *tree, Vma_struct *vma, Vma_struct **vma_prevp) {
    Rb_node *node = &vma->rb_link, *prev;
    rb_insert(tree, node);
    if (vma_prevp != nullptr) {
        prev = rb_node_prev(tree, node);
        *vma_prevp = (prev != nullptr) ? rbn2vma(prev, rb_link) : nullptr;
    }
}

void insert_vma_struct(Mm_struct *mm, Vma_struct *vma) {
    assert(vma->vm_start < vma->vm_end);
    List_entry *list = &mm->mmap_list;
    List_entry *le_prev = list, *le_next;
    if (mm->mmap_tree != nullptr) {
        Vma_struct *mmap_prev;
        insert_vma_rb(mm->mmap_tree, vma, &mmap_prev);
        if (mmap_prev != nullptr) le_prev = &mmap_prev->list_link;
    } else {
        List_entry *le = list;
        while ((le = list_next(le)) != list) {
            Vma_struct *mmap_prev = le2vma(le, list_link);
            if (mmap_prev->vm_start > vma->vm_start) break;
            le_prev = le;
        }
    }
    le_next = list_next(le_prev);

    if (le_prev != list) check_vma_overlap(le2vma(le_prev, list_link), vma);
    if (le_next != list) check_vma_overlap(vma, le2vma(le_next, list_link));
    vma->vm_mm = mm;
    list_add_after(le_prev, &vma->list_link);
    mm->map_count++;
    if (mm->mmap_tree == nullptr && mm->map_count >= RB_MIN_MAP_COUNT) {
        mm->mmap_tree = rb_tree_create(vma_compare);
        if (mm->mmap_tree != nullptr) {
            List_entry *list = &mm->mmap_list, *le = list;
            while ((le = list_next(le)) != list) insert_vma_rb(mm->mmap_tree, le2vma(le, list_link), nullptr);
        }
    }
}

static int remove_vma_struct(Mm_struct *mm, Vma_struct *vma){
    assert(mm == vma->vm_mm);
    if(mm->mmap_tree != nullptr){
        rb_delete(mm->mmap_tree, &(vma->rb_link));
    }
    list_del(&vma->list_link);
    if(vma == mm->mmap_cache){
        mm->mmap_cache = nullptr;
    }
    mm->map_count--;
    return 0;
}

static int remove_and_destroy_vma_struct(Mm_struct *mm, Vma_struct *vma) {
    assert(mm == vma->vm_mm);
    if (mm->mmap_tree != nullptr) { rb_delete(mm->mmap_tree, &(vma->rb_link)); }
    list_del(&vma->list_link);
    if (vma == mm->mmap_cache) { mm->mmap_cache = nullptr; }
    mm->map_count--;
    vma_destroy(le2vma(&vma->list_link, list_link));
    return 0;
}

void mm_destroy(Mm_struct *mm) {
    if (mm->mmap_tree != nullptr) rb_tree_destroy(mm->mmap_tree);
    List_entry *list = &mm->mmap_list, *le;
    while((le = list_next(list)) != list){
        list_del(le);
        vma_destroy(le2vma(le, list_link));
    }
    kfree(mm);
}

void vmm_init(void) {
#ifdef PRINT_MM_TEST
    check_vmm(); 
#endif
}

int mm_map(Mm_struct *mm, uintptr_t addr, size_t len, uint32_t vm_flags, Vma_struct **vma_store){
    uintptr_t start = PGROUNDDOWN(addr), end = PGROUNDUP(addr + len);
    assert(mm != nullptr);
    int ret = -E_INVAL;
    Vma_struct *vma;
    if((vma = find_vma(mm, start)) != nullptr && end > vma->vm_start)
        goto out;
    ret = -E_NO_MEM;
    if((vma = vma_create(start, end, vm_flags)) == nullptr)
        goto out;
    insert_vma_struct(mm, vma);
    if(vma_store != nullptr){
        *vma_store = vma;
    }
    ret = 0;
out:
    return ret;
}

int mm_map_shmem(Mm_struct *mm, uintptr_t addr, uint32_t vm_flags, Shmem_struct *shmem,
                 Vma_struct **vma_store){
    if((addr % PGSIZE) != 0 || shmem == nullptr)
        return -E_INVAL;
    int ret;
    Vma_struct *vma;
    shmem_ref_inc(shmem);
    if((ret = mm_map(mm, addr, shmem->len, vm_flags, &vma)) != 0){
        shmem_ref_dec(shmem);
        return ret;
    }              
    vma->shmem = shmem;
    vma->shmem_off = 0;
    vma->vm_flags |= VM_SHARE;
    if(vma_store != nullptr)
        *vma_store = vma;
    return 0;   
}

    static void vma_resize(Vma_struct *vma, uintptr_t start, uintptr_t end) {
    assert(start % PGSIZE == 0 && end % PGSIZE == 0);
    assert(vma->vm_start <= start && start < end && end <= vma->vm_end);
    if(vma->vm_flags & VM_SHARE){
        vma->shmem_off += start - vma->vm_start;
    }
    vma->vm_start = start, vma->vm_end = end;
}

int mm_unmap(Mm_struct *mm, uintptr_t addr, size_t len){
    uintptr_t start = PGROUNDDOWN(addr), end = PGROUNDUP(addr + len);
    if(!(addr < MAXVA && (addr + len) < MAXVA))
        return -E_INVAL;
    assert(mm != nullptr);
    
    Vma_struct *vma;
    if((vma = find_vma(mm, start)) == nullptr || end <= vma->vm_start)
        return 0;
    
    if(vma->vm_start < start && end < vma->vm_end){
        Vma_struct *nvma;
        if((nvma = vma_create(vma->vm_start, start, vma->vm_flags)) == nullptr)
            return -E_NO_MEM;
        vma_resize(vma, end, vma->vm_end);
        insert_vma_struct(mm, nvma);
        unmap_range(mm->pagetable, start, end);
        return 0;
    }

    List_entry free_list, *le;
    list_init(&free_list);
    while(vma->vm_start < end){
        le = list_next(&vma->list_link);
        remove_vma_struct(mm, vma);
        list_add(&free_list, &vma->list_link);
        if(le == &(mm->mmap_list))
            break;
        vma = le2vma(le, list_link);
    }
    le = list_next(&free_list);
    while(le != &free_list){
        vma = le2vma(le, list_link);
        le = list_next(le);
        uintptr_t un_start, un_end;
        if(vma->vm_start < start){
            un_start = start, un_end = vma->vm_end;
            vma_resize(vma, vma->vm_start, un_start);
            insert_vma_struct(mm, vma);
        }else{
            un_start = vma->vm_start, un_end = vma->vm_end;
            if(end < un_end){
                un_end = end;
                vma_resize(vma, un_end, vma->vm_end);
                insert_vma_struct(mm, vma);
            }
            else{
                vma_destroy(vma);
            }
        }
        unmap_range(mm->pagetable, un_start, un_end);
    }
    return 0;
}

int dup_mmap(Mm_struct *to, Mm_struct *from){
    assert(to != nullptr && from != nullptr);
    if(!list_empty(&to->mmap_list)){
        exit_mmap(to);
    }
    List_entry *list = &from->mmap_list, *le = list;
    while((le = list_next(le)) != list){
        Vma_struct *vma, *nvma;
        vma = le2vma(le, list_link);
        nvma = vma_create(vma->vm_start, vma->vm_end, vma->vm_flags);
        if(nvma == nullptr)
            return -E_NO_MEM;
        else{
            if(vma->vm_flags & VM_SHARE){
                nvma->shmem = vma->shmem;
                nvma->shmem_off = vma->shmem_off;
                shmem_ref_inc(vma->shmem);
            }
        }
        insert_vma_struct(to, nvma);
        bool share = vma->vm_flags & VM_SHARE;
        if(copy_range(to->pagetable, from->pagetable, vma->vm_start, vma->vm_end, share) != 0){
            return -E_NO_MEM;
        }
    }
    return 0;
}

void exit_mmap(Mm_struct *mm){
    assert(mm != nullptr);
    pagetable_t *pagatable = mm->pagetable;
    List_entry *list = &mm->mmap_list, *le = list;
    while((le = list_next(le)) != list){
        Vma_struct *vma = le2vma(le, list_link);
        unmap_range(pagatable, vma->vm_start, vma->vm_end);
    }
    while((le = list_next(le)) != list){
        Vma_struct *vma = le2vma(le, list_link);
        exit_range(pagatable, vma->vm_start, vma->vm_end);
    }
    while ((le = list_next(le)) != list) {
        Vma_struct *vma = le2vma(le, list_link);
        remove_and_destroy_vma_struct(mm, vma);
    }
}

int64_t get_unmapped_area(Mm_struct *mm, size_t len){
    if(len == 0 || len >= MAXVA )
        return 0;
    uintptr_t start = MAXVA - len;
    List_entry *list = &mm->mmap_list, *le = list;
    while((le = list_prev(le)) != list){
        Vma_struct *vma = le2vma(le, list_link);
        if(start >= vma->vm_end)
            return start;
        start = vma->vm_start - len;
    }
    return start;     //if not found will return negative value
}

bool user_mem_check(Mm_struct *mm, uintptr_t addr, size_t len, bool write){
    if(addr + len >= MAXVA)
        return 0;
    if(mm != nullptr){
        Vma_struct *vma;
        uintptr_t start = addr, end = addr + len;
        while(start < end){
            if((vma = find_vma(mm, start)) == nullptr || start < vma->vm_start)
                return 0;
            if(!(vma->vm_flags & ((write) ? VM_WRITE : VM_READ)))
                return 0;
            if(write && (vma->vm_flags & VM_STACK)){
                if(start < vma->vm_start + PGSIZE)
                    return 0;
            }
            start = vma->vm_end;
        }
        return 1;
    }
    return 1;
}

static void check_vmm(void) {
    size_t slab_allocated_store = slab_allocated();

    check_vma_struct();
    check_pgfault();

    assert(slab_allocated_store == slab_allocated());

    cprintf("check_vmm() succeeded.\n");
}

void print_vma_list(Mm_struct *mm) {
    Vma_struct *vma = nullptr;
    List_entry *list = &mm->mmap_list, *le = list;
    while ((le = list_next(le)) != list) {
        vma = le2vma(le, list_link);
        cprintf("vma->start is : 0x%016x     ", vma->vm_start);
        cprintf("vma->end is : 0x%016x       ", vma->vm_end);
        cprintf("vma->flags is : 0x%x\n", vma->vm_flags);
    }
}

static void check_vma_struct(void) {
    size_t nr_free_pages_store = nr_free_pages();
    size_t slab_allocated_store = slab_allocated();

    Mm_struct *mm = mm_create();
    assert(mm != nullptr);

    int step1 = RB_MIN_MAP_COUNT * 2, step2 = step1 * 10;

    int i;
    for (i = step1; i >= 0; i--) {
        Vma_struct *vma = vma_create(i * 5, i * 5 + 2, 0);
        assert(vma != nullptr);
        insert_vma_struct(mm, vma);
    }

    for (i = step1 + 1; i <= step2; i++) {
        struct vma_struct *vma = vma_create(i * 5, i * 5 + 2, 0);
        assert(vma != NULL);
        insert_vma_struct(mm, vma);
    }

    List_entry *le = list_next(&mm->mmap_list);
    for (i = 0; i <= step2; i++) {
        assert(le != &mm->mmap_list);
        Vma_struct *mmap = le2vma(le, list_link);
        assert(mmap->vm_start == i * 5 && mmap->vm_end == i * 5 + 2);
        le = list_next(le);
    }

    for (i = 0; i < 5 * step2 + 2; i++) {
        Vma_struct *vma = find_vma(mm, i);
        assert(vma != nullptr);
        int j = i / 5;
        if (i >= 5 * j + 2) j++;
        assert(vma->vm_start == j * 5 && vma->vm_end == j * 5 + 2);
    }
    mm_destroy(mm);
    assert(nr_free_pages_store == nr_free_pages());
    assert(slab_allocated_store == slab_allocated());
    init_kernel_pagetable();
    kvm_init_hart();
    cprintf("check_vma_struct() succeeded!\n");
}

struct mm_struct *check_mm_struct;

static void check_pgfault(void) {
    size_t nr_free_pages_store = nr_free_pages();
    size_t slab_allocated_store = slab_allocated();
    check_mm_struct = mm_create();
    assert(check_mm_struct != nullptr);
    Mm_struct *mm = check_mm_struct;
    mm->pagetable = kernel_pagetable;

    Vma_struct *vma = vma_create(0, PGSIZE, VM_WRITE);
    assert(vma != nullptr);

    insert_vma_struct(mm, vma);

    uintptr_t addr = 0x100;
    assert(find_vma(mm, addr) == vma);
    int i, sum = 0;
    for (i = 0; i < 100; i++) {
        *(char *)(addr + i) = i;
        sum += i;
    }
    for (i = 0; i < 100; i++) { sum -= *(char *)(addr + i); }
    assert(sum == 0);

    page_remove(kernel_pagetable, ROUNDDOWN(addr, PGSIZE));
    pte_t pg1 = kernel_pagetable[0];
    pte_t pg2 = ((pte_t *)PTE2PA(pg1))[0];
    ((pte_t *)PTE2PA(pg1))[0] = 0;
    // FreePage(pa2page(PTE2PA(pg1)));
    FreePage(pa2page(PTE2PA(pg2)));
    // FreePagetable(kernel_pagetable);
    mm->pagetable = nullptr;
    mm_destroy(mm);
    check_mm_struct = nullptr;
    assert(nr_free_pages_store == nr_free_pages());
    assert(slab_allocated_store == slab_allocated());
    // assert(slab_allocated_store == slab_allocated());

    cprintf("check_pgfault() succeeded!\n");
}

int do_pagatable_fault(Mm_struct *mm, uintptr_t addr, bool write) {
    int ret = -E_INVAL;
    Vma_struct *vma = find_vma(mm, addr);
    if (vma == nullptr || vma->vm_start > addr) goto failed;
    if(vma->vm_flags & VM_STACK){
        if(addr < vma->vm_start + PGSIZE)
            goto failed;
    }
    uint32_t perm = 0;
    // uint32_t perm = PTE_U;
    if (vma->vm_flags & VM_WRITE) {
        perm |= PTE_W | PTE_R;
    } else if (vma->vm_flags & VM_READ) {
        perm |= PTE_R;
    }
    addr = PGROUNDDOWN(addr);

    ret = -E_NO_MEM;

    pte_t *ptep;
    if ((ptep = get_pte(mm->pagetable, addr, 1)) == nullptr) goto failed;
    if (*ptep == 0) {
        if(!(vma->vm_flags & VM_SHARE)){
            if (pagetable_alloc_page(mm->pagetable, addr, perm) == 0) 
                goto failed;
        }
        else{
            lock_shmem(vma->shmem);
            uintptr_t shmem_addr = addr - vma->vm_start + vma->shmem_off;
            pte_t *sh_ptep = shmem_get_entry(vma->shmem, shmem_addr, 1);
            if(sh_ptep == nullptr || *sh_ptep == 0) {
                unlock_shmem(vma->shmem);
                goto failed;
            }       
            unlock_shmem(vma->shmem);
            if(*sh_ptep & PTE_V){
                page_insert(mm->pagetable, pa2page(PTE2PA(*sh_ptep)), addr, perm);
            }else{
                swap_duplicate(*ptep);
                *ptep = *sh_ptep;
            }
        }
    } else {
        Page *page, *newpage = nullptr;
        bool cow = ((vma->vm_flags & (VM_SHARE | VM_WRITE)) == VM_WRITE);
        assert(!(*ptep & PTE_V) || (write && !(*ptep & PTE_PW) && cow));
        if(cow){
            newpage = AllocPage();
        }
        if(*ptep & PTE_V){
            page = pte2page(*ptep);
        }
        else{
            if ((ret = swap_in_page(*ptep, &page)) != 0){
                if(newpage != nullptr)
                    FreePage(newpage);
                goto failed;
            } 
            if(!write){
                perm &= ~PTE_PW;
                cow = 0;
            }
        }
        if(cow){
            if(page_ref(page) + swap_page_count(page) > 1){
                if(newpage == nullptr)
                    goto failed;
                memcpy((void *)page2kva(newpage), (void *)page2kva(page), PGSIZE);
                page = newpage, newpage = nullptr;
            }
        }   
        page_insert(mm->pagetable, page, addr, perm);
        if(newpage != nullptr){
            FreePage(newpage);
        }
    }
    ret = 0;

failed:
    return ret;
}