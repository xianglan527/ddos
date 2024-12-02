#include "assert.h"
#include "dir.h"
#include "malloc.h"
#include "panic.h"
#include "printf.h"
#include "string.h"
#include "sysdef.h"
#include "user.h"

int main(int argc, char *argv[]) {
    int i;
    int ret;
    if (argc < 2) {
        fprintf(2, "Usage: mkdir files...\n");
        exit(1);
    }

    for (i = 1; i < argc; i++) {
        if ((ret = mkdir(argv[i])) < 0) {
            fprintf(2, "mkdir: %s failed to create ret is %d\n", argv[i], ret);
            break;
        }
    }

    exit(0);
}
