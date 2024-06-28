#include "slab.h"
#include "rbtree.h"
#include "assert.h"
#include "config.h"
#include "list.h"
#include "pmm.h"
#include "spinlock.h"
#include "stdio.h"

#define BUFCTL_END 0xFFFFFFFFL
#define SLAB_LIMIT 0xFFFFFFFFL

static Spinlock slab_lock;

typedef size_t kem_bufctl_t;

typedef struct slab Slab;
struct slab {
    List_entry slab_link;
    void *s_mem;
    size_t inuse;
    size_t offset;
    kem_bufctl_t free;
};

#define le2slab(le, member) to_struct((le), Slab, member)

typedef struct kmem_cache Kmem_cache;
struct kmem_cache {
    List_entry slabs_full;
    List_entry slabs_notfull;
    size_t objsize;
    size_t num;
    size_t offset;
    bool off_slab;
    size_t page_order;
    Kmem_cache *slab_cachep;
};

#define MIN_SIZE_ORDER 5
#define MAX_SIZE_ORDER 17
#define SLAB_CACHE_NUM (MAX_SIZE_ORDER - MIN_SIZE_ORDER + 1)

static Kmem_cache slab_cache[SLAB_CACHE_NUM];
void check_slab(void);

static size_t slab_mgmt_size(size_t num, size_t align) {
    return ROUNDUP(sizeof(Slab) + num * sizeof(kem_bufctl_t), align);
}

static void cache_estimate(size_t order, size_t objsize, size_t align, bool off_slab, size_t *remainder,
                           size_t *num) {
    size_t nr_objs, mgmt_size;
    size_t slab_size = (PGSIZE << order);

    if (off_slab) {
        mgmt_size = 0;
        nr_objs = slab_size / objsize;
        if (nr_objs > SLAB_LIMIT) nr_objs = SLAB_LIMIT;
    } else {
        nr_objs = (slab_size - sizeof(Slab)) / (objsize + sizeof(kem_bufctl_t));
        while (slab_mgmt_size(nr_objs, align) + nr_objs * objsize > slab_size) { nr_objs--; }
        if (nr_objs > SLAB_LIMIT) { nr_objs = SLAB_LIMIT; }
        mgmt_size = slab_mgmt_size(nr_objs, align);
    }
    *num = nr_objs;
    *remainder = slab_size - nr_objs * objsize - mgmt_size;
}

static void calculate_slab_order(Kmem_cache *cachep, size_t objsize, size_t align, bool off_slab,
                                 size_t *left_over) {
    size_t order;
    for (order = 0; order <= KMALLOC_MAX_ORDER; order++) {
        size_t num, remainder;
        cache_estimate(order, objsize, align, off_slab, &remainder, &num);
        if (num != 0) {
            if (off_slab) {
                size_t off_slab_limit = objsize - sizeof(Slab);
                off_slab_limit /= sizeof(kem_bufctl_t);
                if (num > off_slab_limit) panic("off_slab: objsize = %d, num = %d.", objsize, num);
            }
        }
        if (remainder * 8 <= (PGSIZE << order)) {
            cachep->num = num;
            cachep->page_order = order;
            if (left_over != nullptr) *left_over = remainder;
            return;
        }
    }
    panic("calculate slab: faild.");
    return;
}

static inline size_t getorder(size_t n) {
    size_t order = MIN_SIZE_ORDER, order_size = (1 << order);
    for (; order <= MAX_SIZE_ORDER; order++, order_size <<= 1) {
        if (n <= order_size) return order;
    }
    panic("getorder faild. %d\n", n);
    return 0;
}

static void init_kmem_cache(Kmem_cache *cachep, size_t objsize, size_t align) {
    list_init(&cachep->slabs_full);
    list_init(&(cachep->slabs_notfull));

    objsize = ROUNDUP(objsize, align);
    cachep->objsize = objsize;
    cachep->off_slab = (objsize >= (PGSIZE >> 3));

    size_t left_over;
    calculate_slab_order(cachep, objsize, align, cachep->off_slab, &left_over);
    assert(cachep->num > 0);
    size_t mgmt_size = slab_mgmt_size(cachep->num, align);
    if (cachep->off_slab && left_over >= mgmt_size) { cachep->off_slab = 0; }
    if (cachep->off_slab) {
        cachep->offset = 0;
        cachep->slab_cachep = slab_cache + (getorder(mgmt_size) - MIN_SIZE_ORDER);
    } else {
        cachep->offset = mgmt_size;
    }
}

void slab_init(void) {
    size_t i;
    for (i = 0; i < SLAB_CACHE_NUM; i++) {
        init_kmem_cache(slab_cache + i, 1 << (i + MIN_SIZE_ORDER), SLAB_ALIGN);
    }
    initlock(&slab_lock, "slab_lock");
#ifdef PRINT_MM_TEST
    check_slab();
#endif
}

size_t slab_allocated(void) {
    size_t total = 0;
    int i;
    acquire(&slab_lock);
    for (i = 0; i < SLAB_CACHE_NUM; i++) {
        Kmem_cache *cachep = slab_cache + i;
        List_entry *list, *le;
        list = le = &(cachep->slabs_full);
        while ((le = list_next(le)) != list) { total += cachep->num * cachep->objsize; }
        list = le = &(cachep->slabs_notfull);
        while ((le = list_next(le)) != list) {
            Slab *slabp = le2slab(le, slab_link);
            total += slabp->inuse * cachep->objsize;
        }
    }
    release(&slab_lock);
    return total;
}

static void *kmem_cache_alloc(Kmem_cache *cachep);
#define slab_bufctl(slabp) ((kem_bufctl_t *)((Slab *)(slabp) + 1))

static Slab *kmem_cache_slabmgmt(Kmem_cache *cachep, Page *page) {
    void *objp = (void *)page2kva(page);
    Slab *slabp;
    if (cachep->off_slab) {
        if ((slabp = kmem_cache_alloc(cachep->slab_cachep)) == nullptr) return nullptr;
    } else {
        slabp = (void *)page2kva(page);
    }
    slabp->inuse = 0;
    slabp->offset = cachep->offset;
    slabp->s_mem = objp + cachep->offset;
    return slabp;
}

#define SET_PAGE_CACHE(page, cachep)                                       \
    do {                                                                   \
        Page *__page = (Page *)(page);                                     \
        Kmem_cache **__cachepp = (Kmem_cache **)&(__page->page_link.next); \
        *__cachepp = (Kmem_cache *)(cachep);                               \
    } while (0)

#define SET_PAGE_SLAB(page, slabp)                            \
    do {                                                      \
        Page *__page = (Page *)(page);                        \
        Slab **__slabpp = (Slab **)&(__page->page_link.prev); \
        *__slabpp = (Slab *)(slabp);                          \
    } while (0)

static bool kmem_cache_grow(Kmem_cache *cachep) {
    Page *page = alloc_pages(1 << cachep->page_order);
    if (page == nullptr) goto failed;
    Slab *slabp;
    if ((slabp = kmem_cache_slabmgmt(cachep, page)) == nullptr) goto oops;
    size_t order_size = 1 << cachep->page_order;
    do {
        SET_PAGE_CACHE(page, cachep);
        SET_PAGE_SLAB(page, slabp);
        SetPageSlab(page);
        page++;
    } while (--order_size);
    for (int i = 0; i < cachep->num; i++) slab_bufctl(slabp)[i] = i + 1;
    slab_bufctl(slabp)[cachep->num - 1] = BUFCTL_END;
    slabp->free = 0;
    acquire(&slab_lock);
    list_add(&cachep->slabs_notfull, &slabp->slab_link);
    release(&slab_lock);
    return 1;

oops:
    free_pages(page, 1 << cachep->page_order);
failed:
    return 0;
}

static void *kmem_cache_alloc_one(Kmem_cache *cachep, Slab *slabp) {
    slabp->inuse++;
    void *objp = slabp->s_mem + slabp->free * cachep->objsize;
    slabp->free = slab_bufctl(slabp)[slabp->free];

    if (slabp->free == BUFCTL_END) {
        list_del(&slabp->slab_link);
        list_add(&cachep->slabs_full, &slabp->slab_link);
    }
    return objp;
}

static void *kmem_cache_alloc(Kmem_cache *cachep) {
    void *objp;
try_again:
    acquire(&slab_lock);
    if (list_empty(&cachep->slabs_notfull)) goto alloc_new_slab;
    Slab *slabp = le2slab(list_next(&cachep->slabs_notfull), slab_link);
    objp = kmem_cache_alloc_one(cachep, slabp);
    release(&slab_lock);
    return objp;

alloc_new_slab:
    release(&slab_lock);
    if (kmem_cache_grow(cachep)) { goto try_again; }
    return nullptr;
}

void *kmalloc(size_t size) {
    assert(size > 0);
    size_t order = getorder(size);
    if (order > MAX_SIZE_ORDER) return nullptr;
    return kmem_cache_alloc(slab_cache + (order - MIN_SIZE_ORDER));
}


void *aligned_kmalloc(size_t size, size_t alignment){
    void *original = kmalloc(size + alignment - 1 +sizeof(void *));
    if(original == nullptr) return nullptr;
    void *aligned = (void *)(((size_t)original + alignment - 1 + sizeof(void *)) & ~(alignment - 1));
    ((void **)aligned)[-1] = original;
    return aligned;
}

static void kmem_cache_free(Kmem_cache *cachep, void *obj);

static void kmem_slab_destroy(Kmem_cache *cachep, Slab *slabp) {
    Page *page = kva2page((uintptr_t)slabp->s_mem - slabp->offset);
    Page *p = page;
    size_t order_size = 1 << cachep->page_order;
    do {
        assert(PageSlab(p));
        ClearPageSlab(p);
        p++;
    } while (--order_size);
    free_pages(page, 1 << cachep->page_order);
    if (cachep->off_slab) {
        bool locked = slab_lock.locked;
        if (locked) release(&slab_lock);
        kmem_cache_free(cachep->slab_cachep, slabp);
        if (locked) acquire(&slab_lock);
    }
}

static void kmem_cache_free_one(Kmem_cache *cachep, Slab *slabp, void *objp) {
    size_t objnr = (objp - slabp->s_mem) / cachep->objsize;
    slab_bufctl(slabp)[objnr] = slabp->free;
    slabp->free = objnr;
    slabp->inuse--;
    if (slabp->inuse == 0) {
        list_del(&slabp->slab_link);
        kmem_slab_destroy(cachep, slabp);
    } else if (slabp->inuse == cachep->num - 1) {
        list_del(&slabp->slab_link);
        list_add(&cachep->slabs_notfull, &slabp->slab_link);
    }
}

#define GET_PAGE_CACHE(page) (Kmem_cache *)((page)->page_link.next)

#define GET_PAGE_SLAB(page) (Slab *)((page)->page_link.prev)

static void kmem_cache_free(Kmem_cache *cachep, void *objp) {
    Page *page = kva2page((uintptr_t)objp);
    if (!PageSlab(page)) panic("not a slab page %p\n", objp);
    acquire(&slab_lock);
    kmem_cache_free_one(cachep, GET_PAGE_SLAB(page), objp);
    release(&slab_lock);
}

void kfree(void *objp) { kmem_cache_free(GET_PAGE_CACHE(kva2page((uintptr_t)objp)), objp); }

void aligned_kfree(void *aligned) { kfree(((void **)aligned)[-1]); }

static inline void check_slab_empty(void) {
    for (int i = 0; i < SLAB_CACHE_NUM; i++) {
        Kmem_cache *cachep = slab_cache + i;
        assert(list_empty(&cachep->slabs_full));
        assert(list_empty(&cachep->slabs_notfull));
    }
}

void check_slab(void) {
    int i;
    void *v0, *v1;

    size_t nr_free_pages_store = nr_free_pages();
    size_t slab_allocated_store = slab_allocated();

    check_slab_empty();
    assert(slab_allocated() == 0);
    Kmem_cache *cachep0, *cachep1;

    cachep0 = slab_cache;
    assert(cachep0->objsize == 32 && cachep0->num > 1 && !cachep0->off_slab);
    assert((v0 = kmalloc(16)) != nullptr);

    Slab *slabp0, *slabp1;

    assert(!list_empty(&cachep0->slabs_notfull));
    slabp0 = le2slab(list_next(&cachep0->slabs_notfull), slab_link);
    assert(slabp0->inuse == 1 && list_next(&slabp0->slab_link) == &cachep0->slabs_notfull);

    Page *p0, *p1;
    size_t order_size;

    p0 = kva2page((uintptr_t)slabp0->s_mem - slabp0->offset), p1 = p0;
    order_size = 1 << cachep0->page_order;
    for (int i = 0; i < cachep0->page_order; i++, p1++) {
        assert(PageSlab(p1));
        assert(GET_PAGE_CACHE(p1) == cachep0 && GET_PAGE_SLAB(p1) == slabp0);
    }
    assert(v0 == slabp0->s_mem);
    assert((v1 = kmalloc(16)) != nullptr && v1 == v0 + 32);

    kfree(v0);
    assert(slabp0->free == 0);
    kfree(v1);
    assert(list_empty(&cachep0->slabs_notfull));

    for (i = 0; i < cachep0->page_order; i++, p0++) { assert(!PageSlab(p0)); }

    v0 = kmalloc(16);
    assert(!list_empty(&cachep0->slabs_notfull));
    slabp0 = le2slab(list_next(&cachep0->slabs_notfull), slab_link);

    for (i = 0; i < cachep0->num - 1; i++) kmalloc(16);

    assert(slabp0->inuse == cachep0->num);
    assert(list_next(&cachep0->slabs_full) == &slabp0->slab_link);
    assert(list_empty(&cachep0->slabs_notfull));

    v1 = kmalloc(16);
    assert(!list_empty(&cachep0->slabs_notfull));
    slabp1 = le2slab(list_next(&cachep0->slabs_notfull), slab_link);

    kfree(v0);
    assert(list_empty(&cachep0->slabs_full));
    assert(list_next(&slabp0->slab_link) == &slabp1->slab_link ||
           list_next(&slabp1->slab_link) == &slabp0->slab_link);
    kfree(v1);
    assert(!list_empty(&cachep0->slabs_notfull));
    assert(list_next(&cachep0->slabs_notfull) == &slabp0->slab_link);
    assert(list_next(&slabp0->slab_link) == &cachep0->slabs_notfull);

    v1 = kmalloc(16);
    assert(v1 == v0);
    assert(list_next(&cachep0->slabs_full) == &slabp0->slab_link);
    assert(list_empty(&cachep0->slabs_notfull));

    for (i = 0; i < cachep0->num; i++) { kfree(v1 + i * cachep0->objsize); }
    assert(list_empty(&cachep0->slabs_full));
    assert(list_empty(&cachep0->slabs_notfull));

    cachep0 = slab_cache;

    bool has_off_slab = 0;
    for (i = 0; i < SLAB_CACHE_NUM; i++, cachep0++) {
        if (cachep0->off_slab) {
            has_off_slab = 1;
            cachep1 = cachep0->slab_cachep;
            if (!cachep1->off_slab) break;
        }
    }
    if (!has_off_slab) goto check_pass;
    assert(cachep0->off_slab && !cachep1->off_slab);
    assert(cachep1 < cachep0);

    assert(list_empty(&cachep0->slabs_full));
    assert(list_empty(&cachep0->slabs_notfull));

    assert(list_empty(&cachep1->slabs_full));
    assert(list_empty(&cachep1->slabs_notfull));

    v0 = kmalloc(cachep0->objsize);
    p0 = kva2page((uintptr_t)v0);
    assert((void *)page2kva(p0) == v0);
    if (cachep0->num == 1) {
        assert(!list_empty(&cachep0->slabs_full));
        slabp0 = le2slab(list_next(&cachep0->slabs_full), slab_link);
    } else {
        assert(!list_empty(&cachep0->slabs_notfull));
        slabp0 = le2slab(list_next(&cachep0->slabs_notfull), slab_link);
    }
    assert(slabp0 != nullptr);

    if (cachep1->num == 1) {
        assert(!list_empty(&(cachep1->slabs_full)));
        slabp1 = le2slab(list_next(&(cachep1->slabs_full)), slab_link);
    } else {
        assert(!list_empty(&(cachep1->slabs_notfull)));
        slabp1 = le2slab(list_next(&(cachep1->slabs_notfull)), slab_link);
    }

    assert(slabp1 != NULL);

    order_size = 1 << cachep0->page_order;
    for (i = 0; i < order_size; i++, p0++) {
        assert(PageSlab(p0));
        assert(GET_PAGE_CACHE(p0) == cachep0 && GET_PAGE_SLAB(p0) == slabp0);
    }
    kfree(v0);

    v0 = aligned_kmalloc(527, 16);
    assert((uintptr_t)v0 % 16 == 0);
    aligned_kfree(v0);

check_pass:
    check_rb_tree();
    check_slab_empty();

    assert(slab_allocated() == 0);
    assert(nr_free_pages_store == nr_free_pages());
    assert(slab_allocated_store == slab_allocated());

    cprintf("check_slab() succeeded!\n");
}