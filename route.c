#include <stdlib.h>
#include "console.h"
#include "net.h"
#include "route.h"

struct route_table *rtab;

int route_exists(ip_t base, ip_t mask)
{
    struct route *r;
    node_t *node;
    
    spin_lock(&rtab->lock);
    forEach(rtab->routes, node, r) {
        if (r->base == base && r->mask == mask)
            break;
    }
    spin_unlock(&rtab->lock);

    return node ? 0 : -1;
}

/**
 * Adds a new route into routing table.
 */
int route_add(ip_t base, ip_t mask, ip_t gateway, struct netif *netif)
{
    struct route *r, *rt;
    node_t *node;
    char ip_str[16], mask_str[16];

    base = base & mask;
    if (route_exists(base, mask) == 0) {
        ip2str(base, ip_str, sizeof(ip_str));
        ip2str(mask, mask_str, sizeof(mask_str));
        net_dbg("route: %s, %s already exists\n", ip_str, mask_str);
        return -1;
    }
    if (!(r = malloc(sizeof(*r)))) {
        net_dbg("route: could not allocate memory\n");
        return -1;
    }
    r->base = base;
    r->mask = mask;
    r->gateway = gateway;
    r->netif = netif;
    
    /* Add new node, preserving descending order of the list */
    spin_lock(&rtab->lock);
    forEach(rtab->routes, node, rt) {
        if (base > rt->base)
            break;
        else if (base == rt->base && mask > rt->mask)
            break;
    }

    r->node = list_insert_before(rtab->routes, node, r);
    spin_unlock(&rtab->lock);

    return 0;
}

void route_dump()
{
    struct route *r;
    node_t *node;
    char base[16], mask[16], gw[16];

    kprintf("Destination     Gateway         Genmask         Iface\n");
    spin_lock(&rtab->lock);
    for (node = rtab->routes->tail; node; node = node->prev) {
        r = node->data;
        ip2str(r->base, base, sizeof(base));
        ip2str(r->mask, mask, sizeof(mask));
        ip2str(r->gateway, gw, sizeof(gw));
        kprintf("%-15s %-15s %-15s eth%d\n", base, gw, mask, r->netif->id);
    }
    spin_unlock(&rtab->lock);
}

void route_del(void *data)
{
    spin_lock(&rtab->lock);
    struct route *r = data;
    list_del(rtab->routes, r->node);
    spin_unlock(&rtab->lock);
}

struct route *route_find(ip_t ip)
{
    struct route *r;
    node_t *node;

    spin_lock(&rtab->lock);
    forEach(rtab->routes, node, r) {
        if ((ip & r->mask) == r->base)
            break;
    }
    spin_unlock(&rtab->lock);

    return node ? r : NULL;
}

int route_init()
{
    if (!(rtab = malloc(sizeof(*rtab)))) {
        net_dbg("route: could not allocate route table\n");
        return -1;
    }
    if (!(rtab->routes = list_open(route_del))) {
       net_dbg("route: could not init route list\n");
       return -1;
    }
    rtab->lock = 0;

    return 0;
}

