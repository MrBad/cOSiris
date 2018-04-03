#ifndef _NET_H
#define _NET_H

#include "list.h"
#include "i386.h"

#define MAC_SIZE 6
#define MAX_NET_IFACES 8
#define IFACE_MAX_IPS 4

typedef struct netif netif_t;
typedef int (*netif_send_t) (netif_t *netif, void *buf, int size);
typedef int (*netif_up_t) (netif_t *netif);
typedef int (*netif_down_t) (netif_t *netif);

/* Network Interface */
struct netif {
    int id;
    uint8_t mac[6];
    uint32_t ips[IFACE_MAX_IPS];
    netif_up_t up;
    netif_down_t down;
    netif_send_t send;
    void *priv;
};

/* Array of network interfaces */
extern netif_t netifs[MAX_NET_IFACES];

/* Number of physical nics in the system */
extern int nics;

/* Ethernet Frame Header */
struct eth_hdr {
    uint8_t dmac[6];
    uint8_t smac[6];
    uint16_t eth_type;
    uint8_t data[];
} __attribute__((packed));

typedef struct eth_hdr eth_hdr_t;

/* Network buffer */
struct net_buf {
    uint8_t nic;        /* Network interface id */
    uint16_t size;      /* Size of the buffer */
    int captured;       /* Is this buffer captured? */
    struct {
        void *data;     /* Points to final payload, after decapsulation */
        uint32_t len;   /* Length of final data */
        void *ptr;     /* Variable read pointer */
    } u;
    void *data;         /* Buffer start */
};

/* Network queue */
struct net_queue {
    list_t *list;
    spin_lock_t lock;
};

typedef uint32_t ip_t;
/* The network receive queue */
extern struct net_queue *netq;

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

uint32_t make_ip(uint8_t a, uint8_t b, uint8_t c, uint8_t d);
char *ip2str(uint32_t ip, char *str, int size);
void print_ip(uint32_t ip);
void print_mac(uint8_t mac[6]);
eth_hdr_t *buf2eth(void *buf);
void *eth2buf(eth_hdr_t *eth);

#define ETH_P_ARP 0x0806
#define ETH_P_IP 0x0800

int is_bige;

#define net_dbg kprintf

#endif

