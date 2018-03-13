#ifndef _ICMP_H
#define _ICMP_H

#include "net.h"

struct icmpv4_hdr {
    uint8_t type;
    uint8_t code;
    uint16_t csum;
    uint8_t data[];
} __attribute__((packed));

struct icmpv4_echo {
    uint16_t id;
    uint16_t seq;
    uint8_t data[];
} __attribute__((packed));

int icmpv4_process(struct net_buf *buf);

#define ICMP_T_ECHO_REPLAY 0
#define ICMP_T_DEST_UNREACHABLE 3
#define ICMP_T_ECHO_REQUEST 8

#endif
