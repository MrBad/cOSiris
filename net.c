#include <stdlib.h>
#include <string.h>
#include "console.h"
#include "net.h"

#define NB_MAX_SIZE 2000    /* Maximum size of a network buffer, in bytes */
#define NETQ_MAX_ITEMS 256  /* Maximum number of buffers in network queue */

/* Global network queue */
struct net_queue *netq;

/* Global array of network interfaces */
struct net_iface net_ifaces[MAX_NET_IFACES];

/* Global number of network interfaces */
int nics;

struct net_buf *net_buf_alloc(int size, int nic_id)
{
    struct net_buf *buf = NULL;

    if (netq->list->num_items >= NETQ_MAX_ITEMS) {
        kprintf("net_buf_alloc: queue full\n");
        return NULL;
    }
    if (size > NB_MAX_SIZE) {
        kprintf("net_buf_alloc: buffer too big (%d)\n", NB_MAX_SIZE);
        return NULL;
    }
    if (!(buf = malloc(sizeof(*buf) + size))) {
        kprintf("net_buf_alloc: out of memory\n");
        return NULL;
    }
    buf->nic = nic_id;
    buf->buf = buf + 1;
    buf->size = size;

    return buf;
}

void net_buf_free(void *buf)
{
    free(buf);
}

node_t *netq_push(struct net_buf *buf)
{
    node_t *node = NULL;

    spin_lock(&netq->lock);
    if (netq->list->num_items >= NETQ_MAX_ITEMS) {
        kprintf("Too many net_bufs\n");
        goto ret;
    }
    node = list_add(netq->list, buf);

ret:
    spin_unlock(&netq->lock);
    {
        kprintf("incoming: %d, size: %d\n", netq->list->num_items, buf->size);
        hexdump(buf->buf, buf->size);
    }
    return node;
}

struct net_buf *netq_shift()
{
    struct net_buf *buf = NULL;

    if (netq->list->num_items == 0)
        goto ret;

    spin_lock(&netq->lock);
    buf = netq->list->head->data;
    list_del(netq->list, netq->list->head);

ret:
    spin_unlock(&netq->lock);
    return buf;
}

int net_init()
{
    nics = 0;
    /* Init network queue */
    if (!(netq = calloc(1, sizeof(*netq)))) {
        kprintf("netq_init: out of memory\n");
        return -1;
    }
    if (!(netq->list = list_open(NULL))) {
        kprintf("netq_init: can't initialize queue\n");
        return -1;
    }

    return 0;
}

