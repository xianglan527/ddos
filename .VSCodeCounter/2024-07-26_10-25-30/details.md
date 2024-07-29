# Details

Date : 2024-07-26 10:25:30

Directory c:\\Users\\45228\\Desktop\\share\\ddos\\code

Total : 102 files,  8000 codes, 505 comments, 1379 blanks, all 9884 lines

[Summary](results.md) / Details / [Diff Summary](diff.md) / [Diff Details](diff-details.md)

## Files
| filename | language | code | comment | blank | total |
| :--- | :--- | ---: | ---: | ---: | ---: |
| [code/kernel/Makefile](/code/kernel/Makefile) | Makefile | 21 | 0 | 4 | 25 |
| [code/kernel/boot/start.c](/code/kernel/boot/start.c) | C | 40 | 13 | 18 | 71 |
| [code/kernel/config/config.h](/code/kernel/config/config.h) | C | 41 | 1 | 12 | 54 |
| [code/kernel/config/error.h](/code/kernel/config/error.h) | C++ | 14 | 2 | 3 | 19 |
| [code/kernel/config/sysdef.h](/code/kernel/config/sysdef.h) | C++ | 27 | 0 | 1 | 28 |
| [code/kernel/driver/Makefile](/code/kernel/driver/Makefile) | Makefile | 4 | 0 | 4 | 8 |
| [code/kernel/driver/uart.c](/code/kernel/driver/uart.c) | C | 54 | 12 | 15 | 81 |
| [code/kernel/driver/uart.h](/code/kernel/driver/uart.h) | C++ | 7 | 0 | 1 | 8 |
| [code/kernel/driver/virtio/virtio-blk.c](/code/kernel/driver/virtio/virtio-blk.c) | C | 236 | 28 | 48 | 312 |
| [code/kernel/driver/virtio/virtio-blk.h](/code/kernel/driver/virtio/virtio-blk.h) | C++ | 77 | 24 | 11 | 112 |
| [code/kernel/driver/virtio/virtio-mmio.c](/code/kernel/driver/virtio/virtio-mmio.c) | C | 104 | 10 | 26 | 140 |
| [code/kernel/driver/virtio/virtio-mmio.h](/code/kernel/driver/virtio/virtio-mmio.h) | C++ | 48 | 0 | 6 | 54 |
| [code/kernel/driver/virtio/virtio-ring.c](/code/kernel/driver/virtio/virtio-ring.c) | C | 54 | 6 | 10 | 70 |
| [code/kernel/driver/virtio/virtio-ring.h](/code/kernel/driver/virtio/virtio-ring.h) | C++ | 41 | 9 | 9 | 59 |
| [code/kernel/driver/virtio/virtio-rng.c](/code/kernel/driver/virtio/virtio-rng.c) | C | 101 | 14 | 23 | 138 |
| [code/kernel/driver/virtio/virtio-rng.h](/code/kernel/driver/virtio/virtio-rng.h) | C++ | 17 | 0 | 7 | 24 |
| [code/kernel/driver/virtio/virtio.h](/code/kernel/driver/virtio/virtio.h) | C++ | 21 | 2 | 6 | 29 |
| [code/kernel/driver/virtio/virtio_device.c](/code/kernel/driver/virtio/virtio_device.c) | C | 54 | 0 | 5 | 59 |
| [code/kernel/driver/virtio/virtio_device.h](/code/kernel/driver/virtio/virtio_device.h) | C++ | 18 | 0 | 2 | 20 |
| [code/kernel/fs/fs.h](/code/kernel/fs/fs.h) | C++ | 7 | 0 | 2 | 9 |
| [code/kernel/fs/swapfs.c](/code/kernel/fs/swapfs.c) | C | 31 | 0 | 3 | 34 |
| [code/kernel/fs/swapfs.h](/code/kernel/fs/swapfs.h) | C++ | 8 | 0 | 2 | 10 |
| [code/kernel/kernel/console.c](/code/kernel/kernel/console.c) | C | 109 | 2 | 11 | 122 |
| [code/kernel/kernel/console.h](/code/kernel/kernel/console.h) | C++ | 10 | 0 | 1 | 11 |
| [code/kernel/kernel/main.c](/code/kernel/kernel/main.c) | C | 65 | 5 | 7 | 77 |
| [code/kernel/kernel/plic.c](/code/kernel/kernel/plic.c) | C | 26 | 7 | 7 | 40 |
| [code/kernel/kernel/plic.h](/code/kernel/kernel/plic.h) | C++ | 9 | 0 | 1 | 10 |
| [code/kernel/kernel/proc.c](/code/kernel/kernel/proc.c) | C | 622 | 34 | 60 | 716 |
| [code/kernel/kernel/proc.h](/code/kernel/kernel/proc.h) | C++ | 144 | 4 | 18 | 166 |
| [code/kernel/kernel/riscv.h](/code/kernel/kernel/riscv.h) | C | 157 | 33 | 52 | 242 |
| [code/kernel/kernel/spinlock.c](/code/kernel/kernel/spinlock.c) | C | 86 | 0 | 15 | 101 |
| [code/kernel/kernel/spinlock.h](/code/kernel/kernel/spinlock.h) | C | 23 | 2 | 3 | 28 |
| [code/kernel/kernel/syscall.c](/code/kernel/kernel/syscall.c) | C | 80 | 5 | 10 | 95 |
| [code/kernel/kernel/syscall.h](/code/kernel/kernel/syscall.h) | C++ | 13 | 1 | 4 | 18 |
| [code/kernel/kernel/sysfile.c](/code/kernel/kernel/sysfile.c) | C | 72 | 0 | 12 | 84 |
| [code/kernel/kernel/trap.c](/code/kernel/kernel/trap.c) | C | 189 | 5 | 19 | 213 |
| [code/kernel/kernel/trap.h](/code/kernel/kernel/trap.h) | C++ | 17 | 0 | 3 | 20 |
| [code/kernel/libs/atomic.h](/code/kernel/libs/atomic.h) | C++ | 79 | 1 | 42 | 122 |
| [code/kernel/libs/stdio.c](/code/kernel/libs/stdio.c) | C | 49 | 0 | 8 | 57 |
| [code/kernel/libs/stdio.h](/code/kernel/libs/stdio.h) | C | 11 | 0 | 1 | 12 |
| [code/kernel/mm/buddyPmm.c](/code/kernel/mm/buddyPmm.c) | C | 231 | 0 | 28 | 259 |
| [code/kernel/mm/buddyPmm.h](/code/kernel/mm/buddyPmm.h) | C++ | 8 | 0 | 3 | 11 |
| [code/kernel/mm/defaultPmm.c](/code/kernel/mm/defaultPmm.c) | C | 91 | 0 | 20 | 111 |
| [code/kernel/mm/defaultPmm.h](/code/kernel/mm/defaultPmm.h) | C++ | 8 | 0 | 3 | 11 |
| [code/kernel/mm/memlayout.h](/code/kernel/mm/memlayout.h) | C++ | 29 | 34 | 12 | 75 |
| [code/kernel/mm/pmm.c](/code/kernel/mm/pmm.c) | C | 617 | 9 | 77 | 703 |
| [code/kernel/mm/pmm.h](/code/kernel/mm/pmm.h) | C++ | 151 | 5 | 41 | 197 |
| [code/kernel/mm/shmem.c](/code/kernel/mm/shmem.c) | C | 71 | 0 | 8 | 79 |
| [code/kernel/mm/shmem.h](/code/kernel/mm/shmem.h) | C++ | 30 | 0 | 9 | 39 |
| [code/kernel/mm/slab.c](/code/kernel/mm/slab.c) | C | 385 | 0 | 69 | 454 |
| [code/kernel/mm/slab.h](/code/kernel/mm/slab.h) | C++ | 12 | 0 | 4 | 16 |
| [code/kernel/mm/swap.c](/code/kernel/mm/swap.c) | C | 808 | 4 | 181 | 993 |
| [code/kernel/mm/swap.h](/code/kernel/mm/swap.h) | C++ | 22 | 7 | 6 | 35 |
| [code/kernel/mm/vmm.c](/code/kernel/mm/vmm.c) | C | 524 | 4 | 53 | 581 |
| [code/kernel/mm/vmm.h](/code/kernel/mm/vmm.h) | C++ | 67 | 0 | 16 | 83 |
| [code/kernel/test/kerneltest.c](/code/kernel/test/kerneltest.c) | C | 82 | 2 | 9 | 93 |
| [code/kernel/test/kerneltest.h](/code/kernel/test/kerneltest.h) | C++ | 5 | 0 | 0 | 5 |
| [code/kernel/test/uprintf.c](/code/kernel/test/uprintf.c) | C | 160 | 10 | 17 | 187 |
| [code/kernel/test/uprintf.h](/code/kernel/test/uprintf.h) | C++ | 13 | 0 | 3 | 16 |
| [code/kernel/test/user.c](/code/kernel/test/user.c) | C | 9 | 0 | 3 | 12 |
| [code/kernel/test/user.h](/code/kernel/test/user.h) | C | 16 | 0 | 2 | 18 |
| [code/kernel/test/usertest.c](/code/kernel/test/usertest.c) | C | 39 | 49 | 17 | 105 |
| [code/kernel/test/usertest.h](/code/kernel/test/usertest.h) | C++ | 4 | 0 | 0 | 4 |
| [code/kernel/test/usys.py](/code/kernel/test/usys.py) | Python | 19 | 2 | 6 | 27 |
| [code/kernel/util/assert.h](/code/kernel/util/assert.h) | C++ | 16 | 0 | 4 | 20 |
| [code/kernel/util/bitmap.c](/code/kernel/util/bitmap.c) | C | 81 | 1 | 8 | 90 |
| [code/kernel/util/bitmap.h](/code/kernel/util/bitmap.h) | C++ | 19 | 0 | 2 | 21 |
| [code/kernel/util/elf.h](/code/kernel/util/elf.h) | C++ | 36 | 4 | 5 | 45 |
| [code/kernel/util/hash.c](/code/kernel/util/hash.c) | C | 6 | 1 | 3 | 10 |
| [code/kernel/util/hash.h](/code/kernel/util/hash.h) | C++ | 5 | 0 | 1 | 6 |
| [code/kernel/util/list.h](/code/kernel/util/list.h) | C++ | 58 | 0 | 17 | 75 |
| [code/kernel/util/panic.c](/code/kernel/util/panic.c) | C | 53 | 0 | 6 | 59 |
| [code/kernel/util/panic.h](/code/kernel/util/panic.h) | C++ | 10 | 0 | 3 | 13 |
| [code/kernel/util/printf.c](/code/kernel/util/printf.c) | C | 168 | 3 | 16 | 187 |
| [code/kernel/util/printf.h](/code/kernel/util/printf.h) | C | 15 | 0 | 6 | 21 |
| [code/kernel/util/rand.c](/code/kernel/util/rand.c) | C | 18 | 6 | 7 | 31 |
| [code/kernel/util/rand.h](/code/kernel/util/rand.h) | C++ | 9 | 0 | 2 | 11 |
| [code/kernel/util/rbtree.c](/code/kernel/util/rbtree.c) | C | 344 | 4 | 52 | 400 |
| [code/kernel/util/rbtree.h](/code/kernel/util/rbtree.h) | C++ | 26 | 0 | 6 | 32 |
| [code/kernel/util/stdarg.h](/code/kernel/util/stdarg.h) | C++ | 7 | 1 | 4 | 12 |
| [code/kernel/util/string.c](/code/kernel/util/string.c) | C | 128 | 0 | 14 | 142 |
| [code/kernel/util/string.h](/code/kernel/util/string.h) | C | 18 | 0 | 2 | 20 |
| [code/kernel/util/types.h](/code/kernel/util/types.h) | C++ | 45 | 0 | 15 | 60 |
| [code/user/Makefile](/code/user/Makefile) | Makefile | 6 | 0 | 3 | 9 |
| [code/user/libs/assert.h](/code/user/libs/assert.h) | C++ | 16 | 0 | 4 | 20 |
| [code/user/libs/atomic.h](/code/user/libs/atomic.h) | C++ | 79 | 114 | 25 | 218 |
| [code/user/libs/main.c](/code/user/libs/main.c) | C | 10 | 0 | 1 | 11 |
| [code/user/libs/panic.c](/code/user/libs/panic.c) | C | 55 | 0 | 13 | 68 |
| [code/user/libs/panic.h](/code/user/libs/panic.h) | C++ | 9 | 0 | 3 | 12 |
| [code/user/libs/printf.c](/code/user/libs/printf.c) | C | 168 | 10 | 18 | 196 |
| [code/user/libs/printf.h](/code/user/libs/printf.h) | C | 14 | 0 | 2 | 16 |
| [code/user/libs/rand.c](/code/user/libs/rand.c) | C | 7 | 5 | 5 | 17 |
| [code/user/libs/rand.h](/code/user/libs/rand.h) | C++ | 7 | 0 | 3 | 10 |
| [code/user/libs/stdarg.h](/code/user/libs/stdarg.h) | C++ | 7 | 1 | 4 | 12 |
| [code/user/libs/string.c](/code/user/libs/string.c) | C | 128 | 0 | 14 | 142 |
| [code/user/libs/string.h](/code/user/libs/string.h) | C | 18 | 0 | 2 | 20 |
| [code/user/libs/types.h](/code/user/libs/types.h) | C++ | 27 | 0 | 9 | 36 |
| [code/user/libs/user.c](/code/user/libs/user.c) | C | 9 | 0 | 3 | 12 |
| [code/user/libs/user.h](/code/user/libs/user.h) | C | 15 | 0 | 2 | 17 |
| [code/user/libs/usys.py](/code/user/libs/usys.py) | Python | 19 | 2 | 6 | 27 |
| [code/user/test/usertests.c](/code/user/test/usertests.c) | C | 153 | 7 | 9 | 169 |
| [code/user/test/usertests.h](/code/user/test/usertests.h) | C++ | 4 | 0 | 1 | 5 |

[Summary](results.md) / Details / [Diff Summary](diff.md) / [Diff Details](diff-details.md)