#ifndef __MM_VMM_H__
#define __MM_VMM_H__
#include "list.h"
#include "rbtree.h"
#include "stdarg.h"
#include "types.h"
#include "pmm.h"

typedef struct mm_struct Mm_struct;

typedef struct vma_struct Vma_struct;
struct vma_struct {
    Mm_struct *vm_mm;
    uintptr_t vm_start;
    uintptr_t vm_end;
    uint32_t vm_flags;
    Rb_node rb_link;
    List_entry list_link;
};

#define le2vma(le, member) to_struct((le), Vma_struct, member)

#define rbn2vma(node, member) to_struct((node), Vma_struct, member)

#define VM_READ         0x00000001
#define VM_WRITE        0x00000002
#define VM_EXEC         0x00000004
#define VM_STACK        0x00000008

typedef struct mm_struct Mm_struct;
struct mm_struct{
    List_entry mmap_list;
    Rb_tree *mmap_tree;
    Vma_struct *mmap_cache;
    pagetable_t *pagetable;
    int map_count;
    uintptr_t swap_address;
};

Vma_struct *find_vma(Mm_struct *mm, uintptr_t addr);
Vma_struct *vma_create(uintptr_t vm_start, uintptr_t vm_end, uint32_t vm_flags);
void insert_vma_struct(Mm_struct *mm, Vma_struct *vma);

Mm_struct *mm_create(void);
void mm_destroy(Mm_struct *mm);

void vmm_init(void);

int do_pagatable_fault(Mm_struct *mm, uintptr_t addr);
void print_vma_list(Mm_struct *mm);
int mm_map(Mm_struct *mm, uintptr_t addr, size_t len, uint32_t vm_flags, Vma_struct **vma_store);
int mm_unmap(Mm_struct *mm, uintptr_t addr, size_t len);
int dup_mmap(Mm_struct *to, Mm_struct *from);
void exit_mmap(Mm_struct *mm);
int64_t get_unmapped_area(Mm_struct *mm, size_t len);
bool user_mem_check(Mm_struct *mm, uintptr_t addr, size_t len, bool write);
#endif