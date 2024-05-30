# Details

Date : 2024-05-30 09:45:48

Directory c:\\Users\\45228\\Desktop\\share\\ddos\\code

Total : 49 files,  10559 codes, 330 comments, 870 blanks, all 11759 lines

[Summary](results.md) / Details / [Diff Summary](diff.md) / [Diff Details](diff-details.md)

## Files
| filename | language | code | comment | blank | total |
| :--- | :--- | ---: | ---: | ---: | ---: |
| [code/kernel/Makefile](/code/kernel/Makefile) | Makefile | 18 | 0 | 4 | 22 |
| [code/kernel/boot/start.c](/code/kernel/boot/start.c) | C | 40 | 13 | 18 | 71 |
| [code/kernel/config/config.h](/code/kernel/config/config.h) | C | 21 | 1 | 7 | 29 |
| [code/kernel/config/error.h](/code/kernel/config/error.h) | C | 10 | 2 | 4 | 16 |
| [code/kernel/driver/uart.c](/code/kernel/driver/uart.c) | C | 50 | 12 | 14 | 76 |
| [code/kernel/driver/uart.h](/code/kernel/driver/uart.h) | C | 7 | 0 | 1 | 8 |
| [code/kernel/kernel/console.c](/code/kernel/kernel/console.c) | C | 106 | 2 | 10 | 118 |
| [code/kernel/kernel/console.h](/code/kernel/kernel/console.h) | C | 10 | 0 | 1 | 11 |
| [code/kernel/kernel/main.c](/code/kernel/kernel/main.c) | C | 51 | 8 | 6 | 65 |
| [code/kernel/kernel/plic.c](/code/kernel/kernel/plic.c) | C | 21 | 5 | 5 | 31 |
| [code/kernel/kernel/plic.h](/code/kernel/kernel/plic.h) | C++ | 9 | 0 | 1 | 10 |
| [code/kernel/kernel/proc.c](/code/kernel/kernel/proc.c) | C | 118 | 0 | 23 | 141 |
| [code/kernel/kernel/proc.h](/code/kernel/kernel/proc.h) | C | 100 | 1 | 6 | 107 |
| [code/kernel/kernel/riscv.h](/code/kernel/kernel/riscv.h) | C | 151 | 33 | 53 | 237 |
| [code/kernel/kernel/spinlock.c](/code/kernel/kernel/spinlock.c) | C | 48 | 0 | 6 | 54 |
| [code/kernel/kernel/spinlock.h](/code/kernel/kernel/spinlock.h) | C | 18 | 0 | 2 | 20 |
| [code/kernel/kernel/syscall.c](/code/kernel/kernel/syscall.c) | C | 45 | 5 | 7 | 57 |
| [code/kernel/kernel/syscall.h](/code/kernel/kernel/syscall.h) | C++ | 31 | 1 | 1 | 33 |
| [code/kernel/kernel/sysfile.c](/code/kernel/kernel/sysfile.c) | C | 18 | 0 | 5 | 23 |
| [code/kernel/kernel/trap.c](/code/kernel/kernel/trap.c) | C | 135 | 21 | 19 | 175 |
| [code/kernel/kernel/trap.h](/code/kernel/kernel/trap.h) | C++ | 17 | 0 | 3 | 20 |
| [code/kernel/libs/atomic.h](/code/kernel/libs/atomic.h) | C | 79 | 114 | 26 | 219 |
| [code/kernel/libs/stdio.c](/code/kernel/libs/stdio.c) | C | 46 | 0 | 8 | 54 |
| [code/kernel/libs/stdio.h](/code/kernel/libs/stdio.h) | C | 11 | 0 | 1 | 12 |
| [code/kernel/mm/defaultPmm.c](/code/kernel/mm/defaultPmm.c) | C | 91 | 0 | 20 | 111 |
| [code/kernel/mm/defaultPmm.h](/code/kernel/mm/defaultPmm.h) | C++ | 8 | 0 | 3 | 11 |
| [code/kernel/mm/memlayout.h](/code/kernel/mm/memlayout.h) | C++ | 28 | 36 | 12 | 76 |
| [code/kernel/mm/pmm.c](/code/kernel/mm/pmm.c) | C | 39 | 1 | 13 | 53 |
| [code/kernel/mm/pmm.h](/code/kernel/mm/pmm.h) | C | 71 | 2 | 19 | 92 |
| [code/kernel/output/kernel.asm](/code/kernel/output/kernel.asm) | Assembler file | 8,351 | 0 | 435 | 8,786 |
| [code/kernel/test/sysdef.h](/code/kernel/test/sysdef.h) | C++ | 26 | 0 | 0 | 26 |
| [code/kernel/test/uprintf.c](/code/kernel/test/uprintf.c) | C | 178 | 3 | 20 | 201 |
| [code/kernel/test/uprintf.h](/code/kernel/test/uprintf.h) | C | 6 | 0 | 5 | 11 |
| [code/kernel/test/user.h](/code/kernel/test/user.h) | C++ | 6 | 0 | 1 | 7 |
| [code/kernel/test/usertest.c](/code/kernel/test/usertest.c) | C | 50 | 60 | 15 | 125 |
| [code/kernel/test/usertest.h](/code/kernel/test/usertest.h) | C++ | 4 | 0 | 0 | 4 |
| [code/kernel/test/usys.py](/code/kernel/test/usys.py) | Python | 19 | 2 | 6 | 27 |
| [code/kernel/util/assert.h](/code/kernel/util/assert.h) | C | 16 | 0 | 4 | 20 |
| [code/kernel/util/elf.h](/code/kernel/util/elf.h) | C++ | 36 | 4 | 5 | 45 |
| [code/kernel/util/list.h](/code/kernel/util/list.h) | C++ | 58 | 0 | 17 | 75 |
| [code/kernel/util/panic.c](/code/kernel/util/panic.c) | C | 38 | 0 | 6 | 44 |
| [code/kernel/util/panic.h](/code/kernel/util/panic.h) | C++ | 10 | 0 | 3 | 13 |
| [code/kernel/util/printf.c](/code/kernel/util/printf.c) | C | 163 | 3 | 14 | 180 |
| [code/kernel/util/printf.h](/code/kernel/util/printf.h) | C++ | 15 | 0 | 6 | 21 |
| [code/kernel/util/stdarg.h](/code/kernel/util/stdarg.h) | C++ | 7 | 1 | 4 | 12 |
| [code/kernel/util/string.c](/code/kernel/util/string.c) | C | 115 | 0 | 12 | 127 |
| [code/kernel/util/string.h](/code/kernel/util/string.h) | C | 16 | 0 | 2 | 18 |
| [code/kernel/util/types.h](/code/kernel/util/types.h) | C | 42 | 0 | 14 | 56 |
| [code/user/Makefile](/code/user/Makefile) | Makefile | 6 | 0 | 3 | 9 |

[Summary](results.md) / Details / [Diff Summary](diff.md) / [Diff Details](diff-details.md)