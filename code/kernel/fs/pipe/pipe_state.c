#include "pipe_state.h"

#include "assert.h"
#include "proc.h"
#include "slab.h"

Pipe_state *pipe_state_create(void) {
    assert((int)PIPE_BUFSIZE > 128);
    Pipe_state *state;
    if ((state = kmalloc(sizeof(Pipe_state) + PIPE_BUFSIZE)) != nullptr) {
        state->p_rpos = state->p_wpos = 0;
        state->buf = (uint8_t *)(state + 1);
        state->isclosed = false;
        state->ref_count = 1;
        sem_init(&state->sem, 1);
        initlock(&state->pipe_state_lock, "pipe_state_lock");
        wait_queue_init(&state->reader_queue);
        wait_queue_init(&state->writer_queue);
    }
    return state;
}

static void lock_state(Pipe_state *state) { down(&state->sem); }

static void unlock_state(Pipe_state *state) { up(&state->sem); }

static inline bool is_emtpty(Pipe_state *state) { return state->p_rpos == state->p_wpos; }

static inline bool is_full(Pipe_state *state) { return state->p_wpos - state->p_rpos >= PIPE_BUFSIZE; }

static bool pipe_state_wait(Wait_queue *queue, Pipe_state *state) {
    acquire(&state->pipe_state_lock);
    Wait __wait, *wait = &__wait;
    wait_current_set(queue, wait, WT_KBD);
    sleeping(myproc(), &state->pipe_state_lock);
    wait_current_del(queue, wait);
    release(&state->pipe_state_lock);
    return wait->wakeup_flags == WT_PIPE;
}

static void pipe_state_wakeup(Wait_queue *queue, Pipe_state *state) {
    if (!wait_queue_empty(queue)) {
        acquire(&state->pipe_state_lock);
        wakeup_queue(queue, WT_PIPE, 1);
        release(&state->pipe_state_lock);
    }
}

#define wait_reader(state)  pipe_state_wait(&state->writer_queue, state)
#define wait_writer(state) pipe_state_wait(&state->reader_queue, state)
#define wakeup_reader(state) pipe_state_wakeup(&state->reader_queue, state)
#define wakeup_writer(state) pipe_state_wakeup(&state->writer_queue, state)

void pipe_state_acquire(Pipe_state *state){
    assert(state->ref_count > 0);
    state->ref_count++;
}

void pipe_state_release(Pipe_state *state){
    assert(state != nullptr && state->ref_count > 0);
    if(--state->ref_count == 0){
        assert(wait_queue_empty(&state->reader_queue));
        assert(wait_queue_empty(&state->writer_queue));
        kfree(state);
    }
}

void pipe_state_close(Pipe_state *state){
    assert(state != nullptr && state->ref_count > 0);
    state->isclosed = true;
    wakeup_reader(state);
    wakeup_writer(state);
}

size_t pipe_state_size(Pipe_state *state, bool write){
    size_t size = state->p_wpos - state->p_rpos;
    if(write){
        if(state->isclosed){
            return 0;
        }
        return PIPE_BUFSIZE - size;
    }
    return size;
}

size_t pipe_state_read(Pipe_state *state, void *buf, size_t n){
    size_t ret = 0;
try_again:
    lock_state(state);
    if(is_emtpty(state)){
        if(state->isclosed){
            goto out_unlock;
        }else{
            unlock_state(state);
            if(!wait_writer(state)){
                goto out;
            }
            goto try_again;
        }
    }
    for(; ret < n && !is_emtpty(state); ret++, state->p_rpos++){
        *(uint8_t *)(buf + ret) = state->buf[state->p_rpos % PIPE_BUFSIZE];
    }
    if(ret != 0){
        wakeup_writer(state);
    }
out_unlock:
    unlock_state(state);
out:
    return ret;
}

size_t pipe_state_write(Pipe_state *state, void *buf, size_t n){
    size_t ret = 0, step;
try_again:
    lock_state(state);
    if(state->isclosed){
        goto out_unlock;
    }
    for(step = 0; ret < n; ret++, step++, state->p_wpos++){
        if(is_full(state)){
            wakeup_reader(state);
            unlock_state(state);
            if(!wait_reader(state)){
                goto out;
            }
            goto try_again;
        }
        state->buf[state->p_wpos % PIPE_BUFSIZE] = *(uint8_t *)(buf + ret);
    }
    if(step != 0){
        wakeup_reader(state);
    }
out_unlock:
    unlock_state(state);
out:
    return ret;
}