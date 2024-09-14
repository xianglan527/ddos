#include "spipe.h"
#include "sysdef.h"
#include "assert.h"

#define SPIPE_SIZE  4096
#define SPIPE_BUFSIZE (SPIPE_SIZE - sizeof(Spipe_state))

static int spipe_close_nolock(Spipe *p){
    if(!p->isclosed){
        p->isclosed = p->state->isclosed = true;
        munmap(p->addr, SPIPE_BUFSIZE);
        return 0;
    }
    return -1;
}

static bool spipe_isclosed_nolock(Spipe *p, bool read){
    if(!p->isclosed){
        if(!p->state->isclosed){
            return 0;
        }
        if(p->state->rpos < p->state->wpos){
            return !read;
        }
        spipe_close_nolock(p);
    }
    return 1;
}

void spipe(Spipe *p){
    assert(SPIPE_SIZE > SPIPE_BUFSIZE);
    sem_t spipe_sem;
    uintptr_t addr = 0;
    assert(shmem(&addr, SPIPE_SIZE, MMAP_WRITE) == 0);
    assert((spipe_sem = sem_init(1)) > 0);
    p->isclosed = false;
    p->spipe_sem = spipe_sem;
    p->addr = addr;
    p->state = (Spipe_state *)addr;
    p->buf = (uint8_t *)(p->state + 1);
    p->state->rpos = p->state->wpos = 0;
    p->state->isclosed = 0;
}


int spipe_close(Spipe *p){
    if(!p->isclosed){
        int ret;
        sem_wait(p->spipe_sem);
        ret = spipe_close_nolock(p);
        sem_post(p->spipe_sem);
        return ret;
    }
    return -1;
}

bool spipe_isclosed(Spipe *p){
    if(!p->isclosed){
        bool isclosed;
        sem_wait(p->spipe_sem);
        isclosed = spipe_isclosed_nolock(p, 0);
        sem_post(p->spipe_sem);
        return isclosed;
    }
    return 1;
}

size_t spipe_read(Spipe *p, void *buf, size_t n){
    size_t ret = 0;
try_again:
    assert(p->isclosed == 0 && sem_wait(p->spipe_sem) == 0);
    if(spipe_isclosed_nolock(p, 1)){
        goto out_unlock;
    }
    if (p->state->rpos == p->state->wpos) {
        sem_post(p->spipe_sem);
        yield();
        goto try_again;
    }
    for (; ret < n; ret++, p->state->rpos++) {
        if (p->state->rpos == p->state->wpos) break;
        *(uint8_t *)(buf + ret) = p->buf[p->state->rpos % SPIPE_BUFSIZE];
    }
out_unlock:
    sem_post(p->spipe_sem);
    return ret;
}

size_t spipe_write(Spipe *p, void *buf, size_t n) {
    size_t ret = 0;
try_again:
    assert(p->isclosed == 0 && sem_wait(p->spipe_sem) == 0);
      if(spipe_isclosed_nolock(p, 1)){
        goto out_unlock;
    }
    for (; ret < n; ret++, p->state->wpos++) {
        if (p->state->wpos - p->state->rpos >= SPIPE_BUFSIZE) {
            sem_post(p->spipe_sem);
            yield();
            goto try_again;
        }
        p->buf[p->state->wpos % SPIPE_BUFSIZE] = *(uint8_t *)(buf + ret);
    }
out_unlock:
    sem_post(p->spipe_sem);
    return ret;
}