#include <arpa/inet.h>
#include <string.h>
#include "rtc.h"
#include "console.h"
#include "net.h"
#include "udp.h"

#define DHCP_CLI_PORT 68
#define DHCP_SRV_PORT 67

#define COSIRIS_XID_BASE 0xC0510000
/* Operation */
#define OP_REQUEST 1
#define OP_REPLY 2

/* Hardware address type */
#define HTYPE_ETH 1

/* Options */
#define MAGIC_COOKIE 0x63825363
#define OPT_SUBNET_MASK 1
#define OPT_TIME_OFFS 2
#define OPT_ROUTER 3
#define OPT_DNS 6
#define OPT_HOST_NAME 12
#define OPT_BCAST_ADDR 28
#define OPT_REQUESTED_IP_ADDR 50
#define OPT_LEASE_TIME 51
#define OPT_MESSAGE_TYPE 53
#define OPT_SERVER_ID 54
#define OPT_PARAMETER_REQUEST_LIST 55
#define OPT_END 255

/* Message type */
#define DHCP_DISCOVER 1
#define DHCP_OFFER 2
#define DHCP_REQUEST 3
#define DHCP_DECLINE 4
#define DHCP_ACK 5
#define DHCP_NACK 6
#define DHCP_RELEASE 7
#define DHCP_INFORM 8

#define ARR_LEN(arr) ((int)(sizeof(arr) / sizeof(*arr)))

struct dhcp_hdr {
    uint8_t op;
    uint8_t htype;
    uint8_t hlen;
    uint8_t hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;
    uint32_t cli_ip;
    uint32_t your_ip;
    uint32_t srv_ip;
    uint32_t gw_ip;
    uint8_t cli_mac[6];
    uint8_t pad[8];
    char srv_name[64];
    char boot_file[128];
    uint32_t cookie;
    uint8_t opts[];
};

struct dhcp_opts {
    uint8_t type;
    uint32_t subnet_mask;
    uint32_t routers[4];
    uint8_t num_routers;
    uint32_t dns[4];
    uint8_t num_dns;
    uint32_t lease_time;
    uint32_t server_id;
};

void dhcp_build_hdr(struct dhcp_hdr *dhcp, uint32_t xid, uint8_t mac[6])
{
    dhcp->op = OP_REQUEST;
    dhcp->htype = HTYPE_ETH;
    dhcp->hlen = MAC_SIZE;
    dhcp->hops = 0;
    dhcp->xid = htonl(xid);
    dhcp->secs = 0;
    dhcp->flags = htons(0x8000);
    dhcp->cli_ip = dhcp->your_ip = dhcp->srv_ip = dhcp->gw_ip = 0;
    memcpy(dhcp->cli_mac, mac, MAC_SIZE);
    dhcp->cookie = htonl(MAGIC_COOKIE);
}

/**
 * Sends DHCP DISCOVER packet
 */
int dhcp_discover(sock_t *sock, uint8_t smac[6])
{
    uint8_t buf[512], *ptr;
    uint32_t xid;
    int len;

    memset(buf, 0, sizeof(buf));
    xid = COSIRIS_XID_BASE + (sock->pid & 0xFFFF);
    dhcp_build_hdr((struct dhcp_hdr *) buf, xid, smac);

    ptr = buf + sizeof(struct dhcp_hdr);

    *ptr++ = OPT_MESSAGE_TYPE;
    *ptr++ = 1;
    *ptr++ = DHCP_DISCOVER;

    *ptr++ = OPT_PARAMETER_REQUEST_LIST;
    *ptr++ = 3;
    *ptr++ = OPT_SUBNET_MASK;
    *ptr++ = OPT_ROUTER;
    *ptr++ = OPT_DNS;

    *ptr++ = OPT_END;

    len = ptr - buf;
    len = udp_write(sock, buf, len);

    return len;
}

/**
 * Builds DHCP_REQUEST packet with opts from previous OFFER packet
 */
int dhcp_request(sock_t *sock, struct dhcp_hdr *dhcp, struct dhcp_opts *opts)
{
    char buf[512], *ptr;
    uint32_t xid, server_id;
    int len;

    memset(buf, 0, sizeof(buf));
    xid = COSIRIS_XID_BASE + (sock->pid & 0xFFFF);
    dhcp_build_hdr((struct dhcp_hdr *) buf, xid, dhcp->cli_mac);

    ptr = buf + sizeof(struct dhcp_hdr);

    *ptr++ = OPT_MESSAGE_TYPE;
    *ptr++ = 1;
    *ptr++ = DHCP_REQUEST;

    *ptr++ = OPT_SERVER_ID;
    *ptr++ = 4;
    server_id = htonl(opts->server_id);
    memcpy(ptr, &server_id, 4);
    ptr += 4;

    *ptr++ = OPT_REQUESTED_IP_ADDR;
    *ptr++ = 4;
    memcpy(ptr, &dhcp->your_ip, 4);
    ptr += 4;

    *ptr++ = OPT_HOST_NAME;
    *ptr++ = 7;
    strcpy(ptr, "cOSiris");
    ptr += 7;

    *ptr++ = OPT_PARAMETER_REQUEST_LIST;
    *ptr++ = 3;

    *ptr++ = OPT_SUBNET_MASK;
    *ptr++ = OPT_ROUTER;
    *ptr++ = OPT_DNS;

    *ptr++ = OPT_END;

    len = ptr - buf;
    len = udp_write(sock, buf, len);

    return len;
}

/**
 * Parse options from dhcp packet and save them in opts
 */
int dhcp_parse_opts(struct dhcp_hdr *dhcp, uint32_t len, struct dhcp_opts *opts)
{
    uint8_t *p = dhcp->opts;
    uint32_t i;

    if (ntohl(dhcp->cookie) != MAGIC_COOKIE) {
        net_dbg("invalid cookie\n");
        return -1;
    }
    memset(opts, 0, sizeof(*opts));
    while (p < (uint8_t *)dhcp + len) {
        if (*p == OPT_END)
            break;
        uint8_t op = *p++;
        uint8_t len = *p++;
        switch(op) {
            case OPT_MESSAGE_TYPE:
                opts->type = *p;
                break;
            case OPT_SUBNET_MASK:
                opts->subnet_mask = ntohl(*(uint32_t *)p);
                break;
            case OPT_ROUTER:
                for (i = 0; i < ARR_LEN(opts->routers) && len > 0; i++) {
                    opts->routers[i] = ntohl(*(uint32_t *)p);
                    opts->num_routers++;
                    p += sizeof(uint32_t);
                    len -= sizeof(uint32_t);
                }
                break;
            case OPT_DNS:
                for (i = 0; i < ARR_LEN(opts->dns) && len > 0; i++) {
                    opts->dns[i] = ntohl(*(uint32_t *)p);
                    opts->num_dns++;
                    p += sizeof(uint32_t);
                    len -= sizeof(uint32_t);
                }
                break;
            case OPT_LEASE_TIME:
                opts->lease_time = ntohl(*(uint32_t *)p);
                break;
            case OPT_SERVER_ID:
                opts->server_id = ntohl(*(uint32_t *)p);
                break;
            default:
                net_dbg("DHCP: unknown option %d\n", *p);
                break;
        }
        p += len;
    }

    return 0;
}

/**
 * Sets network interface IP and adds routes for it, from DHCP response
 */
int dhcp_set_netif(struct netif *netif, struct dhcp_hdr *dhcp,
        struct dhcp_opts *opts)
{
    int i;
    char ip[16], mask[16];
    uint32_t netaddr;

    netif->ips[0] = ntohl(dhcp->your_ip);
    net_dbg("Setting eth%d IP to %s\n",
            netif->id, ip2str(netif->ips[0], ip, sizeof(ip)));
    if (opts->num_routers > 0) {
        net_dbg("TODO: add default route to %s for eth%d\n",
                ip2str(opts->routers[0], ip, 16), netif->id);
    }
    if (opts->subnet_mask) {
        netaddr = netif->ips[0] & opts->subnet_mask;
        net_dbg("TODO: add route: net: %s mask: %s for eth%d\n",
                ip2str(netaddr, ip, 16),
                ip2str(opts->subnet_mask, mask, 16), netif->id);
    }
    if (opts->num_dns) {
        for (i = 0; i < opts->num_dns; i++)
            if (dns_add(opts->dns[i]) == 0)
                net_dbg("DNS: %s\n", ip2str(opts->dns[i], ip, sizeof(ip)));
    }

    return 0;
}

/**
 * Receive a response from DHCP server and process it.
 *   TODO: use non-blocking sockets with a timeout
 */
int dhcp_recv(sock_t *sock, struct netif *netif)
{
    int len;
    char buf[640], your_ip[16], server_ip[16];
    uint32_t xid = COSIRIS_XID_BASE + (sock->pid & 0xFFFF);
    struct dhcp_opts opts;
    struct dhcp_hdr *dhcp = (struct dhcp_hdr *) buf;

    len = udp_read(sock, buf, sizeof(buf));

    if (ntohl(dhcp->xid) != xid) {
        net_dbg("DHCP: invalid xid!\n");
        return -1;
    }
    if (dhcp_parse_opts(dhcp, len, &opts) < 0) {
        net_dbg("Could not parse DHCP options\n");
        return -1;
    }

    ip2str(ntohl(dhcp->your_ip), your_ip, sizeof(your_ip));
    ip2str(opts.server_id, server_ip, sizeof(server_ip));

    switch (opts.type) {
        case DHCP_OFFER:
            net_dbg("Got DHCP OFFER for %s, from %s\n", your_ip, server_ip);
            dhcp_request(sock, dhcp, &opts);
            dhcp_recv(sock, netif);
            break;
        case DHCP_ACK:
            net_dbg("Got DHCP ACK for %s, from %s\n", your_ip, server_ip);
            dhcp_set_netif(netif, dhcp, &opts);
            break;
        default:
            net_dbg("Unhandled DHCP type: %d\n", opts.type);
            return -1;
    }

    return 0;
}

/**
 * Sets the network interface netif using DHCP
 */
int dhcp_init(struct netif *netif)
{
    struct sock *sock;
    struct sock_addr addr;
    int ret;

    if (!netif->priv)
        return -1;
    netif->ips[0] = 0;
    sock = sock_alloc(S_UDP);

    addr.ip = ADDR_ANY;
    addr.port = DHCP_CLI_PORT;
    sock_bind(sock, &addr);

    addr.ip = make_ip(255, 255, 255, 255);
    addr.port = DHCP_SRV_PORT;
    udp_connect(sock, &addr);

    kprintf("Sending DHCP discover on eth%d\n", netif->id);
    dhcp_discover(sock, netif->mac);
    ret = dhcp_recv(sock, netif);

    udp_close(sock);

    return ret;
}

