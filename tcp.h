#ifndef _TCP_H
#define _TCP_H

#include "sock.h"
#include "net.h"

struct tcp_hdr {
    uint16_t sport;
    uint16_t dport;
    uint32_t seq_no;
    uint32_t ack_no;
    uint16_t hdrl : 4;  /* Header length (data offset) in multiple 32 bits */
    uint16_t resvz : 3;
    uint16_t ns:1;
    uint16_t cwr:1;
    uint16_t ece:1;
    uint16_t urg:1;
    uint16_t ack:1;
    uint16_t psh:1;
    uint16_t rst:1;
    uint16_t syn:1;
    uint16_t fin:1;
    uint16_t win_size;
    uint16_t csum;
    uint16_t urg_ptr;
} __attribute__((packed));

int tcp_process(struct net_buf *buf);
int tcp_connect(sock_t *sock, sock_addr_t *addr);
int tcp_read(sock_t *sock, void *buffer, unsigned int length);
int tcp_write(sock_t *sock, void *buffer, unsigned int length);
int tcp_close(sock_t *sock);
int tcp_init();

#endif

