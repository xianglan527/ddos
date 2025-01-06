#include "net_device.h"

#include "exmsg.h"
#include "mbox.h"
#include "net.h"
#include "proc.h"
#include "slab.h"
#include "stdio.h"
#include "string.h"

static void recv_thread(void *arg) {
    cprintf("recv thread is running...\n");
    assert(arg != nullptr);
    Netif *netif = (Netif *)arg;
    while (1) {
        if (netif->state == NETIF_ACTIVE) {
            assert(netif->net != nullptr);
            while (netif->net->rx_used_idx != netif->net->rx_vr.used->idx) {
                memset(netif->net->recv_buf, 0, netif->net->mtu + 14);
                uint32_t rlen = virtio_net_rx(netif->net);
                assert(rlen != 0);
                Pktbuf *buf = pktbuf_alloc(rlen);
                assert(buf != nullptr);
                pktbuf_write(buf, netif->net->recv_buf, rlen);
                int ret = netif_put_in(netif, buf, 10);
                if (ret == -E_TIMEOUT) {
                    pktbuf_free(buf);
                    do_sleep(50);
                    continue;
                } else if (ret == -1) {
                    assert(netif->state != NETIF_ACTIVE);
                    pktbuf_free(buf);
                    break;
                }
                dsb();
            }
        }
        do_sleep(50);
    }
}

static void xmit_thread(void *arg) {
    cprintf("xmit thread is running...\n");
    assert(arg != nullptr);
    Netif *netif = (Netif *)arg;
    // assert(netif->state == NETIF_ACTIVE && netif->tx_mbox_id != NETIF_MBOX_ID_NULL);
    while (1) {
        if (netif->state == NETIF_ACTIVE) {
            assert(netif->net != nullptr);
            Pktbuf *buf = netif_get_out(netif, 10);
            if (buf) {
                size_t total_size = buf->total_size;
                assert(total_size <= netif->net->mtu + 14);
                memset(netif->net->tran_buf, 0, netif->net->mtu + 14);
                // pktbuf_reset_acc(buf);
                assert(pktbuf_read(buf, netif->net->tran_buf, buf->total_size) == NET_OK);
                virtio_net_tx(netif->net->tran_buf, total_size, netif->net->net_name);
                pktbuf_free(buf);
            } else {
                do_sleep(50);
            }
        }
        do_sleep(50);
    }
}

int net_tx_rx_create(Netif *netif) {
    kernel_thread_init(recv_thread, netif);
    kernel_thread_init(xmit_thread, netif);
    return NET_OK;
}

static int net_device_open(Netif *netif, void *ops_data) {
    Virtio_net *net = (Virtio_net *)ops_data;
    netif->mtu = net->mtu;
    netif->type = NETIF_TYPE_ETHER;
    netif->net = net;
    netif_set_hwaddr(netif, net->mac, VIRTIO_NET_MAC_SIZE);
    net_tx_rx_create(netif);
    return NET_OK;
}

static void net_device_close(Netif *netif) { virtio_net_close(netif->net->net_name); }

static int net_device_xmit(Netif *netif) {
    return NET_OK;
}

Netif_ops netdev_ops = {
    .open = net_device_open,
    .close = net_device_close,
    .xmit = net_device_xmit,
};