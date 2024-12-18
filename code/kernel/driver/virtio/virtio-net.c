#include "virtio-net.h"

#include "assert.h"
#include "config.h"
#include "memlayout.h"
#include "plic.h"
#include "printf.h"
#include "proc.h"
#include "riscv.h"
#include "slab.h"
#include "stdio.h"
#include "string.h"
#include "virtio-mmio.h"
#include "virtio-ring.h"
#include "virtio.h"

List_entry nets_list;
#define le2net(le, member) to_struct((le), struct virtio_net, member)
static int net_device_num = 0;

static bool net_init_done = false;

int virtio_net_init(uint32_t base, int idx) {
    int ret = -1;
    if (net_device_num == 0) {
        list_init(&nets_list);
        ret = virtio_net_add(base, "net0", idx);
        net_device_num++;
        return ret;
    } else if (net_device_num == 1) {
        ret = virtio_net_add(base, "net1", idx);
        net_device_num++;
        net_init_done = true;
        return ret;
    }
    panic("There are too many net devices");
    return ret;
}

int virtio_net_add(uint32_t base, char *name, int idx) {
    assert(base == (VIRTIO_START_ADDR + ((idx)-1) * VIRTIO_STEP_SIZE));
    if (virtio_mmio_read_reg(VIRTIO_MMIO_MAGIC_VALUE, idx) != 0x74726976 ||
        virtio_mmio_read_reg(VIRTIO_MMIO_VERSION, idx) != 2 ||
        virtio_mmio_read_reg(VIRTIO_MMIO_DEVICE_ID, idx) != 1 ||
        virtio_mmio_read_reg(VIRTIO_MMIO_VENDOR_ID, idx) != 0x554d4551) {
        cprintf("could not find virtio net\n");
        return -1;
    }
    // reset device
    virtio_mmio_reset_device(idx);

    uint32_t status = 0;
    // set ACKNOWLEDGE status bit
    status |= VIRTIO_STAT_ACKNOWLEDGE;
    virtio_mmio_set_status(status, idx);

    // set DRIVER status bit
    status |= VIRTIO_STAT_DRIVER;
    virtio_mmio_set_status(status, idx);

    uint64_t features = virtio_mmio_get_host_features(idx);
    cprintf("device features: 0x%016llx\n", features);
    features =
        VIRTIO_NET_F_MTU | VIRTIO_NET_F_MAC | VIRTIO_NET_F_STATUS | VIRTIO_F_VERSION_1 | VIRTIO_F_RING_RESET;
    cprintf("after set driver features: 0x%016llx\n", features);
    virtio_mmio_set_guest_features(features, idx);

    // tell device that feature negotiation is complete.
    status |= VIRTIO_STAT_FEATURES_OK;
    virtio_mmio_set_status(status, idx);

    status = virtio_mmio_get_status(idx);
    if (!(status & VIRTIO_STAT_FEATURES_OK)) {
        cprintf("virtio net FEATURES_OK unset");
        return -2;
    }

    int qnum = 0;
    int qsize = NET_QSIZE;
    // ensure queue 0 is not in use.
    if (virtio_mmio_get_queue_ready(qnum, idx)) {
        cprintf("virtio net should not be ready");
        return -3;
    }

    uint32_t max = virtio_mmio_get_queue_size(qnum, idx);
    if (max == 0) {
        cprintf("virtio net has no queue 0");
        return -4;
    }
    if (max < qsize) {
        cprintf("virtio net max queue too short");
        return -5;
    }
    cprintf("queue_0 max size: %d\n", max);
    struct virtio_net *net = kmalloc(sizeof(struct virtio_net));
    assert(net != nullptr);
    memset(net, 0, sizeof(*net));
    net->qsize = max;

    uint8_t *net_rbuf = aligned_kmalloc(3 * PGSIZE, PGSIZE);
    assert(net_rbuf != nullptr);
    memset(net_rbuf, 0, 3 * PGSIZE);

    uint8_t *net_tbuf = aligned_kmalloc(3 * PGSIZE, PGSIZE);
    assert(net_tbuf != nullptr);
    memset(net_tbuf, 0, 3 * PGSIZE);

    int r = virtio_vring_init(&net->rx_vr, net_rbuf, 3 * PGSIZE, qsize);
    if (r) {
        cprintf("virtio_vring_init failed1: %d\n", r);
        return r;
    }

    r = virtio_vring_init(&net->tx_vr, net_tbuf, 3 * PGSIZE, qsize);
    if (r) {
        cprintf("virtio_vring_init failed2: %d\n", r);
        return r;
    }

    for (int i = 0; i < qsize; i++) {
        virtio_vring_fill_desc(net->rx_vr.desc + i, (uint64_t)&net->rx_pkt[i], sizeof(struct virtio_net_rxpkt),
                               VRING_DESC_F_WRITE, 0);
        virtio_vring_add_avail(net->rx_vr.avail, i, NET_QSIZE);
    }

    virtio_mmio_set_queue_size(qnum, qsize, idx);
    virtio_mmio_set_queue_size(qnum + 1, qsize, idx);
    // write physical addresses.
    virtio_mmio_set_queue_addr(qnum, &net->rx_vr, idx);
    virtio_mmio_set_queue_addr(qnum + 1, &net->tx_vr, idx);
    // queue is ready.
    virtio_mmio_set_queue_ready(qnum, idx);
    virtio_mmio_set_queue_ready(qnum + 1, idx);
    virtio_mmio_set_notify(qnum, idx);  // rx queue

    // tell device we're completely ready.
    status |= VIRTIO_STAT_DRIVER_OK;
    virtio_mmio_set_status(status, idx);
    dsb();

    cprintf("net device status:0x%02x\n", virtio_mmio_get_status(idx));
    net->idx = idx;
    virtio_net_cfg(net);
    snprintf(net->net_name, sizeof(net->net_name), "%s", name);
    initlock(&net->net_lock, name);
    list_add(&nets_list, &net->net_list);
    return 0;
}

void virtio_net_cfg(struct virtio_net *net) {
    uint32_t pt[(sizeof(struct virtio_net_cfg) + 3) / 4] = {0};
    struct virtio_net_cfg *cfg = (struct virtio_net_cfg *)pt;

    for (int i = 0; i < sizeof(cfg) / 4; ++i) {
        pt[i] = virtio_mmio_read_reg(VIRTIO_MMIO_CONFIG + 4 * i, net->idx);
    }
    memmove(net->mac, cfg->mac, sizeof(net->mac));
    net->mtu = cfg->mtu;
    if(net->mtu == 0){
        net->mtu = 1500;
    }
#ifdef PRINT_VIRTIO_DEVICE_INFO
    cprintf("mac: %02x:%02x:%02x:%02x:%02x:%02x\n", cfg->mac[0], cfg->mac[1], cfg->mac[2], cfg->mac[3],
           cfg->mac[4], cfg->mac[5]);
    cprintf("status: %d\n", cfg->status);
    cprintf("max_virtqueue_pairs: %d\n", cfg->max_virtqueue_pairs);
    cprintf("mtu: %d\n", cfg->mtu);
    cprintf("speed: %d\n", cfg->speed);
    cprintf("duplex: %d\n", cfg->duplex);
    cprintf("rss_max_key_size: %d\n", cfg->rss_max_key_size);
    cprintf("rss_max_indirection_table_length: %d\n", cfg->rss_max_indirection_table_length);
    cprintf("supported_hash_types: %d\n", cfg->supported_hash_types);
#endif
}

struct virtio_net *find_net_by_name(char *name) {
    List_entry *list, *le;
    list = le = &nets_list;
    while ((le = list_next(le)) != list) {
        struct virtio_net *net = le2net(le, net_list);
        if (strcmp(net->net_name, name) == 0) { return net; }
    }
    return nullptr;
}

struct virtio_net *find_net_by_index(int idx) {
    List_entry *list, *le;
    list = le = &nets_list;
    while ((le = list_next(le)) != list) {
        struct virtio_net *net = le2net(le, net_list);
        if (net->idx == idx) { return net; }
    }
    return nullptr;
}

uint32_t virtio_net_rx(uint8_t *buf, struct virtio_net *net) {
    assert(net != nullptr);
    int qnum = 0;
    acquire(&net->net_lock);
    if (net->rx_used_idx == net->rx_vr.used->idx) {  // not rx pkt
        return 0;
    }
    int nn = net->rx_used_idx % NET_QSIZE;
    net->rx_used_idx += 1;
    cprintf("%s rx idx: %d\n",net->net_name, nn);
    int idx = net->rx_vr.used->ring[nn].id;
    int rlen = net->rx_vr.used->ring[nn].len - 10;
    cprintf("rlen: %d\n", rlen);
    memcpy(buf, net->rx_pkt[idx].pkt, rlen);
    for (int i = 0; i < 32; ++i) { cprintf("%02x%s", buf[i], ((i + 1) % 16 == 0) ? "\n" : " "); }
    cprintf("\n");
    virtio_vring_add_avail(net->rx_vr.avail, idx, NET_QSIZE);
    virtio_mmio_set_notify(qnum, net->idx);
    release(&net->net_lock);
    return rlen;
}

uint32_t virtio_net_tx(uint8_t *buf, uint32_t buf_len, char *net_name){
    struct virtio_net *net = find_net_by_name(net_name);
    assert(net != nullptr);
    int idx[2];
    int qnum = 1;
    acquire(&net->net_lock);

    idx[0] = net->tx_avail_idx++ % NET_QSIZE;
    idx[1] = net->tx_avail_idx++ % NET_QSIZE;

    cprintf("%s tx idx: %d, %d\n", net_name, idx[0], idx[1]);
    virtio_vring_fill_desc(net->tx_vr.desc + idx[0], (uint64_t)&net->tx_hdr[idx[0]],
                           sizeof(struct virtio_net_txhdr), VRING_DESC_F_NEXT, idx[1]);
    virtio_vring_fill_desc(net->tx_vr.desc + idx[1], (uint64_t)buf, buf_len, 0, 0);
    virtio_vring_add_avail(net->tx_vr.avail, idx[0], NET_QSIZE);
    virtio_mmio_set_notify(qnum, net->idx);
    volatile uint16_t *pt_used_idx = &net->tx_used_idx;
    volatile uint16_t *pt_idx = &net->tx_vr.used->idx;
    dsb();
    while (*pt_used_idx == *pt_idx);
    uint32_t rlen = net->tx_vr.used->ring[net->tx_used_idx % NET_QSIZE].len;
    net->tx_used_idx += 1;
    release(&net->net_lock);
    return rlen;
}

void virtio_net_intr(int idx){
    // return;
    if(net_init_done == false) return;
    struct virtio_net *net = find_net_by_index(idx);
    assert(net != nullptr);

    uint8_t buf[VIRTIO_NET_PKT_LEN] = {0};

    virtio_mmio_set_ack(idx);
    dsb();
    while (net->rx_used_idx != net->rx_vr.used->idx) {
        virtio_net_rx(buf, net);
        dsb();
    }
}

int virtio_net_close(char *netname) {
    struct virtio_net *net = find_net_by_name(netname);
    virtio_mmio_set_status(VIRTIO_STAT_FAILED, net->idx);
    virtio_mmio_reset_device(net->idx);
    return 0;
}