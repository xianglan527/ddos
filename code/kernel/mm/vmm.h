#ifndef __MM_VMM_H__
#define __MM_VMM_H__
#include "list.h"
#include "pmm.h"
#include "rbtree.h"
#include "shmem.h"
#include "stdarg.h"
#include "types.h"

typedef struct mm_struct Mm_struct;

typedef struct vma_struct Vma_struct;
struct vma_struct {
    Mm_struct *vm_mm;
    uintptr_t vm_start;
    uintptr_t vm_end;
    uint32_t vm_flags;
    Rb_node rb_link;
    List_entry list_link;
    Shmem_struct *shmem;
    size_t shmem_off;
};

#define le2vma(le, member) to_struct((le), Vma_struct, member)

#define rbn2vma(node, member) to_struct((node), Vma_struct, member)

#define VM_READ 0x00000001
#define VM_WRITE 0x00000002
#define VM_EXEC 0x00000004
#define VM_STACK 0x00000008
#define VM_SHARE 0x00000010
#define VM_USER 0x00000020

typedef struct mm_struct Mm_struct;
struct mm_struct {
    List_entry mmap_list;
    Rb_tree *mmap_tree;
    Vma_struct *mmap_cache;
    pagetable_t *pagetable;
    int map_count;
    uintptr_t swap_address;
    Atomic mm_count;
    Spinlock mm_lock;
    uintptr_t brk_start, brk;
};

static inline long mm_count(Mm_struct *mm) { return atomic_read(&mm->mm_count); }

static inline void set_mm_count(Mm_struct *mm, long val) { atomic_set(&mm->mm_count, val); }

static inline long mm_count_inc(Mm_struct *mm) { return atomic_add_return(&mm->mm_count, 1); }

static inline long mm_count_dec(Mm_struct *mm) { return atomic_sub_return(&mm->mm_count, 1); }

static inline void lock_mm(Mm_struct *mm) {
    if (mm != nullptr) acquire(&mm->mm_lock);
}

static inline void unlock_mm(Mm_struct *mm) {
    if (mm != nullptr) release(&mm->mm_lock);
}

Vma_struct *find_vma(Mm_struct *mm, uintptr_t addr);
Vma_struct *vma_create(uintptr_t vm_start, uintptr_t vm_end, uint32_t vm_flags);
void insert_vma_struct(Mm_struct *mm, Vma_struct *vma);

Mm_struct *mm_create(void);
void mm_destroy(Mm_struct *mm);

void vmm_init(void);

int do_pagatable_fault(Mm_struct *mm, uintptr_t addr, bool write);
void print_vma_list(Mm_struct *mm);
int mm_map(Mm_struct *mm, uintptr_t addr, size_t len, uint32_t vm_flags, Vma_struct **vma_store);
int mm_unmap(Mm_struct *mm, uintptr_t addr, size_t len);
int dup_mmap(Mm_struct *to, Mm_struct *from);
void exit_mmap(Mm_struct *mm);
int64_t get_unmapped_area(Mm_struct *mm, size_t len);
bool user_mem_check(Mm_struct *mm, uintptr_t addr, size_t len, bool write);
int mm_map_shmem(Mm_struct *mm, uintptr_t addr, uint32_t vm_flags, Shmem_struct *shmem,
                 Vma_struct **vma_store);
Vma_struct *find_vma_intersection(Mm_struct *mm, uintptr_t start, uintptr_t end);
int mm_brk(Mm_struct *mm, uintptr_t addr, size_t len);
#endif