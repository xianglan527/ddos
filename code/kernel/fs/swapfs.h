#ifndef __FS_SWAPFS_H__
#define __FS_SWAPFS_H__
#include "types.h"
#include "pmm.h"

void swapfs_init(void);
void swapfs_read(swap_entry_t entry, Page *page);
void swapfs_write(swap_entry_t entry, Page *page);
void swapfs_read_syn(swap_entry_t entry, Page *page);
void swapfs_write_syn(swap_entry_t entry, Page *page);
#endif