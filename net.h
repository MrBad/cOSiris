#ifndef _NET_H
#define _NET_H

#include "list.h"
#include "i386.h"

#define MAX_NET_IFACES 4

/* Network interface */
struct net_iface {
    int id;
    void *priv;
};

/* A network buffer */
struct net_buf {
    void *buf;
    int size;
    int nic;
};

/* Network queue */
struct net_queue {
    list_t *list;
    spin_lock_t lock;
};

/* The network receive queue */
extern struct net_queue *netq;

/* Array of network interfaces */
extern struct net_iface net_ifaces[MAX_NET_IFACES];

/* Number of physical nics in the system */
extern int nics;

/* Allocates a buffer */
struct net_buf *net_buf_alloc(int len, int nic_id);

/* Free a buffer */
void net_buf_free(void *buf);

/* Push buffer to network queue */
node_t *netq_push(struct net_buf *buf);

/* Gets first buffer from network queue */
struct net_buf *netq_shift();

/* Initialize the network structures */
int net_init();

#endif

