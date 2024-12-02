#include "assert.h"
#include "dir.h"
#include "error.h"
#include "file.h"
#include "lock.h"
#include "malloc.h"
#include "panic.h"
#include "printf.h"
#include "rand.h"
#include "spipe.h"
#include "string.h"
#include "sysdef.h"
#include "thread.h"
#include "user.h"

int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        write(1, argv[i], strlen(argv[i]));
        if (i + 1 < argc) {
            write(1, " ", 1);
        } else {
            write(1, "\n", 1);
        }
    }
    exit(0);
}