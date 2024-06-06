#include "buddyPmm.h"

#define MAX_ORDER 10

static Free_area free_area[MAX_ORDER + 1];

#define free_list(x) (free_area[x].free_list)
#define nr_free(x) (free_area[x].nr_free)
static Spinlock page_lock;

#define MAX_ZONE_NUM 10

struct {
    Page *mem_base;
} zones[MAX_ZONE_NUM] = {{nullptr}};

static void buddy_init(void) {
    for (int i = 0; i <= MAX_ORDER; i++) {
        list_init(&free_list(i));
        nr_free(i) = 0;
    }
    initlock(&page_lock, "page_lock");
}

static void buddy_init_memmap(Page *base, size_t n) {
    static int zone_num = 0;
    assert(n > 0 && zone_num < MAX_ZONE_NUM);
    Page *p = base;
    for (; p != base + n; p++) {
        assert(PageReserved(p));
        p->flags = p->property = 0;
        p->zone_num = zone_num;
        set_page_ref(p, 0);
    }
    p = zones[zone_num++].mem_base = base;
    size_t order = MAX_ORDER, order_size = (1 << order);
    while (n != 0) {
        while (n >= order_size) {
            p->property = order;
            SetPageProperty(p);
            list_add(&free_list(order), &p->page_link);
            n -= order_size;
            p += order_size;
            nr_free(order)++;
        }
        order--;
        order_size >>= 1;
    }
}

static inline size_t getorder(size_t n) {
    size_t order, order_size;
    for (order = 0, order_size = 1; order <= MAX_ORDER; order++, order_size <<= 1) {
        if (n <= order_size) { return order; }
    }
    panic("getorder failed. %d\n", n);
    return 0;
}

static inline Page *buddy_alloc_pages_sub(size_t order) {
    acquire(&page_lock);
    assert(order <= MAX_ORDER);
    size_t cur_order;
    for (cur_order = order; cur_order <= MAX_ORDER; cur_order++) {
        if (!list_empty(&free_list(cur_order))) {
            List_entry *le = list_next(&free_list(cur_order));
            Page *page = le2page(le, page_link);
            nr_free(cur_order)--;
            list_del(le);
            size_t size = 1 << cur_order;
            while (cur_order > order) {
                cur_order--;
                size >>= 1;
                Page *buddy = page + size;
                buddy->property = cur_order;
                SetPageProperty(buddy);
                nr_free(cur_order)++;
                list_add(&free_list(cur_order), &(buddy->page_link));
            }
            ClearPageProperty(page);
            release(&page_lock);
            return page;
        }
    }
    release(&page_lock);
    return nullptr;
}

static Page *buddy_alloc_pages(size_t n) {
    assert(n > 0);
    size_t order = getorder(n), order_size = 1 << order;
    Page *page = buddy_alloc_pages_sub(order);
    if (page != nullptr && n != order_size) free_pages(page + n, order_size - n);
    return page;
}

static inline bool page_is_buddy(Page *page, size_t order, int zone_num) {
    if (page2ppn(page) < npage) {
        if (page->zone_num == zone_num) {
            return !PageReserved(page) && PageProperty(page) && page->property == order;
        }
    }
    return 0;
}

static inline ppn_t page2idx(Page *page) { return page - zones[page->zone_num].mem_base; }

static inline Page *idx2page(int zone_num, ppn_t idx) { return zones[zone_num].mem_base + idx; }

static void buddy_free_pages_sub(Page *base, size_t order) {
    ppn_t buddy_idx, page_idx = page2idx(base);
    acquire(&page_lock);
    assert((page_idx & ((1 << order) - 1)) == 0);
    Page *p = base;
    for (; p != base + (1 << order); p++) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = 0;
        set_page_ref(p, 0);
    }
    int zone_num = base->zone_num;
    while (order < MAX_ORDER) {
        buddy_idx = page_idx ^ (1 << order);
        Page *buddy = idx2page(zone_num, buddy_idx);
        if (!page_is_buddy(buddy, order, zone_num)) break;
        nr_free(order)--;
        list_del(&buddy->page_link);
        ClearPageProperty(buddy);
        page_idx &= buddy_idx;
        order++;
    }
    Page *page = idx2page(zone_num, page_idx);
    page->property = order;
    SetPageProperty(page);
    nr_free(order)++;
    list_add(&free_list(order), &(page->page_link));
    release(&page_lock);
}

static void buddy_free_pages(Page *base, size_t n) {
    assert(n > 0);
    if (n == 1)
        buddy_free_pages_sub(base, 0);
    else {
        size_t order = 0, order_size = 1;
        while (n > order_size) {
            assert(order <= MAX_ORDER);
            if ((page2idx(base) & order_size) != 0) {
                buddy_free_pages_sub(base, order);
                base += order_size;
                n -= order_size;
            }
            order++;
            order_size <<= 1;
        }
        while (n != 0) {
            while (n < order_size) {
                order--;
                order_size >>= 1;
            }
            buddy_free_pages_sub(base, order);
            base += order_size;
            n -= order_size;
        }
    }
}

static size_t buddy_nr_free_pages(void) {
    size_t ret = 0, order = 0;
    for (; order <= MAX_ORDER; order++) { ret += nr_free(order) * (1 << order); }
    return ret;
}

static void buddy_check(void){
    int i;
    size_t count = 0, total = 0;
    for(i = 0; i <= MAX_ORDER; i++){
        List_entry *list = &free_list(i), *le = list;
        while((le = list_next(le)) != list){
            Page *p = le2page(le, page_link);
            assert(PageProperty(p) && p->property == i);
            count++, total += (1 << i);
        }
    }
    assert(total == nr_free_pages());
    Page *p0 = alloc_pages(8), *buddy = alloc_pages(8), *p1;

    assert(p0 != nullptr);
    assert((page2idx(p0) & 7) == 0);
    assert(!PageProperty(p0));

    List_entry free_list_store[MAX_ORDER + 1];
    uint nr_free_store[MAX_ORDER + 1];
    for(i = 0; i <= MAX_ORDER; i++){
        free_list_store[i] = free_list(i);
        list_init(&free_list(i));
        assert(list_empty(&free_list(i)));
        nr_free_store[i] = nr_free(i);
        nr_free(i) = 0;
    }
    assert(nr_free_pages() == 0);
    assert(AllocPage() == nullptr);
    free_pages(p0, 8);
    assert(nr_free_pages() == 8);
    assert(PageProperty(p0) && p0->property == 3);
    assert((p0 = alloc_pages(6)) != nullptr && !PageProperty(p0) && nr_free_pages() == 2);

    assert((p1 = alloc_pages(2)) != nullptr && p1 == p0 + 6);
    assert(nr_free_pages() == 0);

    free_pages(p0, 3);
    assert(PageProperty(p0) && p0->property == 1);
    assert(PageProperty(p0 + 2) && p0[2].property == 0);

    free_pages(p0 + 3, 3);
    free_pages(p1, 2);

    assert(PageProperty(p0) && p0->property == 3);

    assert((p0 = alloc_pages(6)) != nullptr);
    assert((p1 = alloc_pages(2)) != nullptr);
    free_pages(p0 + 4, 2);
    free_pages(p1, 2);

    p1 = p0 + 4;
    assert(PageProperty(p1) && p1->property == 2);
    free_pages(p0, 4);
    assert(PageProperty(p0) && p0->property == 3);

    assert((p0 = alloc_pages(8)) != nullptr);
    assert(AllocPage() == nullptr && nr_free_pages() == 0);
    for(i = 0; i <= MAX_ORDER; i++){
        free_list(i) = free_list_store[i];
        nr_free(i) = nr_free_store[i];
    }
    free_pages(p0, 8);
    free_pages(buddy, 8);

    assert(total == nr_free_pages());
    for (i = 0; i <= MAX_ORDER; i++) {
        List_entry *list = &free_list(i), *le = list;
        while ((le = list_next(le)) != list) {
            Page *p = le2page(le, page_link);
            assert(PageProperty(p) && p->property == i);
            count--, total -= (1 << i);
        }
    }
    assert(count == 0);
    assert(total == 0);
}

const Pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};