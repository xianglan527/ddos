#ifndef __MM_SLAB_H__
#define __MM_SLAB_H__
#include "stdarg.h"
#include "types.h"
#define KMALLOC_MAX_ORDER 10

void slab_init(void);

void *kmalloc(size_t n);
void *aligned_kmalloc(size_t size, size_t alignment);
void kfree(void *objp);
void aligned_kfree(void *aligned);

size_t slab_allocated(void);

#endif