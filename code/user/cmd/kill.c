#include "assert.h"
#include "dir.h"
#include "malloc.h"
#include "panic.h"
#include "printf.h"
#include "string.h"
#include "sysdef.h"
#include "user.h"

int main(int argc, char **argv) {
    int i;

    if (argc < 2) {
        fprintf(2, "usage: kill pid...\n");
        exit(1);
    }
    for (i = 1; i < argc; i++) kill(atoi(argv[i]));
    exit(0);
}
