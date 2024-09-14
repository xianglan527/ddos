#ifndef __CONFIG_MBOXBUF_H__
#define __CONFIG_MBOXBUF_H__

#include "types.h"

typedef struct mboxbuf Mboxbuf;
struct mboxbuf{
    int from;
    size_t len;
    size_t size;
    void *data;
};

typedef struct mboxinfo Mboxinfo;
struct mboxinfo{
    size_t slots;
    size_t max_slots;
    bool inuse;
    bool has_sender;
    bool has_receiver;
};
#endif
