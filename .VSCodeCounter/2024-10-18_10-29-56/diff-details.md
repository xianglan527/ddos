# Diff Details

Date : 2024-10-18 10:29:56

Directory c:\\Users\\45228\\Desktop\\share\\ddos\\code

Total : 47 files,  1875 codes, 279 comments, 241 blanks, all 2395 lines

[Summary](results.md) / [Details](details.md) / [Diff Summary](diff.md) / Diff Details

## Files
| filename | language | code | comment | blank | total |
| :--- | :--- | ---: | ---: | ---: | ---: |
| [code/kernel/Makefile](/code/kernel/Makefile) | Makefile | 1 | 0 | 0 | 1 |
| [code/kernel/config/config.h](/code/kernel/config/config.h) | C | 5 | 2 | 4 | 11 |
| [code/kernel/config/error.h](/code/kernel/config/error.h) | C | 12 | 0 | -1 | 11 |
| [code/kernel/config/stat.h](/code/kernel/config/stat.h) | C++ | 22 | 0 | 5 | 27 |
| [code/kernel/config/sysdef.h](/code/kernel/config/sysdef.h) | C++ | 13 | 8 | 4 | 25 |
| [code/kernel/fs/Makefile](/code/kernel/fs/Makefile) | Makefile | 6 | 0 | 4 | 10 |
| [code/kernel/fs/devs/dev.c](/code/kernel/fs/devs/dev.c) | C | 101 | 12 | 14 | 127 |
| [code/kernel/fs/devs/dev.h](/code/kernel/fs/devs/dev.h) | C++ | 21 | 0 | 5 | 26 |
| [code/kernel/fs/devs/dev_null.c](/code/kernel/fs/devs/dev_null.c) | C | 29 | 0 | 6 | 35 |
| [code/kernel/fs/devs/dev_stdin.c](/code/kernel/fs/devs/dev_stdin.c) | C | 92 | 0 | 13 | 105 |
| [code/kernel/fs/devs/dev_stdout.c](/code/kernel/fs/devs/dev_stdout.c) | C | 48 | 0 | 8 | 56 |
| [code/kernel/fs/file.c](/code/kernel/fs/file.c) | C | 221 | 3 | 19 | 243 |
| [code/kernel/fs/file.h](/code/kernel/fs/file.h) | C | 43 | 1 | 6 | 50 |
| [code/kernel/fs/fs.c](/code/kernel/fs/fs.c) | C | 66 | 1 | 8 | 75 |
| [code/kernel/fs/fs.h](/code/kernel/fs/fs.h) | C | 27 | 0 | 7 | 34 |
| [code/kernel/fs/iobuf.c](/code/kernel/fs/iobuf.c) | C | 39 | 0 | 5 | 44 |
| [code/kernel/fs/iobuf.h](/code/kernel/fs/iobuf.h) | C++ | 16 | 0 | 3 | 19 |
| [code/kernel/fs/vfs/inode.c](/code/kernel/fs/vfs/inode.c) | C | 56 | 0 | 15 | 71 |
| [code/kernel/fs/vfs/inode.h](/code/kernel/fs/vfs/inode.h) | C | 119 | 123 | 26 | 268 |
| [code/kernel/fs/vfs/vfs.c](/code/kernel/fs/vfs/vfs.c) | C | 67 | 0 | 9 | 76 |
| [code/kernel/fs/vfs/vfs.h](/code/kernel/fs/vfs/vfs.h) | C++ | 47 | 83 | 11 | 141 |
| [code/kernel/fs/vfs/vfsdev.c](/code/kernel/fs/vfs/vfsdev.c) | C | 216 | 19 | 18 | 253 |
| [code/kernel/fs/vfs/vfsfile.c](/code/kernel/fs/vfs/vfsfile.c) | C | 124 | 0 | 9 | 133 |
| [code/kernel/fs/vfs/vfslookup.c](/code/kernel/fs/vfs/vfslookup.c) | C | 66 | 9 | 3 | 78 |
| [code/kernel/fs/vfs/vfspath.c](/code/kernel/fs/vfs/vfspath.c) | C | 82 | 0 | 8 | 90 |
| [code/kernel/kernel/console.c](/code/kernel/kernel/console.c) | C | -11 | 0 | 0 | -11 |
| [code/kernel/kernel/main.c](/code/kernel/kernel/main.c) | C | 2 | 0 | 0 | 2 |
| [code/kernel/kernel/proc.c](/code/kernel/kernel/proc.c) | C | 39 | 0 | 2 | 41 |
| [code/kernel/kernel/proc.h](/code/kernel/kernel/proc.h) | C | 3 | 0 | 0 | 3 |
| [code/kernel/kernel/syscall.c](/code/kernel/kernel/syscall.c) | C | 10 | 0 | 0 | 10 |
| [code/kernel/kernel/sysfile.c](/code/kernel/kernel/sysfile.c) | C | 95 | 9 | 7 | 111 |
| [code/kernel/kernel/trap.c](/code/kernel/kernel/trap.c) | C | 2 | 0 | 0 | 2 |
| [code/kernel/libs/stdio.c](/code/kernel/libs/stdio.c) | C | 1 | 1 | 0 | 2 |
| [code/kernel/test/usertest.c](/code/kernel/test/usertest.c) | C | -1 | 1 | 0 | 0 |
| [code/kernel/util/printf.c](/code/kernel/util/printf.c) | C | 13 | 0 | 0 | 13 |
| [code/kernel/util/printf.h](/code/kernel/util/printf.h) | C++ | 0 | 0 | -1 | -1 |
| [code/kernel/util/string.c](/code/kernel/util/string.c) | C | 11 | 0 | 2 | 13 |
| [code/kernel/util/string.h](/code/kernel/util/string.h) | C | 2 | 0 | 0 | 2 |
| [code/user/libs/file.h](/code/user/libs/file.h) | C | 22 | 0 | 3 | 25 |
| [code/user/libs/main.c](/code/user/libs/main.c) | C | 23 | 0 | 2 | 25 |
| [code/user/libs/printf.c](/code/user/libs/printf.c) | C | 10 | 0 | 1 | 11 |
| [code/user/libs/printf.h](/code/user/libs/printf.h) | C++ | 1 | 0 | 0 | 1 |
| [code/user/libs/string.c](/code/user/libs/string.c) | C | 1 | 0 | 1 | 2 |
| [code/user/libs/string.h](/code/user/libs/string.h) | C | 1 | 0 | 0 | 1 |
| [code/user/libs/user.c](/code/user/libs/user.c) | C | 24 | 0 | 8 | 32 |
| [code/user/libs/user.h](/code/user/libs/user.h) | C | 7 | 0 | 1 | 8 |
| [code/user/test/usertests.c](/code/user/test/usertests.c) | C | 81 | 7 | 6 | 94 |

[Summary](results.md) / [Details](details.md) / [Diff Summary](diff.md) / Diff Details