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
    }
    return vma;
}

static void vma_destroy(Vma_struct *vma) { kfree(vma); }

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

void mm_destroy(Mm_struct *mm) {
    if (mm->mmap_tree != nullptr) rb_tree_destroy(mm->mmap_tree);
    while (!list_empty(&(mm->mmap_list))) {
        List_entry *le = list_next(&(mm->mmap_list));
        list_del(le);
        vma_destroy(le2vma(le, list_link));
    }
    kfree(mm);
    mm->map_count = 0;
}

void vmm_init(void) { check_vmm(); }

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

int do_pagatable_fault(Mm_struct *mm, uintptr_t addr) {
    int ret = -E_INVAL;
    Vma_struct *vma = find_vma(mm, addr);
    if (vma == nullptr || vma->vm_start > addr) goto failed;

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
        if (pagetable_alloc_page(mm->pagetable, addr, perm) == 0) goto failed;
    } else {
        Page *page;
        if ((ret = swap_in_page(*ptep, &page)) != 0) goto failed;
        page_insert(mm->pagetable, page, addr, perm);
    }
    ret = 0;

failed:
    return ret;
}