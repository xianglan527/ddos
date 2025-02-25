#include "mbox.h"

#include "assert.h"
#include "error.h"
#include "pmm.h"
#include "proc.h"
#include "slab.h"
#include "spinlock.h"
#include "string.h"

extern Spinlock timer_lock;

static Msg_mbox *mbox_map[MAX_MBOX_PAGES];
static List_entry free_mbox_list;
static Spinlock mboxs_lock;

void mbox_init(void) {
    int i;
    for (i = 0; i < MAX_MBOX_PAGES; i++) { mbox_map[i] = nullptr; }
    initlock(&mboxs_lock, "mboxs_lock");
    list_init(&free_mbox_list);
    assert(MBOX_P_PAGE != 0);
}

Msg_mbox *get_mbox(int id) {
    if (id >= 0 && id < MAX_MBOX_NUM) {
        int i = id / MBOX_P_PAGE, j = id % MBOX_P_PAGE;
        if (mbox_map[i] != nullptr) {
            Msg_mbox *mbox = mbox_map[i] + j;
            if (mbox->state == OPENED) { return mbox; }
        }
    }
    return nullptr;
}

static void mbox_free(Msg_mbox *mbox) {
    assert(mbox->state == CLOSING && mbox->inuse == 0);
    assert(list_empty(&mbox->msg_link));
    assert(wait_queue_empty(&mbox->senders));
    assert(wait_queue_empty(&mbox->receivers));
    // assert(mbox->msg_mbox_lock.locked == false);
    mbox->state = CLOSED;
    mbox->max_slots = mbox->slots = 0;
    acquire(&mboxs_lock);
    list_add_before(&free_mbox_list, &mbox->msg_link);
    release(&mboxs_lock);
}

static void add_msg(Msg_mbox *mbox, Msg_msg *msg, bool append) {
    assert(mbox->state == OPENED);
    mbox->slots++;
    List_entry *list = &mbox->msg_link, *le = &msg->msg_link;
    if (append) {
        list_add_before(list, le);
    } else {
        list_add_after(list, le);
    }
    wakeup_first(&mbox->receivers, WT_MBOX_RECV, 1);
}

static int pick_msg(Msg_mbox *mbox, size_t max_bytes, Msg_msg **msg_store) {
    assert(mbox->state == OPENED && mbox->slots > 0);
    assert(!list_empty(&mbox->msg_link));
    Msg_msg *msg = le2msg(list_next(&mbox->msg_link), msg_link);
    if (max_bytes < msg->bytes) { return -E_TOO_BIG; }
    mbox->slots--;
    *msg_store = msg;
    list_del(&msg->msg_link);
    wakeup_first(&(mbox->senders), WT_MBOX_SEND, 1);
    return 0;
}

static Msg_mbox *new_mbox(size_t max_slots) {
    Msg_mbox *mbox = nullptr;
    acquire(&mboxs_lock);
    if (list_empty(&free_mbox_list)) {
        int i, id;
        for (i = 0; i < MAX_MBOX_PAGES; i++) {
            if (mbox_map[i] == nullptr) { break; }
        }
        if (i == MAX_MBOX_PAGES) { goto out; }
        Page *page = AllocPage();
        if (page == nullptr) { goto out; }
        id = i * MBOX_P_PAGE;
        mbox = mbox_map[i] = (Msg_mbox *)page2kva(page);
        for (i = 0; i < MBOX_P_PAGE; i++, id++, mbox++) {
            mbox->id = id, mbox->inuse = 0;
            mbox->state = CLOSED;
            mbox->max_slots = mbox->slots = 0;
            list_init(&mbox->msg_link);
            wait_queue_init(&mbox->senders);
            wait_queue_init(&mbox->receivers);
            initlock(&mbox->msg_mbox_lock, "msg_mbox_lock");
            list_add_before(&free_mbox_list, &mbox->msg_link);
        }
    }
    assert(!list_empty(&free_mbox_list));
    mbox = le2mbox(list_next(&free_mbox_list), msg_link);
    list_del_init(&mbox->msg_link);
    mbox->state = OPENED;
    mbox->max_slots = max_slots;
out:
    release(&mboxs_lock);
    return mbox;
}

int ipc_mbox_init(size_t max_slots) {
    if (max_slots == 0 || max_slots > MAX_MSG_SLOTS) { return -E_INVAL; }
    int ret = -E_NO_MEM;
    Msg_mbox *mbox;
    if ((mbox = new_mbox(max_slots)) != nullptr) { ret = mbox->id; }
    return ret;
}

static void free_seg(Msg_seg *seg) {
    if (seg->next != nullptr) { free_seg(seg->next); }
    kfree(seg);
}

static void free_msg(Msg_msg *msg) {
    if (msg->next != nullptr) { free_seg(msg->next); }
    kfree(msg);
}

static Msg_msg *load_msg(const void *src, size_t len) {
    size_t alen, bytes = len;
    if ((alen = len) > MAX_MSG_DATELEN) { alen = MAX_MSG_DATELEN; }
    Msg_msg *msg;
    if ((msg = kmalloc(sizeof(Msg_msg) + alen)) == nullptr) { return nullptr; }
    Msg_seg **segp = &msg->next;
    void *dst = msg + 1;
    goto inside;

    while (len > 0) {
        if ((alen = len) > MAX_MSG_DATELEN) { alen = MAX_MSG_DATELEN; }
        Msg_seg *seg;
        if ((seg = kmalloc(sizeof(Msg_seg) + alen)) == nullptr) { goto failed; }
        *segp = seg, segp = &(seg->next);
        dst = seg + 1;
    inside:
        memcpy(dst, src, alen);
        len -= alen, src = ((char *)src) + alen;
        *segp = nullptr;
    }
    msg->bytes = bytes;
    msg->pid = myproc()->pid;
    return msg;
failed:
    free_msg(msg);
    return nullptr;
}

static int send_msg_wait(Msg_mbox *mbox, Msg_msg *msg, Timer *timer) {
    int ret;
    Proc *current = myproc();
    assert(current != nullptr);
    acquire(&mbox->msg_mbox_lock);
    mbox->inuse++;
    Wait __wait, *wait = &__wait;
    while (mbox->max_slots <= mbox->slots) {
        assert(mbox->state == OPENED);
        wait_current_set(&mbox->senders, wait, WT_MBOX_SEND);
        ipc_add_timer(timer);
        sleeping(current, &mbox->msg_mbox_lock);
        ipc_del_timer(timer);
        wait_current_del(&mbox->senders, wait);
        if (mbox->state != OPENED || wait->wakeup_flags != WT_MBOX_SEND) {
            assert(wait->wakeup_flags == WT_INTERAUPTED);
            ret = WT_INTERAUPTED;
            goto out;
        }
    }
    assert(mbox->state == OPENED && mbox->max_slots > mbox->slots);
    ret = 0;
    add_msg(mbox, msg, 1);
out:
    mbox->inuse--;
    if (mbox->state != OPENED) {
        assert(ret == WT_INTERAUPTED && mbox->state == CLOSING);
        if (mbox->inuse == 0) { mbox_free(mbox); }
    }
    release(&mbox->msg_mbox_lock);
    return ret;
}

static int send_msg_no_wait(Msg_mbox *mbox, Msg_msg *msg) {
    uint32_t ret;
    acquire(&mbox->msg_mbox_lock);
    if (mbox->max_slots <= mbox->slots) {
        release(&mbox->msg_mbox_lock);
        return -E_MBX_FULL;
    }
    mbox->inuse++;
    assert(mbox->state == OPENED && mbox->max_slots > mbox->slots);
    ret = 0;
    add_msg(mbox, msg, 1);
    mbox->inuse--;
    release(&mbox->msg_mbox_lock);
    return ret;
}

static bool mbox_slot_isempty(int id) {
    Msg_mbox *mbox;
    bool ret = false;
    assert((mbox = get_mbox(id)) != nullptr);
    acquire(&mbox->msg_mbox_lock);
    if (mbox->slots == 0) { ret = true; }
    release(&mbox->msg_mbox_lock);
    return ret;
}

static bool mbox_slot_isfull(int id) {
    Msg_mbox *mbox;
    bool ret = false;
    assert((mbox = get_mbox(id)) != nullptr);
    acquire(&mbox->msg_mbox_lock);
    if (mbox->slots == mbox->max_slots) { ret = true; }
    release(&mbox->msg_mbox_lock);
    return ret;
}

int ipc_mbox_send(int id, Mboxbuf *buf, long timeout) {
    if (get_mbox(id) == nullptr) { return -E_INVAL; }
    if (timeout < 0 && mbox_slot_isfull(id)) { return -E_MBX_FULL; }
    Proc *current = myproc();
    Msg_msg *msg;
    Msg_mbox *mbox;
    Mm_struct *mm = current->mm;
    Mboxbuf __local_buf, *local_buf = &__local_buf;
    void *local_data = nullptr;
    int ret = -E_INVAL;
    lock_mm(mm);
    {
        either_copy_user2kernel(local_buf, !current->kernel_proc, (uint64_t)buf, sizeof(*buf));
        size_t len = local_buf->len;
        local_data = kmalloc(len);
        assert(local_data != nullptr);
        if (0 < len && len <= MAX_MSG_BYTES) {
            either_copy_user2kernel(local_data, !current->kernel_proc, (uint64_t)local_buf->data, len);
            ret = ((msg = load_msg(local_data, len)) != nullptr) ? 0 : -E_NO_MEM;
        }
    }
    unlock_mm(mm);
    kfree(local_data);
    mbox = get_mbox(id);
    if (ret == 0) {
        if (timeout >= 0) {
            ulong saved_ticks;
            Timer __timer, *timer = ipc_timer_init((ulong)timeout, &saved_ticks, &__timer);
            if ((ret = send_msg_wait(mbox, msg, timer)) == 0) { return 0; }
            assert(ret == WT_INTERAUPTED);
            ret = ipc_check_timeout((ulong)timeout, saved_ticks);
            free_msg(msg);
        } else {
            if ((ret = send_msg_no_wait(mbox, msg)) != 0) { free_msg(msg); }
        }
    }
    return ret;
}

int kernel_mbox_send(int id, Mboxbuf *buf, long timeout) {
    if (get_mbox(id) == nullptr) { return -E_INVAL; }
    if (timeout < 0 && mbox_slot_isfull(id)) { return -E_MBX_FULL; }
    Msg_msg *msg;
    Msg_mbox *mbox;
    int ret = -E_INVAL;
    size_t len = buf->len;
    assert(buf->data != nullptr);
    if (0 < len && len <= MAX_MSG_BYTES) {
        ret = ((msg = load_msg(buf->data, len)) != nullptr) ? 0 : -E_NO_MEM;
    }
    mbox = get_mbox(id);
    if (ret == 0) {
        if (timeout >= 0) {
            ulong saved_ticks;
            Timer __timer, *timer = ipc_timer_init((ulong)timeout, &saved_ticks, &__timer);
            if ((ret = send_msg_wait(mbox, msg, timer)) == 0) { return 0; }
            assert(ret == WT_INTERAUPTED);
            ret = ipc_check_timeout((ulong)timeout, saved_ticks);
            free_msg(msg);
        } else {
            if ((ret = send_msg_no_wait(mbox, msg)) != 0) { free_msg(msg); }
        }
    }
    return ret;
}

static void store_msg(Msg_msg *msg, void *dst) {
    size_t alen, len = msg->bytes;
    if ((alen = len) > MAX_MSG_DATELEN) { alen = MAX_MSG_DATELEN; }
    Msg_seg *seg = msg->next;
    const void *src = msg + 1;
    goto inside;
    while (len > 0) {
        if ((alen = len) > MAX_MSG_DATELEN) { alen = MAX_MSG_DATELEN; }
        assert(seg != nullptr);
        src = seg + 1, seg = seg->next;
    inside:
        memcpy(dst, src, alen);
        len -= alen, dst = ((char *)dst) + alen;
    }
}

static int recv_msg_wait(Msg_mbox *mbox, size_t max_bytes, Msg_msg **msg_store, Timer *timer) {
    uint32_t ret;
    Proc *current = myproc();
    assert(current != nullptr);
    acquire(&mbox->msg_mbox_lock);
    mbox->inuse++;
    Wait __wait, *wait = &__wait;
    while (mbox->slots == 0) {
        assert(mbox->state == OPENED);
        wait_current_set(&mbox->receivers, wait, WT_MBOX_RECV);
        ipc_add_timer(timer);
        sleeping(current, &mbox->msg_mbox_lock);
        ipc_del_timer(timer);
        wait_current_del(&mbox->receivers, wait);
        if (mbox->state != OPENED || wait->wakeup_flags != WT_MBOX_RECV) {
            // assert(wait->wakeup_flags == WT_INTERAUPTED);
            ret = WT_INTERAUPTED;
            goto out;
        }
    }
    assert(mbox->state == OPENED && mbox->slots > 0);
    assert(!list_empty(&mbox->msg_link));
    if ((ret = pick_msg(mbox, max_bytes, msg_store)) != 0) {
        wakeup_first(&mbox->receivers, WT_MBOX_RECV, 1);
    }
out:
    mbox->inuse--;
    if (mbox->state != OPENED) {
        assert(ret == WT_INTERAUPTED && mbox->state == CLOSING);
        if (mbox->inuse == 0) { mbox_free(mbox); }
    }
    release(&mbox->msg_mbox_lock);
    return ret;
}

static int recv_msg_no_wait(Msg_mbox *mbox, size_t max_bytes, Msg_msg **msg_store) {
    uint32_t ret;
    acquire(&mbox->msg_mbox_lock);
    if (mbox->slots == 0) {
        release(&mbox->msg_mbox_lock);
        return -E_MBX_EMPTY;
    }
    mbox->inuse++;
    Wait __wait, *wait = &__wait;
    assert(mbox->state == OPENED && mbox->slots > 0);
    assert(!list_empty(&mbox->msg_link));
    if ((ret = pick_msg(mbox, max_bytes, msg_store)) != 0) {
        wakeup_first(&mbox->receivers, WT_MBOX_RECV, 1);
    }
    mbox->inuse--;
    if (mbox->state != OPENED) {
        if (mbox->inuse == 0 && mbox->state == CLOSING) { mbox_free(mbox); }
    }
    release(&mbox->msg_mbox_lock);
    return ret;
}

int ipc_mbox_recv(int id, Mboxbuf *buf, long timeout) {
    if (get_mbox(id) == nullptr) { return -E_INVAL; }
    if (timeout < 0 && mbox_slot_isempty(id)) { return -E_MBX_EMPTY; }
    Proc *current = myproc();
    Msg_msg *msg;
    Msg_mbox *mbox;
    Mm_struct *mm = current->mm;
    Mboxbuf __local_buf, *local_buf = &__local_buf;
    void *local_data = nullptr;
    int ret = -E_INVAL;
    size_t size;
    lock_mm(mm);
    {
        either_copy_user2kernel(local_buf, !current->kernel_proc, (uint64_t)buf, sizeof(*buf));
        size = local_buf->size;
    }
    unlock_mm(mm);
    mbox = get_mbox(id);
    ulong saved_ticks;
    if (timeout >= 0) {
        Timer __timer, *timer = ipc_timer_init((ulong)timeout, &saved_ticks, &__timer);
        if ((ret = recv_msg_wait(mbox, size, &msg, timer)) != 0) {
            if (ret == WT_INTERAUPTED) { return ipc_check_timeout((ulong)timeout, saved_ticks); }
            return ret;
        }
    } else {
        if ((ret = recv_msg_no_wait(mbox, size, &msg)) != 0) { return ret; }
    }
    lock_mm(mm);
    {
        size_t len = msg->bytes;
        either_copy_kernel2user((uintptr_t)&buf->len, !current->kernel_proc, (char *)&msg->bytes,
                                sizeof(msg->bytes));
        either_copy_kernel2user((uintptr_t)&buf->from, !current->kernel_proc, (char *)&msg->pid,
                                sizeof(msg->pid));
        local_data = kmalloc(len);
        store_msg(msg, local_data);
        // vm_print(mm->pagetable);
        // print_vma_list(mm);
        either_copy_kernel2user((uintptr_t)local_buf->data, !current->kernel_proc, (char *)local_data, len);
    }
    unlock_mm(mm);
    kfree(local_data);
    free_msg(msg);
    return ret;
}

int kernel_mbox_recv(int id, Mboxbuf *buf, long timeout) {
    if (get_mbox(id) == nullptr) { return -E_INVAL; }
    if (timeout < 0 && mbox_slot_isempty(id)) { return -E_MBX_EMPTY; }
    Msg_msg *msg;
    Msg_mbox *mbox;
    int ret = -E_INVAL;
    mbox = get_mbox(id);
    ulong saved_ticks;
    if (timeout >= 0) {
        Timer __timer, *timer = ipc_timer_init((ulong)timeout, &saved_ticks, &__timer);
        if ((ret = recv_msg_wait(mbox, buf->size, &msg, timer)) != 0) {
            if (ret == WT_INTERAUPTED) { return ipc_check_timeout((ulong)timeout, saved_ticks); }
            return ret;
        }
    } else {
        if ((ret = recv_msg_no_wait(mbox, buf->size, &msg)) != 0) { return ret; }
    }
    size_t len = msg->bytes;
    store_msg(msg, buf->data);
    free_msg(msg);
    return ret;
}

int ipc_mbox_free(int id) {
    Msg_mbox *mbox;
    if ((mbox = get_mbox(id)) == nullptr) { return -E_INVAL; }
    acquire(&mbox->msg_mbox_lock);
    mbox->state = CLOSING;
    List_entry *list = &(mbox->msg_link), *le;
    while ((le = list_next(list)) != list) {
        list_del(le);
        free_msg(le2msg(le, msg_link));
    }
    wakeup_queue(&mbox->senders, WT_INTERAUPTED, 1);
    wakeup_queue(&mbox->receivers, WT_INTERAUPTED, 1);
    if (mbox->inuse == 0) { mbox_free(mbox); }
    release(&mbox->msg_mbox_lock);
    return 0;
}

int ipc_mbox_info(int id, Mboxinfo *info) {
    Msg_mbox *mbox;
    if ((mbox = get_mbox(id)) == nullptr) { return -E_INVAL; }
    acquire(&mbox->msg_mbox_lock);
    Mm_struct *mm = myproc()->mm;
    Mboxinfo __local_info, *local_info = &__local_info;
    local_info->slots = mbox->slots;
    local_info->max_slots = mbox->max_slots;
    local_info->inuse = (mbox->inuse != 0);
    local_info->has_sender = !wait_queue_empty(&mbox->senders);
    local_info->has_receiver = !wait_queue_empty(&(mbox->receivers));
    release(&mbox->msg_mbox_lock);
    lock_mm(mm);
    either_copy_kernel2user((uintptr_t)info, !myproc()->kernel_proc, (char *)local_info, sizeof(*local_info));
    unlock_mm(mm);
    return 0;
}

void mbox_cleanup(void) {
    acquire(&mboxs_lock);
    int i, j;
    for (i = 0; i < MAX_MBOX_PAGES; i++) {
        Msg_mbox *mbox;
        if ((mbox = mbox_map[i]) != nullptr) {
            for (j = 0; j < MBOX_P_PAGE; j++, mbox++) {
                if (mbox->state != CLOSED) { break; }
            }
            if (j != MBOX_P_PAGE) { continue; }
            mbox = mbox_map[i];
            for (j = 0; j < MBOX_P_PAGE; j++, mbox++) { list_del(&mbox->msg_link); }
            mbox = mbox_map[i], mbox_map[i] = nullptr;
            FreePage(kva2page((uintptr_t)mbox));
        }
    }
    release(&mboxs_lock);
}