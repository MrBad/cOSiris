#include <arpa/inet.h>
#include <string.h>
#include "console.h"
#include "assert.h"
#include "task.h"
#include "net.h"
#include "ip.h"
#include "route.h"
#include "sock.h"
#include "arp.h"
#include "udp.h"

udp_hdr_t *buf2udp(void *buf)
{
    udp_hdr_t *udp = buf;

    udp->sport = ntohs(udp->sport);
    udp->dport = ntohs(udp->dport);
    udp->len = ntohs(udp->len);
    udp->csum = ntohs(udp->csum);

    return udp;
}

void *udp2buf(udp_hdr_t *udp)
{
    udp->sport = htons(udp->sport);
    udp->dport = htons(udp->dport);
    udp->len = htons(udp->len);
    udp->csum = htons(udp->csum);

    return udp;
}

/**
 * Computes UDP checksum.
 * Expects udp and iph to be in host format.
 */
uint16_t udp_csum(udp_hdr_t *udp, ipv4_hdr_t *iph)
{
    uint32_t sum = 0, ip;
    uint16_t *ptr, i;

    /* Computes UDP 'pseudo' header sum */
    ip = htonl(iph->saddr);
    ptr = (uint16_t *) &ip;
    sum += ptr[0] + ptr[1];

    ip = htonl(iph->daddr);
    ptr = (uint16_t *) &ip;
    sum += ptr[0] + ptr[1];

    sum += htons(iph->proto);
    sum += htons(udp->len);

    /* Computes UDP sum. */
    ptr = (uint16_t *) udp;
    for (i = 0; i < udp->len / 2; i++) {
        if (i < (sizeof (*udp) / 2))
            sum += htons(*ptr++); // UDP header is in host format, swap bytes.
        else
            sum += *ptr++;
    }
    if (udp->len % 2)
        sum += *(uint8_t *) ptr;
    sum = (sum & 0xFFFF) + (sum >> 16);

    return ~sum;
}

/**
 * Process incoming udp packets
 */
int udp_process(struct net_buf *buf)
{
    eth_hdr_t *eth = buf->data;
    ipv4_hdr_t *iph = (ipv4_hdr_t *) eth->data;
    udp_hdr_t *udp = buf2udp(ip_data(iph));
    uint16_t csum;
    sock_t *s;
    node_t *node;

    if (udp->csum != 0) {
        if ((csum = udp_csum(udp, iph)) != 0) {
            net_dbg("UDP: bad csum!\n");
            return -1;
        }
    }

    /* Check if any socket match this packet. */
    /* XXX Should we also check the interface? */
    spin_lock(&stab->lock);
    forEach(stab->socks, node, s) {
        if ((s->proto == S_UDP) &&
            (s->loc_addr.port == udp->dport) &&
            ((s->loc_addr.ip == 0) || (s->loc_addr.ip == iph->daddr)) &&
            ((s->rem_addr.port == 0) || (s->rem_addr.port == udp->sport)) &&
            ((s->rem_addr.ip == 0 || s->rem_addr.ip == 0xFFFFFFFF)
              || (s->rem_addr.ip == iph->saddr))
        ) {
            /* Add packet to socket recv list. Set where final data starts,
             *  it's size and mark it as captured. */
            spin_lock(&s->lock);
            if (s->bufs->num_items < SOCK_MAX_BUFS) {
                buf->captured = 1;
                buf->u.data = udp->data;
                buf->u.len = udp->len - sizeof(*udp);
                list_add(s->bufs, buf);
                wakeup(&s->bufs);
                net_dbg("Socket [loc: %08x:%d, rem: %08x:%d]\n"
                        " match [dst: %08x:%d, src: %08x:%d], %d bytes\n",
                        s->loc_addr.ip, s->loc_addr.port,
                        s->rem_addr.ip, s->rem_addr.port,
                        iph->daddr, udp->dport,
                        iph->saddr, udp->sport, buf->u.len);
            } else {
                net_dbg("Drop   [loc: %08x:%d, rem: %08x:%d], %d bytes. "
                        "Too many buffers in recv queue\n",
                        iph->daddr, udp->dport,
                        iph->saddr, udp->sport, udp->len - sizeof(*udp));
            }
            spin_unlock(&s->lock);
            break;
        }
    }
    spin_unlock(&stab->lock);

    if (!node) {
        net_dbg("Dropping {local:%#x:%d, remote:%#x:%d}. No match.\n",
                iph->daddr, udp->dport, iph->saddr, udp->sport);
    }
    return 0;
}

/**
 * Associate remote addr to this socket
 */
int udp_connect(sock_t *sock, sock_addr_t *addr)
{
    sock_addr_t loc_addr;

    // find route ?! //
    if (sock->loc_addr.port == 0) {
        loc_addr.ip = ADDR_ANY;
        loc_addr.port = port_alloc(S_UDP);
        if (sock_set_loc_addr(sock, &loc_addr) < 0)
            return -1;
    }
    sock_set_rem_addr(sock, addr);
    sock->state = SOCK_CONNECTED;

    return 0;
}

int udp_read(sock_t *sock, void *buf, unsigned int size)
{
    struct net_buf *nbuf;

    if (sock->bufs->num_items == 0) {
        if (current_task->state == TASK_EXITING)
            return -1;
        if (current_task->state == TASK_RUNNING)
            sleep_on(&sock->bufs);
    }
    /* Read as much as we can from datagram */
    spin_lock(&sock->lock);
    nbuf = ((node_t *) sock->bufs->head)->data;
    if (nbuf->u.len < size)
        size = nbuf->u.len;
    memcpy(buf, nbuf->u.data, size);
    list_del(sock->bufs, sock->bufs->head);
    spin_unlock(&sock->lock);

    return size;
}

/**
 * Sets ethernet frame destination mac address
 * This should be:
 *   if ip is broadcast address, MAC is 0
 *   if ip is is inside LAN, do an ARP for it
 *   if ip is outside the LAN, sets the MAC of the gateway
 * Returns 0 on succes, -1 on error
 */
int eth_set_dmac(uint8_t dmac[6], uint32_t dest_ip)
{
    struct route *r;
    char ip[16];

    /* Destination is broadcast addr? */
    if (dest_ip == 0xFFFFFFFF) {
        memset(dmac, 0, MAC_SIZE);
        return 0;
    }
    if (!(r = route_find(dest_ip))) {
        ip2str(dest_ip, ip, sizeof(ip));
        net_dbg("Could not find route for %s\n", ip);
        return -1;
    }
    /* Gateway matched for the route, IP is outside LAN */
    if (r->base == 0) {
        if (arp_resolve(r->gateway, dmac) < 0) {
            ip2str(r->gateway, ip, sizeof(ip));
            net_dbg("Could not solve gateway MAC at %s\n", ip);
            return -1;
        }
        return 0;
    }
    if (arp_resolve(dest_ip, dmac) < 0) {
        ip2str(dest_ip, ip, sizeof(ip));
        net_dbg("Could not solve MAC for: %s\n", ip);
        return -1;
    }

    return 0;
}

int udp_write(sock_t *sock, void *buf, int len)
{
    struct net_buf *nbuf;
    eth_hdr_t *eth;
    ipv4_hdr_t *iph;
    udp_hdr_t *udp;
    struct netif *netif;

    //netif = &netifs[0];
    netif = sock->netif;

    int hdrs_len = sizeof(*eth) + sizeof(*iph) + sizeof(*udp);
    if (len > 1500 - hdrs_len)
        len = 1500 - hdrs_len;

    if (!(nbuf = net_buf_alloc(hdrs_len + len, netif->id))) {
        net_dbg("Could not allocate net buffer\n");
        return -1;
    }

    eth = nbuf->data;
    if (eth_set_dmac(eth->dmac, sock->rem_addr.ip) < 0) {
        net_dbg("Could not set destination MAC\n");
        return -1;
    }
    memcpy(eth->smac, netif->mac, MAC_SIZE);
    eth->eth_type = ETH_P_IP;
    eth2buf(eth);

    iph = (ipv4_hdr_t *) eth->data;
    iph->version = IPV4;
    iph->ihl = sizeof(*iph) / 4;
    iph->tos = 0;
    iph->len = sizeof(*iph) + sizeof(*udp) + len;
    iph->id = 0;
    iph->flags = 2;
    iph->frag_offs = 0;
    iph->ttl = 64;
    iph->proto = IP_P_UDP;
    iph->csum = 0;
    iph->saddr = sock->loc_addr.ip ? sock->loc_addr.ip: netif->ips[0];
    iph->daddr = sock->rem_addr.ip;

    udp = ip_data(iph);
    memcpy(udp->data, buf, len);
    udp->sport = sock->loc_addr.port;
    udp->dport = sock->rem_addr.port;
    udp->len = sizeof(*udp) + len;
    udp->csum = 0;
    int csum = udp_csum(udp, iph);
    udp2buf(udp);
    udp->csum = csum;

    ipv42buf(iph);
    iph->csum = ip_csum(iph, iphlen(iph));

    netif->send(netif, nbuf->data, nbuf->size);
    net_buf_free(nbuf);

    return len;
}

int udp_close(sock_t *sock)
{
    sock_free(sock);

    return 0;
}

int udp_init()
{
#if 0
    sock_t *sock = sock_alloc(S_UDP);
    sock_addr_t addr;
    int n;

    /* Let's do a short test! */
    net_dbg("Waiting for 2 UDP packets on port 5000\n");
    addr.ip = ADDR_ANY;
    addr.port = 5000;
    sock_bind(sock, &addr);

    char buf[512];
    n = udp_read(sock, buf, 512);
    buf[n] = 0;
    net_dbg("read %d bytes: {%s}\n", n, buf);

    n = udp_read(sock, buf, 512);
    buf[n] = 0;
    net_dbg("read %d bytes: {%s}\n", n, buf);
    udp_close(sock);

    sock = sock_alloc(S_UDP);
    addr.ip = (192 << 24) | (168 << 16) | (0 << 8) | 102;
    addr.port = 2000;
    udp_connect(sock, &addr);
    strcpy(buf, "Hello cOSiris UDP network\n."
            "This message is sent to port 2000, to 192.168.0.102\n");
    udp_write(sock, buf, strlen(buf));
    udp_close(sock);

    sock_table_dump();
#endif
    return 0;
}

