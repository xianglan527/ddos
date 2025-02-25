#include "assert.h"
#include "dir.h"
#include "malloc.h"
#include "panic.h"
#include "printf.h"
#include "string.h"
#include "sysdef.h"
#include "user.h"
#include "socket.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(2, "Usage: ping address count data_size interval...\n");
        exit(1);
    }
    char *dest = nullptr;
    size_t count = 4;       // Default count
    size_t size = 56;       // Default data size
    size_t interval = 500;  // Default interval
    Ping ping;
    memset(&ping, 0, sizeof(ping));
    dest = argv[1];
    if(argc > 2){
        count = atoi(argv[2]);
    }
    if (argc > 3) { size = atoi(argv[3]); }
    if (argc > 4) { interval = atoi(argv[4]); }
    ping_run(&ping, dest, count, size, interval);
    exit(0);
}
