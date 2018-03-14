#include <arpa/inet.h>
#include <string.h>
#include "console.h"
#include "net.h"
#include "icmp.h"
#include "ip.h"

/**
 * Computes the checksum of the buffer having length
 * DOCS: https://tools.ietf.org/html/rfc1071
 */
uint16_t ip_csum(void *buf, int len)
{
    uint32_t sum = 0; 
    uint16_t *ptr = buf;

    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }
    if (len > 0)
        sum += *(uint8_t *) ptr;
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    return ~sum;
}

/**
 * An ugly function to dump an IP header
 */
void ipv4_hdr_dump(ipv4_hdr_t *iph)
{
    kprintf("version: %d\n", iph->version);
    kprintf("ihl: %d (%d bytes)\n", iph->ihl, iph->ihl * 4);
    kprintf("tos: %d\n", iph->tos);
    kprintf("total length: %d bytes\n", iph->len);
    kprintf("id: %#x\n", iph->id);
    kprintf("flags: %#x\n", iph->flags);
    kprintf("fragment offset: %#x (%#x bytes)\n", 
            iph->frag_offs, iph->frag_offs << 3);
    kprintf("ttl: %d\n", iph->ttl);
    kprintf("protocol: %d\n", iph->proto);
    kprintf("csum: %#x %s\n", iph->csum);
    kprintf("saddr: ");
    print_ip(iph->saddr);
    kprintf("\n");
    kprintf("daddr: ");
    print_ip(iph->daddr);
    kprintf("\n");
}

/**
 * Gets the length of the IP header.
 *  buf is the start of the IP header, in network format.
 */
int iphlen(void *buf)
{
    uint8_t hlen = *(uint8_t *)buf;

    if (is_bige)
        hlen = (hlen >> 4);

    return (hlen & 0x07) << 2;
}

/**
 * Converts from network format IP header into internal representation
 */
static ipv4_hdr_t *buf2ipv4(void *buf)
{
    ipv4_hdr_t *iph = buf;

    if (!is_bige) {
        uint8_t ihl = iph->version;
        iph->version = iph->ihl;
        iph->ihl = ihl;
        uint16_t frg = ntohs(*((uint16_t *)iph + 3));
        iph->flags = (frg >> 13) & 0x7;
        iph->frag_offs = frg & 0x1FFF;
    }
    iph->len = ntohs(iph->len);
    iph->id = ntohs(iph->id);
    iph->csum = ntohs(iph->csum);
    iph->saddr = ntohl(iph->saddr);
    iph->daddr = ntohl(iph->daddr);

    return iph;
}

/**
 * Converts an IP header to network format
 */
void *ipv42buf(ipv4_hdr_t *iph)
{
    if (!is_bige) {
        uint8_t ihl = iph->version;
        iph->version = iph->ihl;
        iph->ihl = ihl;
        uint16_t *ptr = (uint16_t *)iph + 3;
        uint16_t val = *ptr;
        // XXX more tests here!
        *ptr = htons(((val & 0x7) << 13) | ((val & ~0x7) >> 3));
    }
    iph->len = ntohs(iph->len);
    iph->id = ntohs(iph->id);
    iph->csum = ntohs(iph->csum);
    iph->saddr = ntohl(iph->saddr);
    iph->daddr = ntohl(iph->daddr);

    return iph;
}

/**
 * Process an IP packet.
 * Expects buf->data to be in network format.
 */
int ip_process(struct net_buf *buf)
{
    int i;
    eth_hdr_t *eth = buf2eth(buf->data);
    uint16_t csum = ip_csum(eth->data, iphlen(eth->data));
    ipv4_hdr_t *iph = buf2ipv4(eth->data);
    struct netif *netif = &netifs[buf->nic];

    //ipv4_hdr_dump(iph);

    if (iph->version != IPV4)
        return -1;
    if (iph->ihl < 5)
        return -1;
    if (iph->ttl == 0)
        return -1;
    if (csum != 0)
        return -1;

    /* Is this packet for us? */
    for (i = 0; i < IFACE_MAX_IPS; i++)
        if (netif->ips[i] == iph->daddr)
            break;
    if (i == IFACE_MAX_IPS)
        return -1;

    switch (iph->proto) {
        case ICMPV4:
            icmpv4_process(buf);
            break;
        default:
            kprintf("Unsupported IP header protocol: %d\n", iph->proto);
            break;
    }

    return 0;
}

