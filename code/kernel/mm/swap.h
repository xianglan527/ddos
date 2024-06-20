#ifndef __MM_SWAP_H__
#define __MM_SWAP_H__
#include "types.h"
/* *
 * swap_entry_t
 * --------------------------------------------
 * |         offset        |   reserved   | 0 |
 * --------------------------------------------
 *           44 bits            9 bits    1 bit
 * */

#define MAX_SWAP_OFFSET_LIMIT (1UL << 44)

extern size_t max_swap_offset;
typedef struct page Page;

#define swap_offset(entry)                                   \
    ({                                                       \
        size_t __offset = (entry >> 10);                      \
        if (!(__offset > 0 && __offset < max_swap_offset)) { \
            panic("invalid swap_entry_t = %016x.\n", entry); \
        }                                                    \
        __offset;                                            \
    })

void swap_init(void);
bool try_free_pages(size_t n);

void swap_remove_entry(swap_entry_t entry);
size_t swap_page_count(Page *page);
void swap_duplicate(swap_entry_t entry);
int swap_in_page(swap_entry_t entry, Page **pagep);
int swap_copy_entry(swap_entry_t entry, swap_entry_t *store);

#endif