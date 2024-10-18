#ifndef __FS_IOBUF_H__
#define __FS_IOBUF_H__
#include "types.h"

typedef struct iobuf Iobuf;
struct iobuf{
    void *io_base;
    off_t io_offset;
    size_t io_len;
    size_t io_resid;
};

#define iobuf_used(iob) ((size_t)((iob)->io_len - (iob)->io_resid))

Iobuf *iobuf_init(Iobuf *iob, void *base, size_t len, off_t offset);
void iobuf_skip(Iobuf *iob, size_t n);
long iobuf_move(Iobuf *iob, void *data, size_t len, bool m2b, size_t *copiedp);
long iobuf_move_zeros(Iobuf *iob, size_t len, size_t *copiedp);
#endif