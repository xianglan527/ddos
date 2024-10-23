#ifndef __FS_PIPE_PIPE_STATE_H__
#define __FS_PIPE_PIPE_STATE_H__

#include "types.h"
#include "config.h"
#include "sem.h"
#include "wait.h"
#include "spinlock.h"

typedef struct pipe_state Pipe_state;
struct pipe_state
{
    off_t p_rpos;
    off_t p_wpos;
    uint8_t *buf;
    bool isclosed;
    int ref_count;
    Sem sem;
    Spinlock pipe_state_lock;
    Wait_queue reader_queue;
    Wait_queue writer_queue;
};

#define PIPE_BUFSIZE    (PIPE_STATE_TOTAL_SIZE - sizeof(Pipe_state))

Pipe_state *pipe_state_create(void);
void pipe_state_acquire(Pipe_state *state);
void pipe_state_release(Pipe_state *state);
void pipe_state_close(Pipe_state *state);
size_t pipe_state_size(Pipe_state *state, bool write);
size_t pipe_state_read(Pipe_state *state, void *buf, size_t n);
size_t pipe_state_write(Pipe_state *state, void *buf, size_t n);
#endif