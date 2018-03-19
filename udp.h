#ifndef _UDP_H
#define _UDP_H

//#include "net.h"
#include "sock.h"

struct udp_hdr {
    uint16_t sport;
    uint16_t dport;
    uint16_t len;
    uint16_t csum;
    uint8_t data[];
} __attribute__((packed));

struct udp_pseudo_hdr {
    uint32_t saddr;
    uint32_t daddr;
    uint8_t zero;
    uint8_t proto;
    uint16_t len;
} __attribute__((packed));

typedef struct udp_hdr udp_hdr_t;

int udp_init();

int udp_process(struct net_buf *buf);

int udp_connect(sock_t *sock, sock_addr_t *addr);
int udp_write(sock_t *sock, void *buf, int len);
int udp_read(sock_t *sock, void *buf, unsigned int len);
int udp_close(sock_t *sock);

#endif
