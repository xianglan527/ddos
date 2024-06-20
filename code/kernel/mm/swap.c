#include "swap.h"

#include "assert.h"
#include "atomic.h"
#include "error.h"
#include "hash.h"
#include "pmm.h"
#include "string.h"
#include "swap.h"
#include "swapfs.h"
#include "vmm.h"
#include "slab.h"
#include "stdio.h"

size_t max_swap_offset;

typedef struct swap_list Swap_list;
struct swap_list {
    List_entry swap_list;
    size_t nr_pages;
};

static Swap_list active_list;

static Swap_list inactive_list;

#define nr_active_pages (active_list.nr_pages)
#define nr_inactive_pages (inactive_list.nr_pages)

static Atomic *mem_map;

static inline long mem_map_getvalue(int index) { return atomic_read(&mem_map[index]); }
static inline void mem_map_setvalue(int index, int value) { return atomic_set(&mem_map[index], value); }
static inline long mem_map_inc(int index) { return atomic_add_return(&mem_map[index], 1); }
static inline long mem_map_dec(int index) { return atomic_sub_return(&mem_map[index], 1); }

#define SWAP_UNSED 0xFFFFF
#define MAX_SWAP_REF 0xFFFFE

static volatile bool swap_init_ok = 0;

#define HASH_SHIFT 10
#define HASH_LIST_SIZE (1 << HASH_SHIFT)
#define entry_hashfn(x) (hash64(x, HASH_SHIFT))

static List_entry hash_list[HASH_LIST_SIZE];

static void check_swap(void);

static Spinlock swap_lock;  
                           
static void swap_list_init(Swap_list *list) {
    list_init(&list->swap_list);
    list->nr_pages = 0;
}

static inline void swap_active_list_add(Page *page) {
    acquire(&swap_lock);
    assert(PageSwap(page));
    SetPageActive(page);
    Swap_list *list = &active_list;
    list->nr_pages++;
    list_add_before(&list->swap_list, &page->swap_link);
    release(&swap_lock);
}

static inline void swap_inactive_list_add(Page *page) {
    acquire(&swap_lock);
    assert(PageSwap(page));
    ClearPageActive(page);
    Swap_list *list = &inactive_list;
    list->nr_pages++;
    list_add_before(&list->swap_list, &page->swap_link);
    release(&swap_lock);
}

static inline void swap_list_del(Page *page) {
    acquire(&swap_lock);
    assert(PageSwap(page));
    (PageActive(page) ? &active_list : &inactive_list)->nr_pages--;
    list_del(&page->swap_link);
    release(&swap_lock);
}

void swap_init(void) {

    swapfs_init();
    swap_list_init(&active_list);
    swap_list_init(&inactive_list);

    if (!(1024 <= max_swap_offset && max_swap_offset < MAX_SWAP_OFFSET_LIMIT))
        panic("bad max_swap_offset %08x.\n", max_swap_offset);
    mem_map = kmalloc(sizeof(*mem_map) * max_swap_offset);
    assert(mem_map != nullptr);

    for (size_t offset = 0; offset < max_swap_offset; offset++){
            mem_map_setvalue(offset, SWAP_UNSED);
    } 

    for (int i = 0; i < HASH_LIST_SIZE; i++) list_init(hash_list + i);
    initlock(&swap_lock, "swap_lock");
    check_swap();
}

bool try_free_pages(size_t n) {
    if (!swap_init_ok) return 0;
    panic("not implemented yet.!\n");
    return 0;
}

static Page *swap_hash_find(swap_entry_t entry) {
    List_entry *list = hash_list + entry_hashfn(entry), *le = list;
    while ((le = list_next(le)) != list) {
        Page *page = le2page(le, page_link);
        if (page->index == entry) return page;
    }
    return nullptr;
}

static void swap_page_del(Page *page) {
    acquire(&swap_lock);
    assert(PageSwap(page));
    ClearPageSwap(page);
    list_del(&page->page_link);
    release(&swap_lock);
}

static void swap_free_page(Page *page) {
    assert(PageSwap(page) && page_ref(page) == 0);
    swap_page_del(page);
    FreePage(page);
}

static swap_entry_t try_alloc_swap_entry(void) {
    static size_t next = 1;
    size_t emtpy = 0, zero = 0, end = next;
    do {
        switch (mem_map_getvalue(next)) {
            case SWAP_UNSED: emtpy = next; break;
            case 0:
                if (zero == 0) zero = next;
                break;
                if (++next == max_swap_offset) next = 1;
        }
        if (++next == max_swap_offset) next = 1;
    } while (emtpy == 0 && next != end);

    swap_entry_t entry = 0;
    if (emtpy != 0)
        entry = emtpy << 10;
    else if (zero != 0) {
        entry = zero << 10;
        Page *page = swap_hash_find(entry);
        assert(page != nullptr && PageSwap(page));
        swap_list_del(page);
        if (page_ref(page) == 0)
            swap_free_page(page);
        else
            swap_page_del(page);
        mem_map_setvalue(zero, SWAP_UNSED);
    }
    static uint failed_counter = 0;
    if (entry == 0 && ((++failed_counter) % 0x1000) == 0) {
        warn("swap: try_alloc_swap_entry: failed too many times.\n");
    }
    return entry;
}

static bool swap_page_add(Page *page, swap_entry_t entry) {
    acquire(&swap_lock);
    assert(!PageSwap(page));
    if (entry == 0) {
        if ((entry = try_alloc_swap_entry()) == 0){
            release(&swap_lock);
            return 0;
        }
        assert(mem_map_getvalue(swap_offset(entry)) == SWAP_UNSED);
        mem_map_setvalue(swap_offset(entry), 0);
        SetPageDirty(page);
    }
    SetPageSwap(page);
    page->index = entry;
    list_add(hash_list + entry_hashfn(entry), &page->page_link);
    release(&swap_lock);
    return 1;
}

void swap_remove_entry(swap_entry_t entry) {
    size_t offset = swap_offset(entry);
    assert(mem_map_getvalue(offset) > 0);
    if (mem_map_dec(offset) == 0) {
        Page *page = swap_hash_find(entry);
        if (page != nullptr) {
            if (page_ref(page) != 0) return;
            swap_list_del(page);
            swap_free_page(page);
        }
        mem_map_setvalue(offset, SWAP_UNSED);
    }
}

size_t swap_page_count(Page *page){
    if(!PageSwap(page))
        return 0;
    size_t offset = swap_offset(page->index);
    assert(mem_map_getvalue(offset) >= 0);
    return mem_map_getvalue(offset);
}

void swap_duplicate(swap_entry_t entry){
    size_t offset = swap_offset(entry);
    assert(mem_map_getvalue(offset) >= 0 && mem_map_getvalue(offset) < MAX_SWAP_REF);
    mem_map_inc(offset);
}

int swap_in_page(swap_entry_t entry, Page **pagep){
    if(pagep == nullptr)
        return -E_INVAL;
    size_t offset = swap_offset(entry);
    assert(mem_map_getvalue(offset) >= 0);
    int ret;
    Page *page, *newpage;
    if((page = swap_hash_find(entry)) != nullptr)
        goto found;
    newpage = AllocPage();

    if((page = swap_hash_find(entry)) != nullptr){
        if(newpage != nullptr)
            FreePage(newpage);
        goto found;
    }
    if(newpage == nullptr){
        ret = -E_NO_MEM;
        goto failed;
    }
    page = newpage;
    swapfs_read(entry, page);
    swap_page_add(page, entry);
    swap_active_list_add(page);

found:
    *pagep = page;
    return 0;
failed:
    return ret;
}

int swap_copy_entry(swap_entry_t entry, swap_entry_t *store){
    if(store == nullptr)
        return -E_INVAL;
    int ret = -E_NO_MEM;
    Page *page, *newpage;
    swap_duplicate(entry);
    if((newpage = AllocPage()) == nullptr)
        goto out;
    if((ret = swap_in_page(entry, &page)) != 0)
        goto failed_free_page;
    ret = -E_NO_MEM;
    if(!swap_page_add(newpage, 0))
        goto failed_free_page;
    swap_active_list_add(newpage);
    memcpy((void *)page2va(newpage), (void *)page2va(page), PGSIZE);
    *store = newpage->index;
    ret = 0;
    goto out;

failed_free_page:
    FreePage(newpage);
out:
    swap_remove_entry(entry);
    return ret;
}

static bool try_free_swap_entry(swap_entry_t entry){
    size_t offset = swap_offset(entry);
    if(mem_map_getvalue(offset) == 0){
        mem_map_setvalue(offset, SWAP_UNSED);
        return 1;
    }
    return 0;
}

static int page_launder(void){
    size_t maxscan = nr_inactive_pages, free_count = 0;
    List_entry *list = &inactive_list.swap_list, *le = list_next(list);
    while(maxscan-- > 0 && le != list){
        Page *page = le2page(le, swap_link);
        le = list_next(le);
        if(!(PageSwap(page) && !PageActive(page)))
            panic("inactive: wrong swap list.\n");
        swap_list_del(page);
        if(page_ref(page) != 0){
            swap_active_list_add(page);
            continue;
        }
        swap_entry_t entry = page->index;
        if(!try_free_swap_entry(entry)){
            if(PageDirty(page)){
                ClearPageDirty(page);
                swap_duplicate(entry);
                swapfs_write(entry, page);
                mem_map_dec(swap_offset(entry));
                if(page_ref(page) != 0){
                    swap_active_list_add(page);
                    continue;
                }
                // if(PageDirty(page)){
                //     swap_inactive_list_add(page);
                //     continue;;
                // }
                try_free_swap_entry(entry);
            }
        }
        free_count++;
        swap_free_page(page);
    }
    return free_count;
}

static void refill_inactive_scan(void){
    size_t maxscan = nr_active_pages;
    List_entry *list = &active_list.swap_list, *le = list_next(list);
    while(maxscan-- > 0 && le != list){
        Page *page = le2page(le, swap_link);
        le = list_next(le);
        if(!(PageSwap(page) && PageActive(page)))
            panic("active: wrong swap list.\n");
        if(page_ref(page) == 0){
            swap_list_del(page);
            swap_inactive_list_add(page);
        }
    }
}

static int swap_out_vma(Mm_struct *mm, Vma_struct *vma, uintptr_t addr, size_t require){
    if(require == 0 || !(addr >= vma->vm_start && addr < vma->vm_end))
        return 0;
    uintptr_t end;
    size_t free_count = 0;
    addr = PGROUNDDOWN(addr), end = PGROUNDUP(vma->vm_end);
    while(addr < end && require != 0){
        pte_t *ptep = get_pte(mm->pagetable, addr, 0);
        if(ptep == nullptr){
            addr = PGROUNDUP(addr + PGSIZE);
            continue;
        }
        if(*ptep & PTE_V){
            Page *page = pte2page(*ptep);
            assert(!PageReserved(page));
            if(*ptep & PTE_A){
                *ptep &= ~PTE_A;
                tlb_invalidate(mm->pagetable, addr);
                goto try_next_entry;
            }
            if(!PageSwap(page)){
                if(!swap_page_add(page, 0))
                    goto try_next_entry;
                swap_active_list_add(page);
            }else if(*ptep & PTE_D){
                SetPageDirty(page);
            }
            swap_entry_t entry = page->index;
            swap_duplicate(entry);
            page_ref_dec(page);
            *ptep = entry;
            tlb_invalidate(mm->pagetable, addr);
            mm->swap_address = addr + PGSIZE;
            free_count++, require--;
        }
try_next_entry:
        addr += PGSIZE;
    }
    return free_count;
}

static int swap_out_mm(Mm_struct *mm, size_t require){
    assert(mm != nullptr);
    if(require == 0 || mm->map_count == 0)
        return 0;
    assert(!list_empty(&mm->mmap_list));
    uintptr_t addr = mm->swap_address;
    Vma_struct *vma;
    if((vma = find_vma(mm, addr)) == nullptr){
        addr = mm->swap_address = 0;
        vma = le2vma(list_next(&mm->mmap_list), list_link);
    }
    assert(vma != nullptr && addr <= vma->vm_end);

    if(addr < vma->vm_start)
        addr = vma->vm_start;
    size_t free_count = 0;
    for(int i = 0; i < mm->map_count; i++){
        int ret = swap_out_vma(mm, vma, addr, require);
        free_count += ret, require -= ret;
        if(require == 0)
            break;
        List_entry *le = list_next(&vma->list_link);
        if(le == &mm->mmap_list)
            le = list_next(le);
        vma = le2vma(le, list_link);
        addr = vma->vm_start;
    }
    return free_count;
}

static void check_swap(void){

    size_t nr_free_pages_store = nr_free_pages();
    size_t slab_allocated_store = slab_allocated();

    size_t offset;
    for(offset = 2; offset < max_swap_offset; offset++){
        mem_map_setvalue(offset, 1);
    }
    Mm_struct *mm = mm_create();
    assert(mm != nullptr);
    extern Mm_struct *check_mm_struct;
    assert(check_mm_struct == nullptr);
    check_mm_struct = mm;
    // Mm_struct *mm;
    // mm = check_mm_struct;
    pagetable_t *pgdir = mm->pagetable = kernel_pagetable;
    
    Vma_struct *vma = vma_create(0, PT2PGSIZE, VM_WRITE | VM_READ);
    assert(vma != nullptr);

    insert_vma_struct(mm, vma);

    Page *rp0 = AllocPage(), *rp1 = AllocPage();
    assert(rp0 != nullptr && rp1 != nullptr);

    uint32_t perm = PTE_U | PTE_W;
    int ret = page_insert(pgdir, rp1, 0, perm);
    assert(ret == 0 && page_ref(rp1) == 1);

    page_ref_inc(rp1);
    ret = page_insert(pgdir, rp0, 0, perm);
    assert(ret == 0 && page_ref(rp1) == 1 && page_ref(rp0) == 1);

    swap_entry_t entry = try_alloc_swap_entry();
    assert(swap_offset(entry) == 1);
    mem_map_setvalue(1, 1);
    assert(try_alloc_swap_entry() == 0);

    swap_page_add(rp1, entry);
    swap_active_list_add(rp1);
    assert(PageSwap(rp1));
    mem_map_setvalue(1, 0);
    entry = try_alloc_swap_entry();
    assert(swap_offset(entry) == 1);
    assert(!PageSwap(rp1));

    assert(swap_hash_find(entry) == nullptr);
    mem_map_setvalue(1, 2);
    swap_remove_entry(entry);
    assert(mem_map_getvalue(1) == 1);

    swap_page_add(rp1, entry);
    swap_inactive_list_add(rp1);
    swap_remove_entry(entry);
    assert(PageSwap(rp1));
    assert(rp1->index == entry && mem_map_getvalue(1) == 0);

    assert(page_ref(rp1) == 1);
    assert(nr_active_pages == 0 && nr_inactive_pages == 1);
    assert(list_next(&inactive_list.swap_list) == &rp1->swap_link);

    page_launder();
    assert(nr_active_pages == 1 && nr_inactive_pages == 0);
    assert(PageSwap(rp1) && PageActive(rp1));

    entry = try_alloc_swap_entry();
    assert(swap_offset(entry) == 1);
    assert(!PageSwap(rp1) && nr_active_pages == 0);
    assert(list_empty(&active_list.swap_list));

    assert(page_ref(rp1) == 1);
    swap_page_add(rp1, 0);
    assert(PageSwap(rp1) && swap_offset(rp1->index) == 1);
    swap_inactive_list_add(rp1);
    mem_map_setvalue(1, 1);
    assert(nr_inactive_pages == 1);
    page_ref_dec(rp1);

    size_t count = nr_free_pages();
    swap_remove_entry(entry);
    assert(nr_inactive_pages == 0 && nr_free_pages() == count + 1);

    pte_t *ptep0 = get_pte(pgdir, 0, 0), *ptep1;
    assert(ptep0 != nullptr && pte2page(*ptep0) == rp0);

    ret = swap_out_mm(mm, 0);
    assert(ret == 0);

    ret = swap_out_mm(mm, 10);
    assert(ret == 1 && mm->swap_address == PGSIZE);

    ret = swap_out_mm(mm, 10);
    assert(ret == 0 && *ptep0 == entry && mem_map_getvalue(1) == 1);
    assert(PageDirty(rp0) && PageActive(rp0) && page_ref(rp0) == 0);
    assert(nr_active_pages == 1 && list_next(&active_list.swap_list) == &rp0->swap_link);

    refill_inactive_scan();
    assert(!PageActive(rp0) && page_ref(rp0) == 0);
    assert(nr_inactive_pages == 1 && list_next(&inactive_list.swap_list) == &rp0->swap_link);

    page_ref_inc(rp0);
    page_launder();
    assert(PageActive(rp0) && page_ref(rp0) == 1);
    assert(nr_active_pages == 1 && list_next(&active_list.swap_list) == &rp0->swap_link);

    page_ref_dec(rp0);
    refill_inactive_scan();
    assert(!PageActive(rp0));

    int i;
    for (i = 0; i < PGSIZE; i++) { ((char *)page2va(rp0))[i] = (char)i; }
    page_launder();
    assert(nr_inactive_pages == 0 && list_empty(&inactive_list.swap_list));
    assert(mem_map_getvalue(1) == 1);

    rp1 = AllocPage();
    assert(rp1 != nullptr);
    swapfs_read(entry, rp1);
    for(i = 0; i < PGSIZE; i++){
        assert(((char *)page2va(rp0))[i] == (char)i);
    }

    *(char *)0 = 0xEF;
    rp0 = pte2page(*ptep0);
    assert(page_ref(rp0) == 1);
    assert(PageSwap(rp0) && PageActive(rp0));

    entry = try_alloc_swap_entry();
    assert(swap_offset(entry) == 1 && mem_map_getvalue(1) == SWAP_UNSED);
    assert(!PageSwap(rp0) && nr_active_pages == 0 && nr_inactive_pages == 0);

    assert(rp0 == pte2page(*ptep0));
    assert(!PageSwap(rp0));

    // vm_print(kernel_pagetable);
    // print_vma_list(mm);

    mm->swap_address = 0;
    ret = swap_out_mm(mm, 10);
    assert(ret == 0);
    assert(!PageSwap(rp0) && (*ptep0 & PTE_V));

    ret = swap_out_mm(mm, 10);
    assert(ret == 1);
    assert(*ptep0 == entry && page_ref(rp0) == 0 && mem_map_getvalue(1) == 1);

    count = nr_free_pages();
    refill_inactive_scan();
    page_launder();
    assert(count + 1 == nr_free_pages());

    swapfs_read(entry, rp1);
    assert(*(char *)(page2va(rp1)) == (char)0xEF);
    FreePage(rp1);

    assert(mem_map_getvalue(1) == 1);
    ptep1 = get_pte(pgdir, PGSIZE, 0);
    assert(ptep1 != nullptr && *ptep1 == 0);
    swap_duplicate(*ptep0);
    *ptep1 = *ptep0;
    assert(swap_offset(*ptep1) == 1 && mem_map_getvalue(1) == 2);


    *(char *)0 = 0xFF;
    *(char *)(PGSIZE + 1) = 0x88;
    assert(pte2page(*ptep0) == pte2page(*ptep1));
    rp0 = pte2page(*ptep0);
    assert(*(char *)1 == (char)0x88 && *(char *)PGSIZE == (char)0xFF);

    assert(page_ref(rp0) == 2 && rp0->index == entry && mem_map_getvalue(1) == 0);

    assert(PageSwap(rp0) && PageActive(rp0));
    entry = try_alloc_swap_entry();
    assert(swap_offset(entry) == 1 && mem_map_getvalue(1) == SWAP_UNSED);
    assert(!PageSwap(rp0));
    assert(list_empty(&active_list.swap_list));
    assert(list_empty(&inactive_list.swap_list));

    *(char *)0 = *(char *)PGSIZE = 0xEE;
    mm->swap_address = 0;
    ret = swap_out_mm(mm, 2);
    assert(ret == 0);
    assert((*ptep0 & PTE_V) && !(*ptep0 & PTE_A));
    assert((*ptep1 & PTE_V) && !(*ptep1 & PTE_A));

    ret = swap_out_mm(mm, 2);
    assert(ret == 2);
    assert(mem_map_getvalue(1) == 2 && page_ref(rp0) == 0);

    refill_inactive_scan();
    page_launder();
    assert(mem_map_getvalue(1) == 2 && swap_hash_find(entry) == nullptr);

    swap_remove_entry(entry);
    *ptep1 = 0;
    assert(mem_map_getvalue(1) == 1);

    swap_entry_t store;
    ret = swap_copy_entry(entry, &store);
    assert(ret == -E_NO_MEM);
    mem_map_setvalue(2, SWAP_UNSED);

    ret = swap_copy_entry(entry, &store);
    assert(ret == 0 && swap_offset(store) == 2 && mem_map_getvalue(2) == 0);
    mem_map_setvalue(2, 1);
    *ptep1 = store;

    assert(*(char *)PGSIZE == (char)0xEE && *(char *)(PGSIZE + 1) == (char)0x88);

    *(char *)PGSIZE = 1, *(char *)(PGSIZE + 1) = 2;
    assert(*(char *)0 == (char)0xEE && *(char *)1 == (char)0x88);

    ret = swap_in_page(entry, &rp0);
    assert(ret == 0);
    ret = swap_in_page(store, &rp1);   
 
   
    assert(ret == 0);
    assert(rp1 != rp0);

    swap_list_del(rp0), swap_list_del(rp1);
    swap_page_del(rp0), swap_page_del(rp1);

    assert(page_ref(rp0) == 1 && page_ref(rp1) == 1);
    assert(nr_active_pages == 0 && list_empty(&active_list.swap_list));
    assert(nr_inactive_pages == 0 && list_empty(&inactive_list.swap_list));

    for(i = 0; i < HASH_LIST_SIZE; i++){
        assert(list_empty(hash_list + i));
    }

    page_remove(pgdir, 0);
    page_remove(pgdir, PGSIZE);

    pte_t pg1 = kernel_pagetable[0];
    pte_t pg2 = ((pte_t *)PTE2PA(pg1))[0];
    ((pte_t *)PTE2PA(pg1))[0] = 0;
    FreePage(pa2page(PTE2PA(pg2)));


    // vm_print(kernel_pagetable);

    assert(nr_active_pages == 0 && nr_inactive_pages == 0);
    for (offset = 0; offset < max_swap_offset; offset++) { mem_map_setvalue(offset,SWAP_UNSED); }

    mm_destroy(mm);
    int pp1 = nr_free_pages();
    assert(nr_free_pages_store == nr_free_pages());
    int pp = slab_allocated();
    assert(slab_allocated_store == slab_allocated());

    cprintf("check_swap() succeeded.\n");
}