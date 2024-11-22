# Details

Date : 2024-11-12 11:16:33

Directory c:\\Users\\45228\\Desktop\\share\\ddos\\code

Total : 175 files,  34100 codes, 1190 comments, 3009 blanks, all 38299 lines

[Summary](results.md) / Details / [Diff Summary](diff.md) / [Diff Details](diff-details.md)

## Files
| filename | language | code | comment | blank | total |
| :--- | :--- | ---: | ---: | ---: | ---: |
| [code/kernel/Makefile](/code/kernel/Makefile) | Makefile | 26 | 0 | 4 | 30 |
| [code/kernel/boot/entry.S](/code/kernel/boot/entry.S) | Assembler file | 13 | 0 | 3 | 16 |
| [code/kernel/boot/start.c](/code/kernel/boot/start.c) | C | 40 | 13 | 18 | 71 |
| [code/kernel/config/config.h](/code/kernel/config/config.h) | C | 62 | 7 | 25 | 94 |
| [code/kernel/config/error.h](/code/kernel/config/error.h) | C | 28 | 2 | 2 | 32 |
| [code/kernel/config/fs_img.h](/code/kernel/config/fs_img.h) | C | 47 | 9 | 14 | 70 |
| [code/kernel/config/mboxbuf.h](/code/kernel/config/mboxbuf.h) | C | 19 | 0 | 4 | 23 |
| [code/kernel/config/sigaction.h](/code/kernel/config/sigaction.h) | C | 39 | 0 | 8 | 47 |
| [code/kernel/config/stat.h](/code/kernel/config/stat.h) | C++ | 22 | 0 | 5 | 27 |
| [code/kernel/config/sysdef.h](/code/kernel/config/sysdef.h) | C++ | 79 | 8 | 8 | 95 |
| [code/kernel/driver/Makefile](/code/kernel/driver/Makefile) | Makefile | 4 | 0 | 4 | 8 |
| [code/kernel/driver/uart.c](/code/kernel/driver/uart.c) | C | 54 | 12 | 15 | 81 |
| [code/kernel/driver/uart.h](/code/kernel/driver/uart.h) | C++ | 7 | 0 | 1 | 8 |
| [code/kernel/driver/virtio/virtio-blk.c](/code/kernel/driver/virtio/virtio-blk.c) | C | 255 | 44 | 46 | 345 |
| [code/kernel/driver/virtio/virtio-blk.h](/code/kernel/driver/virtio/virtio-blk.h) | C | 80 | 26 | 12 | 118 |
| [code/kernel/driver/virtio/virtio-mmio.c](/code/kernel/driver/virtio/virtio-mmio.c) | C | 104 | 10 | 26 | 140 |
| [code/kernel/driver/virtio/virtio-mmio.h](/code/kernel/driver/virtio/virtio-mmio.h) | C++ | 48 | 0 | 6 | 54 |
| [code/kernel/driver/virtio/virtio-ring.c](/code/kernel/driver/virtio/virtio-ring.c) | C | 54 | 6 | 10 | 70 |
| [code/kernel/driver/virtio/virtio-ring.h](/code/kernel/driver/virtio/virtio-ring.h) | C++ | 41 | 9 | 9 | 59 |
| [code/kernel/driver/virtio/virtio-rng.c](/code/kernel/driver/virtio/virtio-rng.c) | C | 101 | 14 | 23 | 138 |
| [code/kernel/driver/virtio/virtio-rng.h](/code/kernel/driver/virtio/virtio-rng.h) | C++ | 17 | 0 | 7 | 24 |
| [code/kernel/driver/virtio/virtio.h](/code/kernel/driver/virtio/virtio.h) | C++ | 21 | 2 | 6 | 29 |
| [code/kernel/driver/virtio/virtio_device.c](/code/kernel/driver/virtio/virtio_device.c) | C | 54 | 0 | 5 | 59 |
| [code/kernel/driver/virtio/virtio_device.h](/code/kernel/driver/virtio/virtio_device.h) | C++ | 18 | 0 | 2 | 20 |
| [code/kernel/fs/Makefile](/code/kernel/fs/Makefile) | Makefile | 10 | 0 | 4 | 14 |
| [code/kernel/fs/devs/dev.c](/code/kernel/fs/devs/dev.c) | C | 102 | 12 | 14 | 128 |
| [code/kernel/fs/devs/dev.h](/code/kernel/fs/devs/dev.h) | C++ | 21 | 0 | 5 | 26 |
| [code/kernel/fs/devs/dev_disk0.c](/code/kernel/fs/devs/dev_disk0.c) | C | 100 | 0 | 14 | 114 |
| [code/kernel/fs/devs/dev_null.c](/code/kernel/fs/devs/dev_null.c) | C | 29 | 0 | 6 | 35 |
| [code/kernel/fs/devs/dev_stdin.c](/code/kernel/fs/devs/dev_stdin.c) | C | 92 | 0 | 13 | 105 |
| [code/kernel/fs/devs/dev_stdout.c](/code/kernel/fs/devs/dev_stdout.c) | C | 48 | 0 | 8 | 56 |
| [code/kernel/fs/file.c](/code/kernel/fs/file.c) | C | 337 | 3 | 24 | 364 |
| [code/kernel/fs/file.h](/code/kernel/fs/file.h) | C | 49 | 1 | 6 | 56 |
| [code/kernel/fs/fs.c](/code/kernel/fs/fs.c) | C | 70 | 1 | 9 | 80 |
| [code/kernel/fs/fs.h](/code/kernel/fs/fs.h) | C | 34 | 0 | 9 | 43 |
| [code/kernel/fs/iobuf.c](/code/kernel/fs/iobuf.c) | C | 39 | 0 | 5 | 44 |
| [code/kernel/fs/iobuf.h](/code/kernel/fs/iobuf.h) | C++ | 16 | 0 | 3 | 19 |
| [code/kernel/fs/pipe/pipe.c](/code/kernel/fs/pipe/pipe.c) | C | 52 | 1 | 8 | 61 |
| [code/kernel/fs/pipe/pipe.h](/code/kernel/fs/pipe/pipe.h) | C | 35 | 0 | 9 | 44 |
| [code/kernel/fs/pipe/pipe_inode.c](/code/kernel/fs/pipe/pipe_inode.c) | C | 179 | 0 | 12 | 191 |
| [code/kernel/fs/pipe/pipe_root.c](/code/kernel/fs/pipe/pipe_root.c) | C | 138 | 0 | 6 | 144 |
| [code/kernel/fs/pipe/pipe_state.c](/code/kernel/fs/pipe/pipe_state.c) | C | 123 | 0 | 15 | 138 |
| [code/kernel/fs/pipe/pipe_state.h](/code/kernel/fs/pipe/pipe_state.h) | C++ | 29 | 0 | 4 | 33 |
| [code/kernel/fs/sfs/buf_io.c](/code/kernel/fs/sfs/buf_io.c) | C | 79 | 0 | 11 | 90 |
| [code/kernel/fs/sfs/sfs.c](/code/kernel/fs/sfs/sfs.c) | C | 140 | 4 | 18 | 162 |
| [code/kernel/fs/sfs/sfs.h](/code/kernel/fs/sfs/sfs.h) | C | 65 | 0 | 15 | 80 |
| [code/kernel/fs/sfs/sfs_inode.c](/code/kernel/fs/sfs/sfs_inode.c) | C | 703 | 9 | 50 | 762 |
| [code/kernel/fs/swapfs.c](/code/kernel/fs/swapfs.c) | C | 47 | 0 | 5 | 52 |
| [code/kernel/fs/swapfs.h](/code/kernel/fs/swapfs.h) | C++ | 10 | 0 | 1 | 11 |
| [code/kernel/fs/vfs/inode.c](/code/kernel/fs/vfs/inode.c) | C | 72 | 0 | 15 | 87 |
| [code/kernel/fs/vfs/inode.h](/code/kernel/fs/vfs/inode.h) | C | 127 | 122 | 26 | 275 |
| [code/kernel/fs/vfs/vfs.c](/code/kernel/fs/vfs/vfs.c) | C | 75 | 0 | 10 | 85 |
| [code/kernel/fs/vfs/vfs.h](/code/kernel/fs/vfs/vfs.h) | C | 67 | 83 | 17 | 167 |
| [code/kernel/fs/vfs/vfsdev.c](/code/kernel/fs/vfs/vfsdev.c) | C | 216 | 19 | 18 | 253 |
| [code/kernel/fs/vfs/vfsfile.c](/code/kernel/fs/vfs/vfsfile.c) | C | 125 | 0 | 9 | 134 |
| [code/kernel/fs/vfs/vfslookup.c](/code/kernel/fs/vfs/vfslookup.c) | C | 68 | 9 | 3 | 80 |
| [code/kernel/fs/vfs/vfspath.c](/code/kernel/fs/vfs/vfspath.c) | C | 100 | 0 | 9 | 109 |
| [code/kernel/kernel/console.c](/code/kernel/kernel/console.c) | C | 98 | 2 | 11 | 111 |
| [code/kernel/kernel/console.h](/code/kernel/kernel/console.h) | C++ | 10 | 0 | 1 | 11 |
| [code/kernel/kernel/kernelvec.S](/code/kernel/kernel/kernelvec.S) | Assembler file | 107 | 0 | 11 | 118 |
| [code/kernel/kernel/main.c](/code/kernel/kernel/main.c) | C | 73 | 9 | 7 | 89 |
| [code/kernel/kernel/plic.c](/code/kernel/kernel/plic.c) | C | 26 | 7 | 7 | 40 |
| [code/kernel/kernel/plic.h](/code/kernel/kernel/plic.h) | C++ | 9 | 0 | 1 | 10 |
| [code/kernel/kernel/proc.c](/code/kernel/kernel/proc.c) | C | 1,019 | 154 | 91 | 1,264 |
| [code/kernel/kernel/proc.h](/code/kernel/kernel/proc.h) | C | 187 | 3 | 18 | 208 |
| [code/kernel/kernel/riscv.h](/code/kernel/kernel/riscv.h) | C | 200 | 48 | 57 | 305 |
| [code/kernel/kernel/spinlock.c](/code/kernel/kernel/spinlock.c) | C | 64 | 0 | 7 | 71 |
| [code/kernel/kernel/spinlock.h](/code/kernel/kernel/spinlock.h) | C++ | 26 | 2 | 2 | 30 |
| [code/kernel/kernel/swtch.S](/code/kernel/kernel/swtch.S) | Assembler file | 54 | 0 | 12 | 66 |
| [code/kernel/kernel/syscall.c](/code/kernel/kernel/syscall.c) | C | 180 | 5 | 11 | 196 |
| [code/kernel/kernel/syscall.h](/code/kernel/kernel/syscall.h) | C++ | 13 | 1 | 4 | 18 |
| [code/kernel/kernel/sysfile.c](/code/kernel/kernel/sysfile.c) | C | 479 | 11 | 56 | 546 |
| [code/kernel/kernel/trampoline.S](/code/kernel/kernel/trampoline.S) | Assembler file | 127 | 0 | 16 | 143 |
| [code/kernel/kernel/trap.c](/code/kernel/kernel/trap.c) | C | 215 | 22 | 20 | 257 |
| [code/kernel/kernel/trap.h](/code/kernel/kernel/trap.h) | C++ | 17 | 0 | 3 | 20 |
| [code/kernel/libs/atomic.h](/code/kernel/libs/atomic.h) | C | 79 | 1 | 42 | 122 |
| [code/kernel/libs/stdio.c](/code/kernel/libs/stdio.c) | C | 50 | 1 | 8 | 59 |
| [code/kernel/libs/stdio.h](/code/kernel/libs/stdio.h) | C | 11 | 0 | 1 | 12 |
| [code/kernel/mm/buddyPmm.c](/code/kernel/mm/buddyPmm.c) | C | 226 | 1 | 29 | 256 |
| [code/kernel/mm/buddyPmm.h](/code/kernel/mm/buddyPmm.h) | C++ | 8 | 0 | 3 | 11 |
| [code/kernel/mm/defaultPmm.c](/code/kernel/mm/defaultPmm.c) | C | 91 | 0 | 20 | 111 |
| [code/kernel/mm/defaultPmm.h](/code/kernel/mm/defaultPmm.h) | C++ | 8 | 0 | 3 | 11 |
| [code/kernel/mm/memlayout.h](/code/kernel/mm/memlayout.h) | C | 29 | 35 | 13 | 77 |
| [code/kernel/mm/pmm.c](/code/kernel/mm/pmm.c) | C | 664 | 9 | 78 | 751 |
| [code/kernel/mm/pmm.h](/code/kernel/mm/pmm.h) | C++ | 155 | 5 | 41 | 201 |
| [code/kernel/mm/shmem.c](/code/kernel/mm/shmem.c) | C | 71 | 0 | 8 | 79 |
| [code/kernel/mm/shmem.h](/code/kernel/mm/shmem.h) | C++ | 30 | 0 | 9 | 39 |
| [code/kernel/mm/slab.c](/code/kernel/mm/slab.c) | C | 385 | 1 | 69 | 455 |
| [code/kernel/mm/slab.h](/code/kernel/mm/slab.h) | C | 12 | 0 | 4 | 16 |
| [code/kernel/mm/swap.c](/code/kernel/mm/swap.c) | C | 894 | 20 | 185 | 1,099 |
| [code/kernel/mm/swap.h](/code/kernel/mm/swap.h) | C++ | 27 | 7 | 5 | 39 |
| [code/kernel/mm/vmm.c](/code/kernel/mm/vmm.c) | C | 606 | 10 | 57 | 673 |
| [code/kernel/mm/vmm.h](/code/kernel/mm/vmm.h) | C++ | 74 | 7 | 19 | 100 |
| [code/kernel/schedule/sched.c](/code/kernel/schedule/sched.c) | C | 131 | 7 | 13 | 151 |
| [code/kernel/schedule/sched.h](/code/kernel/schedule/sched.h) | C | 50 | 1 | 5 | 56 |
| [code/kernel/schedule/sched_CFS.c](/code/kernel/schedule/sched_CFS.c) | C | 150 | 17 | 13 | 180 |
| [code/kernel/schedule/sched_CFS.h](/code/kernel/schedule/sched_CFS.h) | C | 7 | 0 | 1 | 8 |
| [code/kernel/schedule/sched_FCFS.c](/code/kernel/schedule/sched_FCFS.c) | C | 41 | 5 | 7 | 53 |
| [code/kernel/schedule/sched_FCFS.h](/code/kernel/schedule/sched_FCFS.h) | C++ | 7 | 0 | 1 | 8 |
| [code/kernel/schedule/sched_RR.c](/code/kernel/schedule/sched_RR.c) | C | 91 | 3 | 8 | 102 |
| [code/kernel/schedule/sched_RR.h](/code/kernel/schedule/sched_RR.h) | C++ | 7 | 0 | 1 | 8 |
| [code/kernel/syn/event.c](/code/kernel/syn/event.c) | C | 106 | 17 | 13 | 136 |
| [code/kernel/syn/event.h](/code/kernel/syn/event.h) | C++ | 16 | 0 | 2 | 18 |
| [code/kernel/syn/mbox.c](/code/kernel/syn/mbox.c) | C | 360 | 4 | 24 | 388 |
| [code/kernel/syn/mbox.h](/code/kernel/syn/mbox.h) | C | 50 | 0 | 9 | 59 |
| [code/kernel/syn/sem.c](/code/kernel/syn/sem.c) | C | 252 | 0 | 28 | 280 |
| [code/kernel/syn/sem.h](/code/kernel/syn/sem.h) | C | 43 | 0 | 6 | 49 |
| [code/kernel/syn/signal.c](/code/kernel/syn/signal.c) | C | 111 | 12 | 7 | 130 |
| [code/kernel/syn/signal.h](/code/kernel/syn/signal.h) | C++ | 35 | 1 | 6 | 42 |
| [code/kernel/syn/syn.c](/code/kernel/syn/syn.c) | C | 5 | 0 | 2 | 7 |
| [code/kernel/syn/syn.h](/code/kernel/syn/syn.h) | C++ | 6 | 0 | 3 | 9 |
| [code/kernel/syn/wait.c](/code/kernel/syn/wait.c) | C | 92 | 1 | 15 | 108 |
| [code/kernel/syn/wait.h](/code/kernel/syn/wait.h) | C | 32 | 0 | 6 | 38 |
| [code/kernel/test/kerneltest.c](/code/kernel/test/kerneltest.c) | C | 87 | 7 | 9 | 103 |
| [code/kernel/test/kerneltest.h](/code/kernel/test/kerneltest.h) | C++ | 5 | 0 | 0 | 5 |
| [code/kernel/test/uprintf.c](/code/kernel/test/uprintf.c) | C | 159 | 10 | 17 | 186 |
| [code/kernel/test/uprintf.h](/code/kernel/test/uprintf.h) | C++ | 13 | 0 | 3 | 16 |
| [code/kernel/test/user.c](/code/kernel/test/user.c) | C | 9 | 0 | 2 | 11 |
| [code/kernel/test/user.h](/code/kernel/test/user.h) | C | 20 | 0 | 1 | 21 |
| [code/kernel/test/usertest.c](/code/kernel/test/usertest.c) | C | 26 | 47 | 15 | 88 |
| [code/kernel/test/usertest.h](/code/kernel/test/usertest.h) | C++ | 4 | 0 | 0 | 4 |
| [code/kernel/test/usys.S](/code/kernel/test/usys.S) | Assembler file | 128 | 0 | 1 | 129 |
| [code/kernel/test/usys.py](/code/kernel/test/usys.py) | Python | 19 | 2 | 6 | 27 |
| [code/kernel/util/assert.h](/code/kernel/util/assert.h) | C | 16 | 0 | 4 | 20 |
| [code/kernel/util/bitmap.c](/code/kernel/util/bitmap.c) | C | 81 | 1 | 8 | 90 |
| [code/kernel/util/bitmap.h](/code/kernel/util/bitmap.h) | C | 19 | 0 | 2 | 21 |
| [code/kernel/util/elf.h](/code/kernel/util/elf.h) | C++ | 36 | 4 | 5 | 45 |
| [code/kernel/util/hash.c](/code/kernel/util/hash.c) | C | 6 | 1 | 3 | 10 |
| [code/kernel/util/hash.h](/code/kernel/util/hash.h) | C | 5 | 0 | 1 | 6 |
| [code/kernel/util/list.h](/code/kernel/util/list.h) | C | 67 | 0 | 19 | 86 |
| [code/kernel/util/panic.c](/code/kernel/util/panic.c) | C | 50 | 3 | 6 | 59 |
| [code/kernel/util/panic.h](/code/kernel/util/panic.h) | C++ | 10 | 0 | 3 | 13 |
| [code/kernel/util/printf.c](/code/kernel/util/printf.c) | C | 183 | 3 | 16 | 202 |
| [code/kernel/util/printf.h](/code/kernel/util/printf.h) | C++ | 15 | 0 | 5 | 20 |
| [code/kernel/util/rand.c](/code/kernel/util/rand.c) | C | 18 | 6 | 7 | 31 |
| [code/kernel/util/rand.h](/code/kernel/util/rand.h) | C++ | 9 | 0 | 2 | 11 |
| [code/kernel/util/rbtree.c](/code/kernel/util/rbtree.c) | C | 344 | 4 | 52 | 400 |
| [code/kernel/util/rbtree.h](/code/kernel/util/rbtree.h) | C++ | 26 | 0 | 6 | 32 |
| [code/kernel/util/stdarg.h](/code/kernel/util/stdarg.h) | C++ | 7 | 1 | 4 | 12 |
| [code/kernel/util/string.c](/code/kernel/util/string.c) | C | 149 | 0 | 17 | 166 |
| [code/kernel/util/string.h](/code/kernel/util/string.h) | C | 21 | 0 | 2 | 23 |
| [code/kernel/util/types.h](/code/kernel/util/types.h) | C | 47 | 0 | 17 | 64 |
| [code/user/Makefile](/code/user/Makefile) | Makefile | 6 | 0 | 3 | 9 |
| [code/user/libs/assert.h](/code/user/libs/assert.h) | C | 16 | 0 | 4 | 20 |
| [code/user/libs/atomic.h](/code/user/libs/atomic.h) | C | 79 | 114 | 25 | 218 |
| [code/user/libs/clone.S](/code/user/libs/clone.S) | Assembler file | 30 | 0 | 10 | 40 |
| [code/user/libs/dir.c](/code/user/libs/dir.c) | C | 34 | 0 | 3 | 37 |
| [code/user/libs/dir.h](/code/user/libs/dir.h) | C | 13 | 0 | 2 | 15 |
| [code/user/libs/file.h](/code/user/libs/file.h) | C | 22 | 0 | 3 | 25 |
| [code/user/libs/initcode.S](/code/user/libs/initcode.S) | Assembler file | 6 | 0 | 1 | 7 |
| [code/user/libs/lock.h](/code/user/libs/lock.h) | C++ | 31 | 0 | 9 | 40 |
| [code/user/libs/main.c](/code/user/libs/main.c) | C | 33 | 0 | 3 | 36 |
| [code/user/libs/malloc.c](/code/user/libs/malloc.c) | C | 127 | 0 | 18 | 145 |
| [code/user/libs/malloc.h](/code/user/libs/malloc.h) | C++ | 8 | 0 | 3 | 11 |
| [code/user/libs/panic.c](/code/user/libs/panic.c) | C | 55 | 0 | 13 | 68 |
| [code/user/libs/panic.h](/code/user/libs/panic.h) | C++ | 9 | 0 | 3 | 12 |
| [code/user/libs/printf.c](/code/user/libs/printf.c) | C | 177 | 10 | 19 | 206 |
| [code/user/libs/printf.h](/code/user/libs/printf.h) | C++ | 15 | 0 | 2 | 17 |
| [code/user/libs/rand.c](/code/user/libs/rand.c) | C | 7 | 5 | 5 | 17 |
| [code/user/libs/rand.h](/code/user/libs/rand.h) | C++ | 7 | 0 | 3 | 10 |
| [code/user/libs/spipe.c](/code/user/libs/spipe.c) | C | 98 | 0 | 9 | 107 |
| [code/user/libs/spipe.h](/code/user/libs/spipe.h) | C++ | 25 | 0 | 5 | 30 |
| [code/user/libs/stdarg.h](/code/user/libs/stdarg.h) | C++ | 7 | 1 | 4 | 12 |
| [code/user/libs/string.c](/code/user/libs/string.c) | C | 129 | 0 | 15 | 144 |
| [code/user/libs/string.h](/code/user/libs/string.h) | C | 19 | 0 | 2 | 21 |
| [code/user/libs/thread.c](/code/user/libs/thread.c) | C | 37 | 0 | 3 | 40 |
| [code/user/libs/thread.h](/code/user/libs/thread.h) | C++ | 15 | 0 | 6 | 21 |
| [code/user/libs/types.h](/code/user/libs/types.h) | C | 29 | 0 | 11 | 40 |
| [code/user/libs/user.c](/code/user/libs/user.c) | C | 258 | 1 | 68 | 327 |
| [code/user/libs/user.h](/code/user/libs/user.h) | C | 70 | 0 | 1 | 71 |
| [code/user/libs/usys.S](/code/user/libs/usys.S) | Assembler file | 288 | 0 | 1 | 289 |
| [code/user/libs/usys.py](/code/user/libs/usys.py) | Python | 24 | 2 | 6 | 32 |
| [code/user/output/user.asm](/code/user/output/user.asm) | Assembler file | 16,815 | 0 | 611 | 17,426 |
| [code/user/test/usertests.c](/code/user/test/usertests.c) | C | 1,482 | 113 | 161 | 1,756 |
| [code/user/test/usertests.h](/code/user/test/usertests.h) | C++ | 4 | 0 | 1 | 5 |

[Summary](results.md) / Details / [Diff Summary](diff.md) / [Diff Details](diff-details.md)