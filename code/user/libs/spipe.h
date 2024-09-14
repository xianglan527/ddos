#ifndef __LIBS_SPIPE_H__
#define __LIBS_SPIPE_H__

#include "stdarg.h"
#include "types.h"
#include "user.h"

typedef struct spipe_state Spipe_state;
struct spipe_state{
    off_t rpos;
    off_t wpos;
    bool isclosed;
};

typedef struct spipe Spipe;
struct spipe{
    volatile bool isclosed;
    sem_t spipe_sem;
    uintptr_t addr;
    Spipe_state *state;
    uint8_t *buf;
};

void spipe(Spipe *p);
bool spipe_isclosed(Spipe *p);
int spipe_close(Spipe *p);
size_t spipe_read(Spipe *p, void *buf, size_t n);
size_t spipe_write(Spipe *p, void *buf, size_t n);
#endif
