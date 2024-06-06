#include "pmm.h"

static Spinlock page_lock;
static Free_area free_area;
#define free_list (free_area.free_list)
#define nr_free (free_area.nr_free)

static void default_init(void){
    list_init(&free_list);
    nr_free = 0;
    initlock(&page_lock, "page_lock");
}

static void default_init_memmap(Page *base, size_t n){
    Page *p = base;
    for(; p != base + n; p++){
        assert(PageReserved(p));
        p->flags = 0;
        set_page_ref(p, 0);
        list_add(&free_list, &p->page_link);
    }
    nr_free += n;
}

static Page *default_alloc_pages(size_t n){
    List_entry *le;
    acquire(&page_lock);
    if((le = list_next(&free_list)) != &free_list){
        nr_free--;
        list_del(le);
    release(&page_lock);
        return le2page(le, page_link);
    }
    release(&page_lock);
    return nullptr;
}

static void default_free_pages(Page *base, size_t n){
    acquire(&page_lock);
    assert(!PageReserved(base));
    base->flags = 0;
    set_page_ref(base, 0);
    nr_free++;
    list_add(&free_list, &base->page_link);
    release(&page_lock);
}

static void basic_check(void){
    Page *p0, *p1, *p2;
    p0 = p1 = p2 = nullptr;
    assert((p0 = AllocPage()) != nullptr);
    assert((p1 = AllocPage()) != nullptr);
    assert((p2 = AllocPage()) != nullptr);

    assert(p0 != p1 && p0 != p2 && p1 != p2);
    assert(page_ref(p0) == 0 && page_ref(p1) == 0 && page_ref(p2) == 0);

    assert(page2pa(p0) < PHYSTOP);
    assert(page2pa(p1) < PHYSTOP);
    assert(page2pa(p1) < PHYSTOP);

    List_entry free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));

    size_t nr_free_store = nr_free;
    nr_free = 0;
    assert(AllocPage() == nullptr);
    FreePage(p0);
    FreePage(p1);
    FreePage(p2);
    assert(nr_free == 3);

    assert((p0 = AllocPage()) != nullptr);
    assert((p1 = AllocPage()) != nullptr);
    assert((p2 = AllocPage()) != nullptr);

    assert(AllocPage() == nullptr);

    FreePage(p0);
    assert(!list_empty(&free_list));

    Page *p;
    assert((p = AllocPage()) == p0);
    assert(AllocPage() == nullptr);

    assert(nr_free == 0);
    free_list = free_list_store;
    nr_free = nr_free_store;

    FreePage(p);
    FreePage(p1);
    FreePage(p2);

}

static void default_check(void){
    basic_check();
}

static size_t default_nr_free_pages(void) { return nr_free; }

const Pmm_manager default_pmm_manager = {
    .name = "default_pmm_manager",
    .init = default_init,
    .init_memmap = default_init_memmap,
    .alloc_pages = default_alloc_pages,
    .free_pages = default_free_pages,
    .nr_free_pages = default_nr_free_pages,
    .check = default_check,
};