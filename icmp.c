#include <string.h>
#include <arpa/inet.h>
#include "console.h"
#include "net.h"
#include "ip.h"
#include "icmp.h"

static struct icmpv4_hdr *buf2icmpv4(void *buf)
{
    struct icmpv4_hdr *icmph = buf;
    icmph->csum = ntohs(icmph->csum);

    return icmph;
}

static void *icmpv42buf(struct icmpv4_hdr *icmph)
{
    icmph->csum = htons(icmph->csum);

    return icmph;
}

struct icmpv4_echo *buf2echo(void *buf)
{
    struct icmpv4_echo *echo = buf;
    echo->id = ntohs(echo->id);
    echo->seq = ntohs(echo->seq);

    return echo;
}

/**
 * Replies to an ICMP echo packet, reusing recv buffer (echo data back).
 * Expects buf->data to be in host format.
 */
static int icmpv4_echo_reply(struct net_buf *buf)
{
    struct netif *netif = &netifs[buf->nic];
    eth_hdr_t *eth = buf->data;
    ipv4_hdr_t *iph = (ipv4_hdr_t *) eth->data;
    struct icmpv4_hdr *icmph;

    /* ICMP header */
    icmph = (struct icmpv4_hdr *)((uint8_t *)iph + (iph->ihl * 4));
    icmph->type = ICMP_T_ECHO_REPLAY;
    icmph->code = icmph->csum = 0;
    icmpv42buf(icmph);
    icmph->csum = ip_csum(icmph, iph->len - sizeof(*iph));

    /* IP header */
    uint32_t dip = iph->daddr;
    iph->daddr = iph->saddr;
    iph->saddr = dip;
    iph->ttl = 64;
    iph->csum = 0;
    ipv42buf(iph);
    iph->csum = ip_csum(iph, iphlen(iph));

    /* Ethernet */
    memcpy(eth->dmac, eth->smac, MAC_SIZE);
    memcpy(eth->smac, netif->mac, MAC_SIZE);
    eth2buf(eth);

    netif->send(netif, buf->data, buf->size);

    return 0;
}

/**
 * Called when we received an ICMP.
 * Expects buf->data to be in host format
 */
int icmpv4_process(struct net_buf *buf)
{
    eth_hdr_t *eth = buf->data;
    ipv4_hdr_t *iph = (ipv4_hdr_t *) eth->data;
    struct icmpv4_hdr *icmph = (void *)((uint8_t *)iph + (iph->ihl * 4));
    uint16_t csum = ip_csum(icmph, iph->len - sizeof(*iph));

    if (csum != 0)
        return -1;

    icmph = buf2icmpv4(icmph);

    switch (icmph->type) {
        case ICMP_T_ECHO_REQUEST:
            icmpv4_echo_reply(buf);
            break;
        case ICMP_T_DEST_UNREACHABLE:
            kprintf("Got Dest Unreachable\n");
            break;
        default:
            kprintf("ICMPV4 type %d not supported\n", icmph->type);
            break;
    }
    return 0;
}

