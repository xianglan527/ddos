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
    int i;

    if (argc < 2) {
        lsdir(".");
        exit(0);
    }
    for (i = 1; i < argc; i++) { lsdir(argv[i]); }
    exit(0);
}
