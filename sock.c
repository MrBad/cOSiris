#include <string.h>
#include <stdlib.h>
#include "console.h"
#include "assert.h"
#include "i386.h"
#include "list.h"
#include "task.h"
#include "sysfile.h"
#include "bmap.h"
#include "net.h"
#include "sock.h"

#define MAX_SOCKS 32
#define MIN_PORT 32768
#define MAX_PORTS 1024 

struct socks_table *stab; /* global sock table */
bmap_t *udp_ports; /* udp ports bitmap */
bmap_t *tcp_ports; /* tcp ports bitmap */

/**
 * Creates a new socket and adds it to socket table
 */
sock_t *sock_alloc(uint8_t proto)
{
    sock_t *sock;

    if (stab->socks->num_items == MAX_SOCKS) {
        net_dbg("MAX_SOCKS %d reached\n", MAX_SOCKS);
        return NULL;
    }
    if (!(sock = calloc(1, sizeof(*sock)))) {
        net_dbg("Could not allocate sock\n");
        return NULL;
    }
    sock->pid = getpid();
    sock->proto = proto;
    /* When deleting a node from list, will also call it's free buf. */
    sock->bufs = list_open(net_buf_free);

    /* Append it to list */
    spin_lock(&stab->lock);
    sock->node = list_add(stab->socks, sock);
    spin_unlock(&stab->lock);

    return sock;
}

/**
 * Remove the socket from list and frees it
 */
void sock_free(sock_t *sock)
{
    KASSERT(sock->lock == 0);

    spin_lock(&stab->lock);
    list_del(stab->socks, sock->node);
    spin_unlock(&stab->lock);

    spin_lock(&sock->lock);
    list_close(sock->bufs); // this will also free the net_bufs
    spin_unlock(&sock->lock);

    free(sock);
}

/**
 * Set local address. Use it in bind.
 */
int sock_set_loc_addr(sock_t *sock, sock_addr_t *addr)
{
    sock_t *s;
    node_t *node = NULL;

    spin_lock(&stab->lock);
    forEach(stab->socks, node, s) {
        if ((s->proto == sock->proto) && 
            (s->loc_addr.port == addr->port) && 
            (s->loc_addr.ip == addr->ip || s->loc_addr.ip == 0))
                break;
    }
    if (node) {
        net_dbg("cannot set %#x:%d. Address in use.\n", addr->ip, addr->port);
        spin_unlock(&stab->lock);
        return -1;
    }
    spin_lock(&sock->lock);
    sock->loc_addr.ip = addr->ip;
    sock->loc_addr.port = addr->port;
    spin_unlock(&sock->lock);
    spin_unlock(&stab->lock);

    return 0;
}

/**
 * Sets remote address. Use it in connect.
 */
int sock_set_rem_addr(sock_t *sock, sock_addr_t *addr)
{
    spin_lock(&sock->lock);
    sock->rem_addr.ip = addr->ip;
    sock->rem_addr.port = addr->port;
    spin_unlock(&sock->lock);

    return 0;
}

/**
 * Search bitmap type for a free port
 */
int port_alloc(int proto)
{
    int port = -1;

    switch (proto) {
        case S_UDP:
            port = bmap_get_free(udp_ports);
            break;
        case S_TCP:
            port = bmap_get_free(tcp_ports);
            break;
        default:
            net_dbg("Unknown proto %d\n", proto);
            break;
    }
    if (port >= 0)
        port += MIN_PORT;

    return port;
}

/**
 * Release the port
 */
void port_free(int proto, int port)
{
    port -= MIN_PORT;

    switch (proto) {
        case S_UDP:
            bmap_free(udp_ports, port);
            break;
        case S_TCP:
            bmap_free(tcp_ports, port);
            break;
        default:
            net_dbg("Unknown %d type\n", proto);
            break;
    }
}

/**
 * Sets sock local address to addr.
 * If addr->ip is 0, means ANY IP.
 * If addr->port is zero, a free port for this protocol will be assigned.
 * TODO: if ip != 0, check if we can bind this IP.
 */
int sock_bind(sock_t *sock, sock_addr_t *addr)
{
    int port;

    if (addr->port == 0) {
        if ((port = port_alloc(sock->proto)) < 0) {
            net_dbg("Could not allocate port\n");
            return -1;
        }
        addr->port = port;
    }
    if (sock_set_loc_addr(sock, addr) < 0) {
        net_dbg("Could not bind to %#x:%d\n", addr->ip, addr->port);
        return -1;
    }

    return 0;
}

/**
 * Dump sock table
 */
void sock_table_dump()
{
    node_t *node;
    sock_t *sock;
    int i = 0;

    spin_lock(&stab->lock);
    forEach(stab->socks, node, sock) {
        net_dbg("%d. pid: %d, local %#x:%d, remote:%#x:%d, stat:%d\n",
            i++, sock->pid,
            sock->loc_addr.ip, sock->loc_addr.port, 
            sock->rem_addr.ip, sock->rem_addr.port,
            sock->state);
    }
    spin_unlock(&stab->lock);
}

/**
 * Initialize the sock table
 */
int sock_init()
{
    if (!(stab = calloc(1, sizeof(*stab))))
        return -1;
    stab->socks = list_open(NULL);

    udp_ports = bmap_alloc(MAX_PORTS);
    tcp_ports = bmap_alloc(MAX_PORTS);

    return 0;
}

