#include "swap.h"

#include "assert.h"
#include "atomic.h"
#include "config.h"
#include "error.h"
#include "hash.h"
#include "pmm.h"
#include "proc.h"
#include "shmem.h"
#include "slab.h"
#include "stdio.h"
#include "string.h"
#include "swap.h"
#include "swapfs.h"
#include "vmm.h"
#include "wait.h"

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

#define SWAP_UNUSED 0xFFFFF
#define MAX_SWAP_REF 0xFFFFE

static volatile bool swap_init_ok = 0;

#define HASH_SHIFT 10
#define HASH_LIST_SIZE (1 << HASH_SHIFT)
#define entry_hashfn(x) (hash64(x, HASH_SHIFT))

static List_entry hash_list[HASH_LIST_SIZE];

static void check_swap(void);
static void check_mm_swap(void);
static void check_mm_shm_swap(void);

static Spinlock swap_lock;

static Atomic pressure;
static Wait_queue kswapd_done;
static Spinlock kswapd_done_lock;

extern Spinlock proc_mm_list_lock;

static void swap_list_init(Swap_list *list) {
    list_init(&list->swap_list);
    list->nr_pages = 0;
}

static inline void swap_active_list_add(Page *page) {
    assert(PageSwap(page));
    SetPageActive(page);
    Swap_list *list = &active_list;
    list->nr_pages++;
    list_add_before(&list->swap_list, &page->swap_link);
}

static inline void swap_inactive_list_add(Page *page) {
    assert(PageSwap(page));
    ClearPageActive(page);
    Swap_list *list = &inactive_list;
    list->nr_pages++;
    list_add_before(&list->swap_list, &page->swap_link);
}

static inline void swap_list_del(Page *page) {
    assert(PageSwap(page));
    (PageActive(page) ? &active_list : &inactive_list)->nr_pages--;
    list_del(&page->swap_link);
}

void swap_init(void) {
    swapfs_init();
    swap_list_init(&active_list);
    swap_list_init(&inactive_list);

    if (!(1024 <= max_swap_offset && max_swap_offset < MAX_SWAP_OFFSET_LIMIT))
        panic("bad max_swap_offset %08x.\n", max_swap_offset);
    mem_map = kmalloc(sizeof(*mem_map) * max_swap_offset);
    assert(mem_map != nullptr);

    for (size_t offset = 0; offset < max_swap_offset; offset++) { mem_map_setvalue(offset, SWAP_UNUSED); }

    for (int i = 0; i < HASH_LIST_SIZE; i++) list_init(hash_list + i);
    initlock(&swap_lock, "swap_lock");
    atomic_set(&pressure, 0);
    wait_queue_init(&kswapd_done);
    initlock(&kswapd_done_lock, "kswapd_done_lock");
    swap_init_ok = true;
#ifdef PRINT_MM_TEST
    check_swap();
    check_mm_swap();
    check_mm_shm_swap();
#endif
}

static Page *swap_hash_find(swap_entry_t entry) {
    List_entry *list = hash_list + entry_hashfn(entry), *le = list;
    while ((le = list_next(le)) != list) {
        Page *page = le2page(le, page_link);
        if (page->index == entry) return page;
    }
    return nullptr;
}

size_t swap_page_num(void) {
    size_t nums = 0;
    acquire(&swap_lock);
    for (int i = 0; i < HASH_LIST_SIZE; i++) {
        List_entry *list = &hash_list[i], *le = list;
        while ((le = list_next(le)) != list) { nums++; }
    }
    release(&swap_lock);
    return nums;
}

size_t swap_page_inactive_num(void) { return nr_inactive_pages; }

size_t swap_page_active_num(void) { return nr_active_pages; }

static void swap_page_del(Page *page) {
    assert(PageSwap(page));
    ClearPageSwap(page);
    list_del(&page->page_link);
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
            case SWAP_UNUSED: emtpy = next; break;
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
        mem_map_setvalue(zero, SWAP_UNUSED);
    }
    static uint failed_counter = 0;
    if (entry == 0 && ((++failed_counter) % 0x1000) == 0) {
        warn("swap: try_alloc_swap_entry: failed too many times.\n");
    }
    return entry;
}

bool try_free_pages(size_t n) {
    if (!swap_init_ok || daemonproc == nullptr) return false;
    Proc *current = myproc();
    if (current == daemonproc) panic("daemon process call try_free_pages.\n");
    if (n >= 1 << 7) { return false; }
    atomic_add(&pressure, (long)n);
    Wait __wait, *wait = &__wait;
    wait_init(wait, current);
    acquire(&kswapd_done_lock);
    acquire(&current->lock);
    current->state = SLEEPING;
    current->wait_state = WT_KSWAPD;
    release(&current->lock);
    wait_queue_add(&kswapd_done, wait);
    release(&kswapd_done_lock);
    acquire(&current->lock);
    set_proc_cpu(current, mycpu());
    sleeping(current, &current->lock);
    assert(current->set_cpu == mycpu());
    clear_proc_cpu(current, mycpu());
    release(&current->lock);
    assert(!wait_in_queue(wait) && wait->wakeup_flags == WT_KSWAPD);
    return true;
}

static void kswapd_wakeup_all(void) {
    acquire(&kswapd_done_lock);
    wakeup_queue(&kswapd_done, WT_KSWAPD, 1);
    release(&kswapd_done_lock);
}

static bool swap_page_add(Page *page, swap_entry_t entry) {
    assert(!PageSwap(page));
    if (entry == 0) {
        if ((entry = try_alloc_swap_entry()) == 0) { return 0; }
        assert(mem_map_getvalue(swap_offset(entry)) == SWAP_UNUSED);
        mem_map_setvalue(swap_offset(entry), 0);
        SetPageDirty(page);
    }
    SetPageSwap(page);
    page->index = entry;
    list_add(hash_list + entry_hashfn(entry), &page->page_link);
    return 1;
}

void swap_remove_entry(swap_entry_t entry) {
    acquire(&swap_lock);
    size_t offset = swap_offset(entry);
    assert(mem_map_getvalue(offset) > 0);
    if (mem_map_dec(offset) == 0) {
        Page *page = swap_hash_find(entry);
        if (page != nullptr) {
            swap_list_del(page);
            if (page_ref(page) != 0)
                swap_page_del(page);
            else
                swap_free_page(page);
        }
        mem_map_setvalue(offset, SWAP_UNUSED);
    }
    release(&swap_lock);
}

size_t swap_page_count(Page *page) {
    if (!PageSwap(page)) return 0;
    size_t offset = swap_offset(page->index);
    assert(mem_map_getvalue(offset) >= 0);
    return mem_map_getvalue(offset);
}

void swap_duplicate(swap_entry_t entry) {
    size_t offset = swap_offset(entry);
    assert(mem_map_getvalue(offset) >= 0 && mem_map_getvalue(offset) < MAX_SWAP_REF);
    mem_map_inc(offset);
}

int swap_in_page(swap_entry_t entry, Page **pagep) {
    if (pagep == nullptr) return -E_INVAL;
    size_t offset = swap_offset(entry);
    assert(mem_map_getvalue(offset) >= 0);
    int ret;
    Page *page, *newpage;
    acquire(&swap_lock);
    if ((page = swap_hash_find(entry)) != nullptr) {
        goto found;
    }
    release(&swap_lock);
    newpage = AllocPage();
    acquire(&swap_lock);
    if ((page = swap_hash_find(entry)) != nullptr) {
        if (newpage != nullptr) FreePage(newpage);
        goto found;
    }
    if (newpage == nullptr) {
        ret = -E_NO_MEM;
        goto failed;
    }
    page = newpage;
    swapfs_read_syn(entry, page);
    swap_page_add(page, entry);
    swap_active_list_add(page);

found:
    release(&swap_lock);
    *pagep = page;
    return 0;
failed:
    release(&swap_lock);
    return ret;
}

int swap_copy_entry(swap_entry_t entry, swap_entry_t *store) {
    if (store == nullptr) return -E_INVAL;
    int ret = -E_NO_MEM;
    Page *page, *newpage;
    swap_duplicate(entry);
    if ((newpage = AllocPage()) == nullptr) goto out;
    if ((ret = swap_in_page(entry, &page)) != 0) goto failed_free_page;
    ret = -E_NO_MEM;
    if (!swap_page_add(newpage, 0)) goto failed_free_page;
    swap_active_list_add(newpage);
    memcpy((void *)page2kva(newpage), (void *)page2kva(page), PGSIZE);
    *store = newpage->index;
    ret = 0;
    goto out;

failed_free_page:
    FreePage(newpage);
out:
    swap_remove_entry(entry);
    return ret;
}

static bool try_free_swap_entry(swap_entry_t entry) {
    size_t offset = swap_offset(entry);
    if (mem_map_getvalue(offset) == 0) {
        mem_map_setvalue(offset, SWAP_UNUSED);
        return 1;
    }
    return 0;
}

static int page_launder(void) {
    size_t maxscan = nr_inactive_pages, free_count = 0;
    acquire(&swap_lock);
    List_entry *list = &inactive_list.swap_list, *le = list_next(list);
    while (maxscan-- > 0 && le != list) {
        Page *page = le2page(le, swap_link);
        le = list_next(le);
        if (!(PageSwap(page) && !PageActive(page))) panic("inactive: wrong swap list.\n");
        swap_list_del(page);
        if (page_ref(page) != 0) {
            swap_active_list_add(page);
            continue;
        }
        swap_entry_t entry = page->index;
        if (!try_free_swap_entry(entry)) {
            if (PageDirty(page)) {
                ClearPageDirty(page);
                swap_duplicate(entry);
                swapfs_write_syn(entry, page);
                mem_map_dec(swap_offset(entry));
                if (page_ref(page) != 0) {
                    swap_active_list_add(page);
                    continue;
                }
                try_free_swap_entry(entry);
            }
        }
        free_count++;
        swap_free_page(page);
    }
    release(&swap_lock);
    return free_count;
}

static void refill_inactive_scan(void) {
    size_t maxscan = nr_active_pages;
    acquire(&swap_lock);
    List_entry *list = &active_list.swap_list, *le = list_next(list);
    while (maxscan-- > 0 && le != list) {
        Page *page = le2page(le, swap_link);
        le = list_next(le);
        if (!(PageSwap(page) && PageActive(page))) panic("active: wrong swap list.\n");
        if (page_ref(page) == 0) {
            swap_list_del(page);
            swap_inactive_list_add(page);
        }
    }
    release(&swap_lock);
}

static int swap_out_vma(Mm_struct *mm, Vma_struct *vma, uintptr_t addr, size_t require) {
    if (require == 0 || !(addr >= vma->vm_start && addr < vma->vm_end)) return 0;
    if ((vma->vm_flags & VM_STACK) == VM_STACK) return 0;
    uintptr_t end;
    size_t free_count = 0;
    addr = PGROUNDDOWN(addr), end = PGROUNDUP(vma->vm_end);
    while (addr < end && require != 0) {
        pte_t *ptep = get_pte(mm->pagetable, addr, 0);
        if (ptep == nullptr) {
            addr = ROUNDDOWN(addr + PT2PGSIZE, PT2PGSIZE);
            continue;
        }
        if (*ptep & PTE_V) {
            Page *page = pte2page(*ptep);
            assert(!PageReserved(page));
            if (*ptep & PTE_A) {
                *ptep &= ~PTE_A;
                tlb_invalidate(mm->pagetable, addr);
                goto try_next_entry;
            }
            if (!PageSwap(page)) {
                acquire(&swap_lock);
                if (!swap_page_add(page, 0)) {
                    release(&swap_lock);
                    goto try_next_entry;
                }
                swap_active_list_add(page);
                release(&swap_lock);
            } else if (*ptep & PTE_D) {
                SetPageDirty(page);
            }
            swap_entry_t entry = page->index;
            swap_duplicate(entry);
            page_ref_dec(page);
            *ptep = entry;
            tlb_invalidate(mm->pagetable, addr);
            mm->swap_address = addr + PGSIZE;
            free_count++, require--;
            if ((vma->vm_flags & VM_SHARE) && page_ref(page) == 1) {
                uintptr_t shmem_addr = addr - vma->vm_start + vma->shmem_off;
                pte_t *sh_ptep = shmem_get_entry(vma->shmem, shmem_addr, 0);
                assert(sh_ptep != nullptr && *sh_ptep != 0);
                if (*sh_ptep & PTE_V) { shmem_insert_entry(vma->shmem, shmem_addr, entry); }
            }
        }
    try_next_entry:
        addr += PGSIZE;
    }
    return free_count;
}

static int swap_out_mm(Mm_struct *mm, size_t require) {
    assert(mm != nullptr);
    if (require == 0 || mm->map_count == 0) return 0;
    assert(!list_empty(&mm->mmap_list));
    uintptr_t addr = mm->swap_address;
    Vma_struct *vma;
    if ((vma = find_vma(mm, addr)) == nullptr) {
        addr = mm->swap_address = 0;
        vma = le2vma(list_next(&mm->mmap_list), list_link);
    }
    assert(vma != nullptr && addr <= vma->vm_end);

    if (addr < vma->vm_start) addr = vma->vm_start;
    size_t free_count = 0;
    for (int i = 0; i < mm->map_count; i++) {
        int ret = swap_out_vma(mm, vma, addr, require);
        free_count += ret, require -= ret;
        if (require == 0) break;
        List_entry *le = list_next(&vma->list_link);
        if (le == &mm->mmap_list) le = list_next(le);
        vma = le2vma(le, list_link);
        addr = vma->vm_start;
    }
    return free_count;
}

void dump_proc_mm_list(void) {
    // acquire(&proc_mm_list_lock);
    cprintf("\nproc_mm_list dump....................................\n\n");
    List_entry *le = &proc_mm_list;
    while ((le = list_next(le)) != &proc_mm_list) {
        Mm_struct *mm = le2mm(le, proc_mm_link);
        cprintf("proc_mm_list pid is %d\n", mm->proc->pid);
    }
    cprintf("\nend of proc_mm_list dump.............................\n");
    // release(&proc_mm_list_lock);
}

void kswap_main(void) {
    static int guard = 0;
    size_t launder_pages_num = 0;
    long __pressure;
    long free_page_size = 0;
    size_t needs = 0;
    size_t rounds;
repeat:
    free_page_size = nr_free_pages();
    assert(free_page_size >= 0);
    __pressure = atomic_read(&pressure);
    if (__pressure > 0) {
        size_t nr_free_pages_store1 = nr_free_pages();
        needs = (__pressure << 5), rounds = 16;
        acquire(&proc_mm_list_lock);
        List_entry *list = &proc_mm_list;
        assert(!list_empty(list));
        while (needs > 0 && rounds-- > 0) {
            List_entry *le = list_next(list);
            list_del(le);
            list_add_before(list, le);
            Mm_struct *mm = le2mm(le, proc_mm_link);
            needs -= swap_out_mm(mm, (needs < 32) ? needs : 32);
        }
        release(&proc_mm_list_lock);
    }
    refill_inactive_scan();
    launder_pages_num = page_launder();
    __pressure -= launder_pages_num;
    if (__pressure > 0) {
        if (++guard >= 1000) {
            guard = 0;
            warn("kswaped: may out of memory");
        }
        goto repeat;
    }
    atomic_set(&pressure, 0);
    guard = 0;
    kswapd_wakeup_all();
    do_sleep(10);
}

static void check_swap(void) {
    size_t nr_free_pages_store = nr_free_pages();
    size_t slab_allocated_store = slab_allocated();

    size_t offset;
    for (offset = 2; offset < max_swap_offset; offset++) { mem_map_setvalue(offset, 1); }
    Mm_struct *mm = mm_create();
    assert(mm != nullptr);
    extern Mm_struct *check_mm_struct;
    assert(check_mm_struct == nullptr);
    check_mm_struct = mm;
    pagetable_t *pgdir = mm->pagetable = kernel_pagetable;

    Vma_struct *vma = vma_create(0, PT2PGSIZE, VM_WRITE | VM_READ);
    assert(vma != nullptr);

    insert_vma_struct(mm, vma);

    Page *rp0 = AllocPage(), *rp1 = AllocPage();
    assert(rp0 != nullptr && rp1 != nullptr);

    uint32_t perm = PTE_W;
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
    // assert(PageSwap(rp1));
    // assert(rp1->index == entry && mem_map_getvalue(1) == 0);

    assert(page_ref(rp1) == 1);
    // assert(nr_active_pages == 0 && nr_inactive_pages == 1);
    // assert(list_next(&inactive_list.swap_list) == &rp1->swap_link);

    page_launder();
    // assert(nr_active_pages == 1 && nr_inactive_pages == 0);
    // assert(PageSwap(rp1) && PageActive(rp1));

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
    for (i = 0; i < PGSIZE; i++) { ((char *)page2kva(rp0))[i] = (char)i; }
    page_launder();
    assert(nr_inactive_pages == 0 && list_empty(&inactive_list.swap_list));
    assert(mem_map_getvalue(1) == 1);

    rp1 = AllocPage();
    assert(rp1 != nullptr);
    swapfs_read(entry, rp1);
    for (i = 0; i < PGSIZE; i++) { assert(((char *)page2kva(rp0))[i] == (char)i); }

    *(char *)0 = 0xEF;
    rp0 = pte2page(*ptep0);
    assert(page_ref(rp0) == 1);
    // assert(PageSwap(rp0) && PageActive(rp0));

    entry = try_alloc_swap_entry();
    assert(swap_offset(entry) == 1 && mem_map_getvalue(1) == SWAP_UNUSED);
    assert(!PageSwap(rp0) && nr_active_pages == 0 && nr_inactive_pages == 0);

    assert(rp0 == pte2page(*ptep0));
    assert(!PageSwap(rp0));

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
    assert(*(char *)(page2kva(rp1)) == (char)0xEF);
    FreePage(rp1);

    assert(mem_map_getvalue(1) == 1);
    ptep1 = get_pte(pgdir, PGSIZE, 0);
    assert(ptep1 != nullptr && *ptep1 == 0);
    swap_duplicate(*ptep0);
    *ptep1 = *ptep0;
    assert(swap_offset(*ptep1) == 1 && mem_map_getvalue(1) == 2);

    *(char *)1 = 0x88;
    *(char *)(PGSIZE) = 0x8F;
    *(char *)(PGSIZE + 1) = 0xFF;
    assert(pte2page(*ptep0) != pte2page(*ptep1));
    assert(*(char *)0 == (char)0xEF);
    assert(*(char *)1 == (char)0x88);
    assert(*(char *)(PGSIZE) == (char)0x8F);
    assert(*(char *)(PGSIZE + 1) == (char)0xFF);

    rp0 = pte2page(*ptep0);
    rp1 = pte2page(*ptep1);
    // assert(!PageSwap(rp0) && PageSwap(rp1) && PageActive(rp1));

    entry = try_alloc_swap_entry();
    assert(!PageSwap(rp0) && !PageSwap(rp1));
    assert(swap_offset(entry) == 1 && mem_map_getvalue(1) == SWAP_UNUSED);
    assert(list_empty(&(active_list.swap_list)));
    assert(list_empty(&(inactive_list.swap_list)));
    page_insert(pgdir, rp0, PGSIZE, (perm | PTE_A));
    sfence_vma();

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
    mem_map_setvalue(2, SWAP_UNUSED);

    ret = swap_copy_entry(entry, &store);
    assert(ret == 0 && swap_offset(store) == 2 && mem_map_getvalue(2) == 0);
    mem_map_setvalue(2, 1);
    *ptep1 = store;

    assert(*(char *)PGSIZE == (char)0xEE);
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

    // assert(page_ref(rp0) == 1 && page_ref(rp1) == 1);
    assert(nr_active_pages == 0 && list_empty(&active_list.swap_list));
    assert(nr_inactive_pages == 0 && list_empty(&inactive_list.swap_list));

    for (i = 0; i < HASH_LIST_SIZE; i++) { assert(list_empty(hash_list + i)); }

    page_remove(pgdir, 0);
    page_remove(pgdir, PGSIZE);

    pte_t pg1 = kernel_pagetable[0];
    pte_t pg2 = ((pte_t *)PTE2PA(pg1))[0];
    ((pte_t *)PTE2PA(pg1))[0] = 0;
    FreePage(pa2page(PTE2PA(pg2)));

    assert(nr_active_pages == 0 && nr_inactive_pages == 0);
    for (offset = 0; offset < max_swap_offset; offset++) { mem_map_setvalue(offset, SWAP_UNUSED); }

    mm_destroy(mm);
    check_mm_struct = nullptr;
    // assert(nr_free_pages_store == nr_free_pages());
    assert(slab_allocated_store == slab_allocated());

    cprintf("check_swap() succeeded.\n");
}

static void check_mm_swap(void) {
    size_t nr_free_pages_store = nr_free_pages();
    size_t slab_allocated_store = slab_allocated();

    int ret, i, j;
    for (i = 0; i < max_swap_offset; i++) { assert(mem_map_getvalue(i) == SWAP_UNUSED); }

    extern Mm_struct *check_mm_struct;
    assert(check_mm_struct == nullptr);

    Mm_struct *mm0 = mm_create(), *mm1;
    assert(mm0 != nullptr && list_empty(&mm0->mmap_list));
    uint32_t addr0, addr1;
    addr0 = 0;
    do {
        ret = mm_map(mm0, addr0, PT1PGSIZE, 0, NULL);
        addr0 += PT1PGSIZE;
    } while (addr0 != 0);
    addr0 = 0;
    for (i = 0; i < 1024; i++, addr0 += PT1PGSIZE) {
        ret = mm_map(mm0, addr0, PGSIZE, 0, NULL);
        assert(ret == -E_INVAL);
    }
    mm_destroy(mm0);

    mm0 = mm_create();
    assert(mm0 != nullptr && list_empty(&mm0->mmap_list));
    addr0 = 0, i = 0;
    do {
        ret = mm_map(mm0, addr0, PT1PGSIZE - PGSIZE, 0, nullptr);
        if (ret == 0) i++;
        addr0 += PT1PGSIZE;
    } while (addr0 != 0);

    addr0 = 0, j = 0;
    do {
        addr0 += PT1PGSIZE - PGSIZE;
        ret = mm_map(mm0, addr0, PGSIZE, 0, nullptr);
        if (ret == 0) j++;
        addr0 += PGSIZE;
    } while (addr0 != 0);

    assert(j + 1 >= i);

    mm_destroy(mm0);

    assert(nr_free_pages_store == nr_free_pages());
    assert(slab_allocated_store == slab_allocated());

    mm0 = mm_create();
    assert(mm0 != nullptr && list_empty(&mm0->mmap_list));

    Page *page = AllocPage();
    assert(page != nullptr);
    pagetable_t *pgdir = (pagetable_t *)page2kva(page);
    memcpy(pgdir, kernel_pagetable, PGSIZE);

    mm0->pagetable = pgdir;
    check_mm_struct = mm0;
    w_satp(MAKE_SATP(mm0->pagetable));
    sfence_vma();

    uint32_t vm_flags = VM_WRITE | VM_READ;
    Vma_struct *vma;

    addr0 = 0;
    do {
        if ((ret = mm_map(mm0, addr0, PT2PGSIZE, vm_flags, &vma)) == 0) { break; }
        addr0 += PT2PGSIZE;
    } while (addr0 != 0);
    assert(ret == 0 && addr0 == 0 && mm0->map_count == 1);
    assert(vma->vm_start == addr0 && vma->vm_end == addr0 + PT2PGSIZE);

    pte_t *ptep;
    for (addr1 = addr0; addr1 < addr0 + PT2PGSIZE; addr1 += PGSIZE) {
        ptep = get_pte(pgdir, addr1, 0);
        assert(ptep == nullptr);
    }

    memset((void *)(uintptr_t)addr0, 0xEF, PGSIZE * 2);
    ptep = get_pte(pgdir, addr0, 0);
    assert(ptep != nullptr && (*ptep & PTE_V));
    ptep = get_pte(pgdir, addr0 + PGSIZE, 0);
    assert(ptep != nullptr && (*ptep & PTE_V));

    ret = mm_unmap(mm0, -PT2PGSIZE, PT2PGSIZE);
    assert(ret == -E_INVAL);
    ret = mm_unmap(mm0, addr0 + PT2PGSIZE, PT2PGSIZE);
    assert(ret == 0);

    addr1 = addr0 + PT2PGSIZE / 2;
    ret = mm_unmap(mm0, addr1, PGSIZE);
    assert(ret == 0 && mm0->map_count == 2);

    ret = mm_unmap(mm0, addr1 + 2 * PGSIZE, PGSIZE * 4);
    assert(ret == 0 && mm0->map_count == 3);

    ret = mm_map(mm0, addr1, PGSIZE * 6, 0, nullptr);
    assert(ret == -E_INVAL);
    ret = mm_map(mm0, addr1, PGSIZE, 0, nullptr);
    assert(ret == 0 && mm0->map_count == 4);
    ret = mm_map(mm0, addr1 + 2 * PGSIZE, PGSIZE * 4, 0, nullptr);
    assert(ret == 0 && mm0->map_count == 5);

    ret = mm_unmap(mm0, addr1 + PGSIZE / 2, PT2PGSIZE / 2 - PGSIZE);
    assert(ret == 0 && mm0->map_count == 1);

    addr1 = addr0 + PGSIZE;

    addr1 = addr0 + PGSIZE;
    for (i = 0; i < PGSIZE; i++) { assert(*(char *)(uintptr_t)(addr1 + i) == (char)0xEF); }

    ret = mm_unmap(mm0, addr1 + PGSIZE / 2, PGSIZE / 4);
    assert(ret == 0 && mm0->map_count == 2);
    ptep = get_pte(pgdir, addr0, 0);
    assert(ptep != nullptr && (*ptep & PTE_V));
    ptep = get_pte(pgdir, addr0 + PGSIZE, 0);
    assert(ptep != nullptr && *ptep == 0);

    ret = mm_map(mm0, addr1, PGSIZE, vm_flags, nullptr);
    memset((void *)(uintptr_t)addr1, 0x88, PGSIZE);
    assert(*(char *)(uintptr_t)addr1 == (char)0x88 && mm0->map_count == 3);

    for (i = 1; i < 16; i += 2) {
        ret = mm_unmap(mm0, addr0 + PGSIZE * i, PGSIZE);
        assert(ret == 0);
        if (i < 8) {
            ret = mm_map(mm0, addr0 + PGSIZE * i, PGSIZE, 0, nullptr);
            assert(ret == 0);
        }
    }
    assert(mm0->map_count == 13);

    ret = mm_unmap(mm0, addr0 + PGSIZE * 2, PT2PGSIZE - PGSIZE * 2);
    assert(ret == 0 && mm0->map_count == 2);

    ret = mm_unmap(mm0, addr0, PGSIZE * 2);
    assert(ret == 0 && mm0->map_count == 0);

    ret = mm_map(mm0, PT1PGSIZE, PT1PGSIZE, vm_flags, nullptr);
    assert(ret == 0);
    for (i = 0, addr1 = PT1PGSIZE; i < 4; i++, addr1 += PGSIZE) { *(char *)(uintptr_t)addr1 = (char)0xFF; }
    assert((mm0->pagetable)[1] != 0);
    pte_t pg = mm0->pagetable[1];
    assert(((pte_t *)PTE2PA(pg))[0] != 0);
    exit_mmap(mm0);
    assert(((pte_t *)PTE2PA(pg))[1] == 0);
    assert((mm0->pagetable)[1] == 0);

    for (i = 0; i < max_swap_offset; i++) { assert(mem_map_getvalue(i) == SWAP_UNUSED); }

    addr0 = PT1PGSIZE;
    ret = mm_map(mm0, addr0, PT1PGSIZE, vm_flags, nullptr);
    assert(ret == 0);

    addr1 = addr0;
    for (i = 0; i < 4; i++, addr1 += PGSIZE) { *(char *)(uintptr_t)addr1 = (char)(i * i); }

    ret = 0;
    ret += swap_out_mm(mm0, 10);
    ret += swap_out_mm(mm0, 10);
    assert(ret == 4);

    for (; i < 8; i++, addr1 += PGSIZE) { *(char *)(uintptr_t)addr1 = (char)(i * i); }

    mm1 = mm_create();
    assert(mm1 != nullptr);

    page = AllocPage();
    assert(page != nullptr);
    pgdir = (pagetable_t *)page2kva(page);
    memcpy(pgdir, kernel_pagetable, PGSIZE);
    mm1->pagetable = pgdir;

    ret = dup_mmap(mm1, mm0);
    assert(ret == 0);

    check_mm_struct = mm1;
    w_satp(MAKE_SATP(mm1->pagetable));
    sfence_vma();
    addr1 = addr0;
    for (i = 0; i < 8; i++, addr1 += PGSIZE) {
        assert(*(char *)(uintptr_t)addr1 == (char)(i * i));
        *(char *)(uintptr_t)addr1 = (char)0x88;
    }

    check_mm_struct = mm0;
    w_satp(MAKE_SATP(mm0->pagetable));
    sfence_vma();

    addr1 = addr0;
    for (i = 0; i < 8; i++, addr1 += PGSIZE) { assert(*(char *)(uintptr_t)addr1 == (char)(i * i)); }

    check_mm_struct = nullptr;
    w_satp(MAKE_SATP(kernel_pagetable));
    sfence_vma();

    exit_mmap(mm0);
    exit_mmap(mm1);

    FreePage(kva2page((uintptr_t)mm0->pagetable));
    mm_destroy(mm0);
    FreePage(kva2page((uintptr_t)mm1->pagetable));
    mm_destroy(mm1);

    refill_inactive_scan();
    page_launder();
    for (i = 0; i < max_swap_offset; i++) { assert(mem_map_getvalue(i) == SWAP_UNUSED); }

    pte_t pg1 = kernel_pagetable[0];
    pte_t pg2 = ((pte_t *)PTE2PA(pg1))[0];
    ((pte_t *)PTE2PA(pg1))[0] = 0;
    FreePage(pa2page(PTE2PA(pg2)));
    assert(nr_free_pages_store == nr_free_pages());
    assert(slab_allocated_store == slab_allocated());

    cprintf("check_mm_swap() succeeded.\n");
}

static void check_mm_shm_swap(void) {
    size_t nr_free_pages_store = nr_free_pages();
    size_t slab_allocated_store = slab_allocated();

    int ret, i;
    for (i = 0; i < max_swap_offset; i++) { assert(mem_map_getvalue(i) == SWAP_UNUSED); }

    extern Mm_struct *check_mm_struct;
    assert(check_mm_struct == nullptr);

    Mm_struct *mm0 = mm_create(), *mm1;
    assert(mm0 != nullptr && list_empty(&mm0->mmap_list));

    Page *page = AllocPage();
    assert(page != nullptr);
    pagetable_t *pgdir = (pagetable_t *)page2kva(page);
    memcpy(pgdir, kernel_pagetable, PGSIZE);

    mm0->pagetable = pgdir;
    check_mm_struct = mm0;
    w_satp(MAKE_SATP(mm0->pagetable));
    sfence_vma();

    uint32_t vm_flags = VM_WRITE | VM_READ;

    uintptr_t addr0, addr1;

    addr0 = PT1PGSIZE;

    ret = mm_map(mm0, addr0, PT2PGSIZE * 4, vm_flags, nullptr);

    assert(ret == 0 && mm0->map_count == 1);

    ret = mm_unmap(mm0, addr0, PT2PGSIZE * 4);
    assert(ret == 0 && mm0->map_count == 0);

    Shmem_struct *shmem = shmem_create(PT2PGSIZE * 2);
    assert(shmem != nullptr && shmem_ref(shmem) == 0);

    Vma_struct *vma;
    addr1 = addr0 + PT2PGSIZE * 2;
    ret = mm_map_shmem(mm0, addr0, vm_flags, shmem, &vma);
    assert(ret == 0);
    assert((vma->vm_flags & VM_SHARE) && vma->shmem == shmem && shmem_ref(shmem) == 1);
    ret = mm_map_shmem(mm0, addr1, vm_flags, shmem, &vma);
    assert(ret == 0);
    assert((vma->vm_flags & VM_SHARE) && vma->shmem == shmem && shmem_ref(shmem) == 2);

    for (i = 0; i < 4; i++) { *(char *)(addr0 + i * PGSIZE) = (char)(i * i); }
    for (i = 0; i < 4; i++) {
        assert(*(char *)(addr1 + i * PGSIZE) == (char)(i * i));
        *(char *)(addr1 + i * PGSIZE) = (char)(-i * i);
    }
    for (i = 0; i < 4; i++) { assert(*(char *)(addr0 + i * PGSIZE) == (char)(-i * i)); }

    ret = swap_out_mm(mm0, 8) + swap_out_mm(mm0, 8);
    assert(ret == 8 && nr_active_pages == 4 && nr_inactive_pages == 0);

    refill_inactive_scan();
    assert(nr_active_pages == 0 && nr_inactive_pages == 4);

    memset((void *)addr0, 0x77, PGSIZE);
    for (i = 0; i < PGSIZE; i++) { assert(*(char *)(addr1 + i) == (char)0x77); }

    ret = mm_unmap(mm0, addr1, PGSIZE * 4);
    assert(ret == 0);

    addr0 += 4 * PGSIZE, addr1 += 4 * PGSIZE;
    *(char *)(addr0) = (char)(0xDC);
    assert(*(char *)(addr1) == (char)(0xDC));
    *(char *)(addr1 + PT2PGSIZE) = (char)(0xDC);
    assert(*(char *)(addr0 + PT2PGSIZE) == (char)(0xDC));

    mm1 = mm_create();
    assert(mm1 != nullptr);

    page = AllocPage();
    assert(page != nullptr);
    pgdir = (pagetable_t *)page2kva(page);
    memcpy(pgdir, kernel_pagetable, PGSIZE);
    mm1->pagetable = pgdir;

    ret = dup_mmap(mm1, mm0);
    assert(ret == 0 && shmem_ref(shmem) == 4);

    check_mm_struct = mm1;
    w_satp(MAKE_SATP(mm1->pagetable));
    sfence_vma();

    for (i = 0; i < 4; i++) { *(char *)(addr0 + i * PGSIZE) = (char)(0x57 + i); }
    for (i = 0; i < 4; i++) { assert(*(char *)(addr1 + i * PGSIZE) == (char)(0x57 + i)); }

    check_mm_struct = mm0;
    w_satp(MAKE_SATP(mm0->pagetable));
    sfence_vma();

    for (i = 0; i < 4; i++) {
        assert(*(char *)(addr0 + i * PGSIZE) == (char)(0x57 + i));
        assert(*(char *)(addr1 + i * PGSIZE) == (char)(0x57 + i));
    }

    exit_mmap(mm1);

    FreePage(kva2page((uintptr_t)mm1->pagetable));
    mm_destroy(mm1);

    assert(shmem_ref(shmem) == 2);

    check_mm_struct = nullptr;
    w_satp(MAKE_SATP(kernel_pagetable));
    sfence_vma();

    exit_mmap(mm0);
    FreePage(kva2page((uintptr_t)mm0->pagetable));
    mm_destroy(mm0);

    refill_inactive_scan();
    page_launder();
    for (i = 0; i < max_swap_offset; i++) { assert(mem_map_getvalue(i) == SWAP_UNUSED); }

    assert(nr_free_pages_store == nr_free_pages());
    assert(slab_allocated_store == slab_allocated());

    cprintf("check_mm_shm_swap() succeeded.\n");
}