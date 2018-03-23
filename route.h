#ifndef _ROUTE_H
#define _ROUTE_H

#include "list.h"

struct route {
    uint32_t base;
    uint32_t mask;
    uint32_t gateway;
    struct netif *netif;
    node_t *node;
};

struct route_table {
    list_t *routes;
    spin_lock_t lock;
};

extern struct route_table *rtab;

int route_add(ip_t dest, ip_t mask, ip_t gateway, struct netif *netif);
void route_del(void *data);
struct route *route_find(ip_t ip);
void route_dump();
int route_init();

#endif

