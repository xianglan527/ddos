# Details

Date : 2024-06-14 17:13:47

Directory c:\\Users\\45228\\Desktop\\share\\ddos\\code

Total : 70 files,  4804 codes, 435 comments, 878 blanks, all 6117 lines

[Summary](results.md) / Details / [Diff Summary](diff.md) / [Diff Details](diff-details.md)

## Files
| filename | language | code | comment | blank | total |
| :--- | :--- | ---: | ---: | ---: | ---: |
| [code/kernel/Makefile](/code/kernel/Makefile) | Makefile | 19 | 0 | 4 | 23 |
| [code/kernel/boot/start.c](/code/kernel/boot/start.c) | C | 40 | 13 | 18 | 71 |
| [code/kernel/config/config.h](/code/kernel/config/config.h) | C | 32 | 1 | 9 | 42 |
| [code/kernel/config/error.h](/code/kernel/config/error.h) | C | 10 | 2 | 4 | 16 |
| [code/kernel/driver/Makefile](/code/kernel/driver/Makefile) | Makefile | 4 | 0 | 4 | 8 |
| [code/kernel/driver/uart.c](/code/kernel/driver/uart.c) | C | 50 | 12 | 14 | 76 |
| [code/kernel/driver/uart.h](/code/kernel/driver/uart.h) | C | 7 | 0 | 1 | 8 |
| [code/kernel/driver/virtio/virtio-blk.c](/code/kernel/driver/virtio/virtio-blk.c) | C | 186 | 20 | 34 | 240 |
| [code/kernel/driver/virtio/virtio-blk.h](/code/kernel/driver/virtio/virtio-blk.h) | C++ | 72 | 13 | 11 | 96 |
| [code/kernel/driver/virtio/virtio-mmio.c](/code/kernel/driver/virtio/virtio-mmio.c) | C | 104 | 10 | 26 | 140 |
| [code/kernel/driver/virtio/virtio-mmio.h](/code/kernel/driver/virtio/virtio-mmio.h) | C++ | 48 | 0 | 6 | 54 |
| [code/kernel/driver/virtio/virtio-ring.c](/code/kernel/driver/virtio/virtio-ring.c) | C | 52 | 5 | 10 | 67 |
| [code/kernel/driver/virtio/virtio-ring.h](/code/kernel/driver/virtio/virtio-ring.h) | C++ | 41 | 9 | 9 | 59 |
| [code/kernel/driver/virtio/virtio-rng.c](/code/kernel/driver/virtio/virtio-rng.c) | C | 101 | 14 | 23 | 138 |
| [code/kernel/driver/virtio/virtio-rng.h](/code/kernel/driver/virtio/virtio-rng.h) | C++ | 17 | 0 | 7 | 24 |
| [code/kernel/driver/virtio/virtio.h](/code/kernel/driver/virtio/virtio.h) | C++ | 22 | 2 | 6 | 30 |
| [code/kernel/driver/virtio/virtio_device.c](/code/kernel/driver/virtio/virtio_device.c) | C | 102 | 0 | 14 | 116 |
| [code/kernel/driver/virtio/virtio_device.h](/code/kernel/driver/virtio/virtio_device.h) | C++ | 17 | 0 | 3 | 20 |
| [code/kernel/kernel/console.c](/code/kernel/kernel/console.c) | C | 107 | 2 | 11 | 120 |
| [code/kernel/kernel/console.h](/code/kernel/kernel/console.h) | C | 10 | 0 | 1 | 11 |
| [code/kernel/kernel/main.c](/code/kernel/kernel/main.c) | C | 63 | 5 | 7 | 75 |
| [code/kernel/kernel/plic.c](/code/kernel/kernel/plic.c) | C | 26 | 7 | 7 | 40 |
| [code/kernel/kernel/plic.h](/code/kernel/kernel/plic.h) | C++ | 9 | 0 | 1 | 10 |
| [code/kernel/kernel/proc.c](/code/kernel/kernel/proc.c) | C | 144 | 1 | 21 | 166 |
| [code/kernel/kernel/proc.h](/code/kernel/kernel/proc.h) | C | 106 | 2 | 9 | 117 |
| [code/kernel/kernel/riscv.h](/code/kernel/kernel/riscv.h) | C | 151 | 33 | 52 | 236 |
| [code/kernel/kernel/spinlock.c](/code/kernel/kernel/spinlock.c) | C | 48 | 0 | 6 | 54 |
| [code/kernel/kernel/spinlock.h](/code/kernel/kernel/spinlock.h) | C | 18 | 0 | 1 | 19 |
| [code/kernel/kernel/syscall.c](/code/kernel/kernel/syscall.c) | C | 45 | 5 | 7 | 57 |
| [code/kernel/kernel/syscall.h](/code/kernel/kernel/syscall.h) | C++ | 31 | 1 | 1 | 33 |
| [code/kernel/kernel/sysfile.c](/code/kernel/kernel/sysfile.c) | C | 17 | 0 | 5 | 22 |
| [code/kernel/kernel/trap.c](/code/kernel/kernel/trap.c) | C | 151 | 20 | 21 | 192 |
| [code/kernel/kernel/trap.h](/code/kernel/kernel/trap.h) | C++ | 17 | 0 | 3 | 20 |
| [code/kernel/libs/atomic.h](/code/kernel/libs/atomic.h) | C | 79 | 114 | 26 | 219 |
| [code/kernel/libs/stdio.c](/code/kernel/libs/stdio.c) | C | 47 | 0 | 8 | 55 |
| [code/kernel/libs/stdio.h](/code/kernel/libs/stdio.h) | C | 11 | 0 | 1 | 12 |
| [code/kernel/mm/buddyPmm.c](/code/kernel/mm/buddyPmm.c) | C | 231 | 0 | 28 | 259 |
| [code/kernel/mm/buddyPmm.h](/code/kernel/mm/buddyPmm.h) | C++ | 8 | 0 | 3 | 11 |
| [code/kernel/mm/defaultPmm.c](/code/kernel/mm/defaultPmm.c) | C | 91 | 0 | 20 | 111 |
| [code/kernel/mm/defaultPmm.h](/code/kernel/mm/defaultPmm.h) | C++ | 8 | 0 | 3 | 11 |
| [code/kernel/mm/memlayout.h](/code/kernel/mm/memlayout.h) | C | 30 | 36 | 12 | 78 |
| [code/kernel/mm/pmm.c](/code/kernel/mm/pmm.c) | C | 433 | 9 | 64 | 506 |
| [code/kernel/mm/pmm.h](/code/kernel/mm/pmm.h) | C | 128 | 5 | 36 | 169 |
| [code/kernel/mm/slab.c](/code/kernel/mm/slab.c) | C | 376 | 0 | 68 | 444 |
| [code/kernel/mm/slab.h](/code/kernel/mm/slab.h) | C++ | 12 | 0 | 4 | 16 |
| [code/kernel/mm/vmm.c](/code/kernel/mm/vmm.c) | C | 237 | 4 | 34 | 275 |
| [code/kernel/mm/vmm.h](/code/kernel/mm/vmm.h) | C++ | 38 | 0 | 11 | 49 |
| [code/kernel/test/sysdef.h](/code/kernel/test/sysdef.h) | C++ | 26 | 0 | 0 | 26 |
| [code/kernel/test/uprintf.c](/code/kernel/test/uprintf.c) | C | 178 | 3 | 20 | 201 |
| [code/kernel/test/uprintf.h](/code/kernel/test/uprintf.h) | C | 6 | 0 | 5 | 11 |
| [code/kernel/test/user.h](/code/kernel/test/user.h) | C++ | 6 | 0 | 1 | 7 |
| [code/kernel/test/usertest.c](/code/kernel/test/usertest.c) | C | 43 | 67 | 14 | 124 |
| [code/kernel/test/usertest.h](/code/kernel/test/usertest.h) | C++ | 4 | 0 | 0 | 4 |
| [code/kernel/test/usys.py](/code/kernel/test/usys.py) | Python | 19 | 2 | 6 | 27 |
| [code/kernel/util/assert.h](/code/kernel/util/assert.h) | C | 16 | 0 | 4 | 20 |
| [code/kernel/util/elf.h](/code/kernel/util/elf.h) | C++ | 36 | 4 | 5 | 45 |
| [code/kernel/util/list.h](/code/kernel/util/list.h) | C++ | 58 | 0 | 17 | 75 |
| [code/kernel/util/panic.c](/code/kernel/util/panic.c) | C | 38 | 0 | 6 | 44 |
| [code/kernel/util/panic.h](/code/kernel/util/panic.h) | C++ | 10 | 0 | 3 | 13 |
| [code/kernel/util/printf.c](/code/kernel/util/printf.c) | C | 164 | 3 | 14 | 181 |
| [code/kernel/util/printf.h](/code/kernel/util/printf.h) | C++ | 15 | 0 | 6 | 21 |
| [code/kernel/util/rand.c](/code/kernel/util/rand.c) | C | 18 | 6 | 7 | 31 |
| [code/kernel/util/rand.h](/code/kernel/util/rand.h) | C++ | 9 | 0 | 2 | 11 |
| [code/kernel/util/rbtree.c](/code/kernel/util/rbtree.c) | C | 344 | 4 | 52 | 400 |
| [code/kernel/util/rbtree.h](/code/kernel/util/rbtree.h) | C++ | 26 | 0 | 6 | 32 |
| [code/kernel/util/stdarg.h](/code/kernel/util/stdarg.h) | C++ | 7 | 1 | 4 | 12 |
| [code/kernel/util/string.c](/code/kernel/util/string.c) | C | 120 | 0 | 13 | 133 |
| [code/kernel/util/string.h](/code/kernel/util/string.h) | C | 17 | 0 | 2 | 19 |
| [code/kernel/util/types.h](/code/kernel/util/types.h) | C | 42 | 0 | 14 | 56 |
| [code/user/Makefile](/code/user/Makefile) | Makefile | 6 | 0 | 3 | 9 |

[Summary](results.md) / Details / [Diff Summary](diff.md) / [Diff Details](diff-details.md)