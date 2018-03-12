#ifndef _ARP_H
#define _ARP_H

#include "net.h"

struct arp_hdr {
    uint16_t htype; /* Hardware type */
    uint16_t ptype; /* Protocol type */
    uint8_t hlen;   /* Hardware length */
    uint8_t plen;   /* Protocol length */
    uint16_t op;    /* Operation */
    uint8_t data[]; /* Payload */
}__attribute__ ((packed));

typedef struct arp_hdr arp_hdr_t;

struct arp_ipv4 {
    uint8_t smac[6]; /* Source mac address */
    uint32_t sip;    /* Source IP */
    uint8_t dmac[6]; /* Destination mac address */
    uint32_t dip;    /* Destination IP */
} __attribute__ ((packed));

typedef struct arp_ipv4 arp_ipv4_t;

#define HTYPE_ETH 0x01
#define PTYPE_IPV4 0x800
#define ETH_HLEN 0x6
#define IPV4_PLEN 0x4
#define ARP_OP_REQ 0x1
#define ARP_OP_REP 0x2 

int arp_init();
int arp_process(struct net_buf *buf);

#endif
