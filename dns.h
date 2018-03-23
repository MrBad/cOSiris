#ifndef _DNS_H
#define _DNS_H
/* Simple DNS client for cOSiris
 * Docs: http://www.networksorcery.com/enp/protocol/dns.htm
 */
#include "net.h"
#define MAX_DNS_SERVERS 4

/* Query types */
#define DNS_T_A 1           /* IN A request (IP)*/
#define DNS_T_NS 2          /* Gets the name servers of the host */
#define DNS_T_CNAME 5       /* Canonical name */
#define DNS_T_SOA 6         /* Not implemented */
#define DNS_T_PTR 12        /* Not implemented */
#define DNS_T_HINFO 13      /* Not implemented */
#define DNS_T_MINFO 14      /* Not implemented */
#define DNS_T_MX 15         /* Mail servers of host */
#define DNS_T_TXT 16        /* TXT entry of the host */

struct dns_answer {
    char *name;
    uint16_t type;
    uint16_t class;
    uint32_t ttl;
    uint16_t prio; // priority - used in MX
    union {
        uint32_t ip;
        char *name;
    } r;
};

struct dns_res {
    uint8_t len;    /* number of answers */
    uint8_t size;   /* size in number of answers */
    struct dns_answer answers[];
};

extern uint32_t dns_servers[MAX_DNS_SERVERS];

/* Adds a DNS IP to dns table */
int dns_add(uint32_t dns_ip);

/* Dumps dns servers */
void dns_dump();

int dns_init();

/**
 * Query host, for a specific type (ex DNS_T_MX)
 * Returns a new allocated dns_res object on success, or NULL on error
 *  dns_res must be free with dns_res_del
 */
struct dns_res *dns_query(char *host, int type);

/**
 * Frees a dns_res object
 */
void dns_res_del(struct dns_res *res);

/**
 * Dumps a result to screen
 */
void dns_res_dump(struct dns_res *res);

/**
 * Shortcut, uncached host to ip resolver
 */
ip_t dns_hostbyname(char *hostname);

#endif

