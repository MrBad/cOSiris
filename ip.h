#ifndef _IP_H
#define _IP_H

#include <sys/types.h>

struct ipv4_hdr {
    uint8_t version : 4;
    uint8_t ihl : 4;
    uint8_t tos;
    uint16_t len;
    uint16_t id;
    uint16_t flags : 3;
    uint16_t frag_offs : 13;
    uint8_t ttl;
    uint8_t proto;
    uint16_t csum;
    uint32_t saddr;
    uint32_t daddr;
} __attribute__ ((packed));

typedef struct ipv4_hdr ipv4_hdr_t;

#define IPV4 4

uint16_t ip_csum(void *buf, int len);

int ip_process(struct net_buf *buf);

void *ipv42buf(ipv4_hdr_t *iph);

int iphlen(void *buf);

#define ICMPV4 1

#endif
