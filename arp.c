#include <arpa/inet.h>
#include <string.h>
#include "rtc.h"
#include "thread.h"
#include "console.h"
#include "arp.h"

struct arp_entry {
    uint32_t ip;
    uint8_t mac[6];
    uint8_t status;
    uint32_t expire_ts;
};

enum {
    ARPC_FREE,
    ARPC_PENDING,
    ARPC_SOLVED,
} arpc_status;

#define ARPCE_TTL_PENDING 2 /* ARP cache entry pending ttl, in seconds */
#define ARPCE_TTL 300       /* ARP cache entry ttl, in seconds */
#define ARP_REP_TIMEOUT 100 /* How many ms to wait for a reply before timeout */
#define ARP_CACHE_SIZE 32   /* How many ARP entries should we cache */

/* Global ARP cache array (table) */
struct arp_entry arp_cache[ARP_CACHE_SIZE];
/* Cache table lock */
spin_lock_t arp_cache_lock;

/**
 * Converts from buffer (network format) to internal ARP header.
 */
static arp_hdr_t *buf2arph(void *buf)
{
    arp_hdr_t *arph = buf;

    arph->htype = ntohs(arph->htype);
    arph->ptype = ntohs(arph->ptype);
    arph->op = ntohs(arph->op);

    return arph;
}

/**
 * Converts from (internal) ARP header to buffer (network)
 */
static void *arph2buf(arp_hdr_t *arph)
{
    arph->htype = htons(arph->htype);
    arph->ptype = htons(arph->ptype);
    arph->op = htons(arph->op);

    return arph;
}

/**
 * Converts from buffer to ARP IPV4 body
 */
static arp_ipv4_t *buf2arpd(void *buf)
{
    arp_ipv4_t *arpd = buf;

    arpd->sip = ntohl(arpd->sip);
    arpd->dip = ntohl(arpd->dip);

    return arpd;
}

/**
 * Converts from ARP IPV4 body to buffer (network format)
 */
static void *arpd2buf(arp_ipv4_t *arpd)
{
    arpd->sip = htonl(arpd->sip);
    arpd->dip = htonl(arpd->dip);

    return arpd;
}

/**
 * Check if entry having index idx has expired and free it
 */
static int arp_cache_check_expired(int idx)
{
    if (arp_cache[idx].expire_ts < get_time()) {
        memset(&arp_cache[idx], 0, sizeof(*arp_cache));
        return 0;
    }
    return -1;
}

/**
 * Search cache for a solved IP.
 * Returns cache entry id on succes and fills the mac buffer.
 * Returns -1 on failure.
 */
static int arp_cache_get_mac(uint32_t ip, uint8_t mac[6])
{
    int i;

    spin_lock(&arp_cache_lock);
    for (i = 0; i < ARP_CACHE_SIZE; i++) {
        arp_cache_check_expired(i);
        if (arp_cache[i].status == ARPC_SOLVED && arp_cache[i].ip == ip) {
            memcpy(mac, arp_cache[i].mac, MAC_SIZE);
            spin_unlock(&arp_cache_lock);
            return i;
        }
    }
    spin_unlock(&arp_cache_lock);
    return -1;
}

/**
 * Sets the mac address for the entry having IP.
 * Returns entry index on success, -1 if IP is not found
 */
static int arp_cache_set_mac(uint32_t ip, uint8_t mac[6])
{
    int i;

    spin_lock(&arp_cache_lock);
    for (i = 0; i < ARP_CACHE_SIZE; i++) {
        arp_cache_check_expired(i);
        if (arp_cache[i].ip == ip && arp_cache[i].status == ARPC_PENDING) {
            memcpy(arp_cache[i].mac, mac, MAC_SIZE);
            arp_cache[i].status = ARPC_SOLVED;
            arp_cache[i].expire_ts = get_time() + ARPCE_TTL;
            spin_unlock(&arp_cache_lock);
            return i;
        }
    }
    spin_unlock(&arp_cache_lock);
    return -1;
}

/**
 * Search for an empty slot in arp_cache, or use the oldest entry.
 * Sets the ip and mark the entry as pending.
 * Returns the index of the slot.
 */
static int arp_cache_set_pending(uint32_t ip)
{
    int i, mid;
    uint32_t min;

    min = ~0;
    spin_lock(&arp_cache_lock);
    for (i = 0; i < ARP_CACHE_SIZE; i++) {
        arp_cache_check_expired(i);
        if (arp_cache[i].status == ARPC_FREE)
            break;
        if (min > arp_cache[i].expire_ts) {
            min = arp_cache[i].expire_ts;
            mid = i;
        }
    }
    if (i == ARP_CACHE_SIZE)
        i = mid;

    arp_cache[i].ip = ip;
    memset(arp_cache[i].mac, 0, MAC_SIZE);
    arp_cache[i].status = ARPC_PENDING;
    arp_cache[i].expire_ts = get_time() + ARPCE_TTL_PENDING;
    spin_unlock(&arp_cache_lock);

    return i;
}

/**
 * Called when we receive an ARP request.
 * Expects buf->data to be in net format.
 */
int arp_process(struct net_buf *buf)
{
    int i, size;
    struct netif *netif; /* Network interface where we receive this */
    eth_hdr_t *eth;      /* Ethernet frame */
    arp_ipv4_t *arpd;    /* Payload should be IPV4 */
    arp_hdr_t *arph;     /* ARP header */

    eth = buf2eth(buf->data);
    arph = buf2arph(eth->data);
    size = sizeof(*eth) + sizeof(*arph) + sizeof(*arpd);

    if (arph->htype != HTYPE_ETH || arph->ptype != PTYPE_IPV4)
        return -1;
    if (arph->hlen != ETH_HLEN || arph->plen != IPV4_PLEN)
        return -1;
    if (arph->op != ARP_OP_REQ && arph->op != ARP_OP_REP)
        return -1;
    if (buf->size < size)
        return -1;

    arpd = buf2arpd(arph->data);
    netif = &netifs[buf->nic];
    if (!netif->priv)
        return -1;

    if (arph->op == ARP_OP_REQ) {
        /* If it's asking for an IP associated with this network interface,
         * send reply with it's mac address */
        for (i = 0; i < IFACE_MAX_IPS; i++)
            if (netif->ips[i] == arpd->dip)
                break;

        if (i == IFACE_MAX_IPS)
            return -1;

        /* Send an ARP reply, reusing buffer. */
        memcpy(eth->dmac, eth->smac, MAC_SIZE);
        memcpy(eth->smac, netif->mac, MAC_SIZE);
        eth2buf(eth);

        arph->op = ARP_OP_REP;
        arph2buf(arph);

        arpd->dip = arpd->sip;
        memcpy(arpd->dmac, arpd->smac, MAC_SIZE);
        arpd->sip = netif->ips[i];
        memcpy(arpd->smac, netif->mac, MAC_SIZE);
        arpd2buf(arpd);

        netif->send(netif, buf->data, size);
    } else {
        /* If it's a reply for us, save it and wake the process */
        if ((i = arp_cache_set_mac(arpd->sip, arpd->smac)) < 0)
            return -1;
        wakeup(&arp_cache[i]);
    }

    return 0;
}

/**
 * Giving ip, find the mac address.
 * Returns 0 on success and sets the mac, -1 on failure (timeout)
 */
int arp_resolve(uint32_t ip, uint8_t mac[6])
{
    int i, size;
    struct net_buf *buf;
    int nic_id = 0; // XXX This needs to be calculated, based on IP and mask!
    eth_hdr_t *eth;
    arp_hdr_t *arph;
    arp_ipv4_t *arpd;
    struct netif *netif;

    if (arp_cache_get_mac(ip, mac) > 0)
        return 0;

    /* Not in cache, let's ask the network */
    size = sizeof(*eth) + sizeof(*arph) + sizeof(*arpd);
    if (!(buf = net_buf_alloc(size, nic_id)))
        return -1;

    netif = &netifs[nic_id];
    if (!netif->priv)
        return -1;

    /* Build ethernet frame. We could use few functions here... */
    eth = (eth_hdr_t *) buf->data;
    memset(eth->dmac, 0xFF, MAC_SIZE);
    memcpy(eth->smac, netif->mac, MAC_SIZE);
    eth->eth_type = ETH_ARP;
    eth2buf(eth);

    /* ARP header */
    arph = (arp_hdr_t *) eth->data;
    arph->htype = HTYPE_ETH;
    arph->ptype = PTYPE_IPV4;
    arph->hlen = ETH_HLEN;
    arph->plen = IPV4_PLEN;
    arph->op = ARP_OP_REQ;
    arph2buf(arph);

    /* ARP IPV4 body */
    arpd = (arp_ipv4_t *) arph->data;
    memcpy(arpd->smac, netif->mac, MAC_SIZE);
    arpd->sip = netif->ips[0];
    memset(arpd->dmac, 0, MAC_SIZE);
    arpd->dip = ip;
    arpd2buf(arpd);

    i = arp_cache_set_pending(ip);

    /* Send the ARP request, and sleep */
    netif->send(netif, buf->data, size);
    net_buf_free(buf);
    sleep_onms(&arp_cache[i], ARP_REP_TIMEOUT);

    /* Waked up because reply arrived or there is a timeout */
    spin_lock(&arp_cache_lock);
    if (arp_cache[i].ip == ip && arp_cache[i].status == ARPC_SOLVED) {
        memcpy(mac, arp_cache[i].mac, MAC_SIZE);
        spin_unlock(&arp_cache_lock);
        return 0;
    }
    memset(&arp_cache[i], 0, sizeof(arp_cache[i]));
    spin_unlock(&arp_cache_lock);

    return -1;
}

void arp_cache_dump()
{
    int i;

    for (i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].status == ARPC_FREE)
            continue;
        print_ip(arp_cache[i].ip);
        kprintf("\t ");
        print_mac(arp_cache[i].mac);
        kprintf(" %-7s %d\n",
                arp_cache[i].status == ARPC_PENDING
                ? "pending" : "solved",
                arp_cache[i].expire_ts);
    }
}

void arp_test()
{
    uint32_t ip;
    uint8_t mac[6];
    kprintf("ARP test\n");
    ip = (192 << 24) | (168 << 16) | (0 << 8) | (103 << 0);
    arp_resolve(ip, mac);
    ip = (192 << 24) | (168 << 16) | (0 << 8) | (102 << 0);
    arp_resolve(ip, mac);
    ip = (192 << 24) |(168 << 16) | (1 << 8) | (100 << 0);
    arp_resolve(ip, mac);

    arp_cache_dump();
}

int arp_init()
{
    memset(arp_cache, 0, sizeof(arp_cache));
    arp_test();
    return 0;
}

