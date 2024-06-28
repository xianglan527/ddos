#ifndef __MM_SHMEM_H__
#define __MM_SHMEM_H__
#include "atomic.h"
#include "list.h"
#include "spinlock.h"
#include "stdarg.h"
#include "types.h"

#define SHMN_NENTRY (PGSIZE / sizeof(pte_t))
#define PAGE_SHMN_LENS (PGSIZE * SHMN_NENTRY)

typedef struct shmem_struct Shmem_struct;
struct shmem_struct {
    List_entry shmn_list;
    size_t len;
    size_t shmem_page_size;
    Atomic shmem_ref;
    pte_t *entry;
    Spinlock shmem_lock;
};

static inline int shmem_ref(Shmem_struct *shmem) { return atomic_read(&shmem->shmem_ref); }

static inline void set_shmem_ref(Shmem_struct *shmem, long val) { atomic_set(&shmem->shmem_ref, val); }

static inline long shmem_ref_inc(Shmem_struct *shmem) { return atomic_add_return(&shmem->shmem_ref, 1); }

static inline long shmem_ref_dec(Shmem_struct *shmem) { return atomic_sub_return(&shmem->shmem_ref, 1); }

static inline void lock_shmem(Shmem_struct *shmem) { acquire(&shmem->shmem_lock); }

static inline void unlock_shmem(Shmem_struct *shmem) { release(&shmem->shmem_lock); }
Shmem_struct *shmem_create(size_t len);
void shmem_destroy(Shmem_struct *shmem);
pte_t *shmem_get_entry(Shmem_struct *shmem, uintptr_t addr, bool create);
int shmem_insert_entry(Shmem_struct *shmem, uintptr_t addr, pte_t entry);
int shmem_remove_entry(Shmem_struct *shmem, uintptr_t addr);

#endif