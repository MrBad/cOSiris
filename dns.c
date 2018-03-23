/**
 * Simple DNS client system for cOSiris
 * Note: this does not support caching yet
 */
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "console.h"
#include "assert.h"
#include "task.h"
#include "dns.h"
#include "sock.h"
#include "udp.h"

struct dns_hdr {
    uint16_t id;            /* Identification number */
    uint16_t qr : 1;        /* Query response */
    uint16_t opcode : 4;    /* Opcode */
    uint16_t aa : 1;        /* Authoritative answer */
    uint16_t tc : 1;        /* Message truncated */
    uint16_t rd : 1;        /* Recursion desired */
    uint16_t ra : 1;        /* Recursion available */
    uint16_t z : 1;         /* z */
    uint16_t ad : 1;        /* Authenticated data */
    uint16_t cd : 1;        /* Checking disabled */
    uint16_t rcode : 4;     /* Return code */
    uint16_t q_count;       /* Total questions entries */
    uint16_t ans_count;     /* Total answer entries */
    uint16_t auth_count;    /* Total authority entries */
    uint16_t add_count;     /* Total additional entries */
    uint8_t data[];
} __attribute__((packed));

struct dns_query {
    uint16_t type;
    uint16_t class;
};

/* Opcodes */
#define DNS_OP_QUERY 0      /* Standard query */
#define DNS_OP_IQUERY 1     /* Inverse query */
#define DNS_OP_STATUS 2     /* Server status request */
#define DNS_OP_NOTIFY 5
#define DNS_OP_UPDATE 6

/* Return codes */
#define DNS_RC_OK 0         /* Request completed successfully */
#define DNS_RC_FMTERR 1     /* Format error */
#define DNS_RC_SRVERR 2     /* Server error */
#define DNS_RC_NAMERR 3     /* Name error */
#define DNS_RC_NOTIMPL 4    /* Not implemented */
#define DNS_RC_REFUSED 5    /* DNS refused to perform query */
#define DNS_RC_YXDOMAIN 6
#define DNS_RC_YXRRSET 7
#define DNS_RC_NXRRSET 8
#define DNS_RC_NOTAUTH 9
#define DNS_RC_NOTZONE 10

char *dns_types[] = {
    [DNS_T_A] = "A",
    [DNS_T_NS] = "NS",
    [DNS_T_CNAME] = "CNAME",
    [DNS_T_SOA] = "SOA",
    [DNS_T_PTR] = "PTR",
    [DNS_T_HINFO] = "HINFO",
    [DNS_T_MINFO] = "MINFO",
    [DNS_T_MX] = "MX",
    [DNS_T_TXT] = "TXT"
};

/* Class values - we use only internet */
#define DNS_C_IN 1

#define DNS_SRV_PORT 53

/* Global list of DNS servers */
uint32_t dns_servers[MAX_DNS_SERVERS];

int dns_add(uint32_t dns_ip)
{
    int i;

    for (i = 0; i < MAX_DNS_SERVERS; i++) {
        if (dns_servers[i] == 0) {
            dns_servers[i] = dns_ip;
            break;
        }
    }

    return i == MAX_DNS_SERVERS ? -1 : 0;
}

void dns_dump()
{
    int i;
    char ip[16];

    kprintf("DNS: ");
    for (i = 0; i < MAX_DNS_SERVERS; i++) {
        if (dns_servers[i]) {
            ip2str(dns_servers[i], ip, sizeof(ip));
            kprintf("%s%s", i == 0 ? "": ", ", ip);
        }
    }
    kprintf("\n");
}

static struct dns_res *dns_res_new(int num_answers)
{
    struct dns_res *res;
    int size = sizeof(*res) + num_answers * sizeof(struct dns_answer);
    if (!(res = calloc(1, size))) {
        net_dbg("dns_res: out of memory\n");
        return NULL;
    }
    res->size = num_answers;

    return res;
}

void dns_res_del(struct dns_res *res)
{
    int i;

    for (i = 0; i < res->size; i++) {
        if (res->answers[i].name)
            free(res->answers[i].name);
        if (res->answers[i].type != DNS_T_A)
            free(res->answers[i].r.name);
    }
    free(res);
}

static int dns_make_id(char *host)
{
    (void) host;
    return getpid();
}

/**
 * Convert from host www.google.com to 3www6google3com
 * Write the result to buf having size bytes
 * Returns new size of the buffer
 */
static int host2dns_name(char *host, char *buf, unsigned int size)
{
    int i;

    if (size <= strlen(host) + 2) {
        net_dbg("host2dns_name: buf too small\n");
        return -1;
    }
    for (i = 0, buf++; size > 2; host++, size--) {
        if (*host == '.' || *host == 0) {
            *(buf - (i + 1)) = i;// +'0';
            if (*host == 0) break;
            i = 0;
            buf++;
            continue;
        }
        i++;
        *buf++ = *host;
    }
    *buf = 0;
    size -= 2;
    return size;
}

/**
 * Converts from dns format name to readable string.
 * Start from s.
 * Writes the result to buf.
 */
static char *dns2host_name(struct dns_hdr *dns, int dns_size, char *s)
{
    char *end = (char *) dns + dns_size;
    int i = 0, j;
    uint8_t n = 0;
    int size = 32;
    char *buf, *nbuf;

    if (!(buf = malloc(size)))
        return NULL;

    while (*s && s < end) {
        if (n > 0)
            buf[i++] = '.';
        n = *s++;
        if (n == 0xc0) {
            s = (char *)dns + *s;
            n = 0;
        } else {
            if (n >= size - i - 1) {
                size *= 2; // i + n + 1 can be enough, but is slower;
                if (!(nbuf = realloc(buf, size))) {
                    free(buf);
                    return NULL;
                }
                buf = nbuf;
            }
            for (j = 0; j < n && i < size; j++)
                buf[i++] = *s++;
        }
    }
    buf[i] = 0;

    return buf;
}

static struct dns_hdr *dns2buf(struct dns_hdr *dns)
{
    uint16_t flags = 0, *ptr;
    dns->id = htons(dns->id);
    dns->q_count = htons(dns->q_count);
    dns->ans_count = htons(dns->ans_count);
    dns->auth_count = htons(dns->auth_count);
    dns->add_count = htons(dns->add_count);
    ptr = ((uint16_t *) dns) + 1;
    flags = (dns->qr << 15) | (dns->opcode << 11) | (dns->aa << 10)
        | (dns->tc << 9) | (dns->rd << 8) | (dns->ra << 7) | (dns->z << 6)
        | (dns->ad << 5) | (dns->cd << 4) | (dns->rcode);
    *ptr = htons(flags);
    return dns;
}

/**
 * Converts from raw dns buffer to dns header
 */
static struct dns_hdr *buf2dns(char *buf)
{
    struct dns_hdr *dns = (struct dns_hdr *)buf;
    uint16_t flags = 0, *ptr;

    dns->id = ntohs(dns->id);
    dns->q_count = ntohs(dns->q_count);
    dns->ans_count = ntohs(dns->ans_count);
    dns->auth_count = ntohs(dns->auth_count);
    dns->add_count = ntohs(dns->add_count);
    ptr = ((uint16_t *) dns) + 1;
    flags = ntohs(*ptr);
    dns->qr = (flags & 1 << 15) >> 15;
    dns->opcode = (flags & 0x7800) >> 11;
    dns->aa = (flags & 1 << 10) >> 10;
    dns->tc = (flags & 1 << 9) >> 9;
    dns->rd = (flags & 1 << 8) >> 8;
    dns->ra = (flags & 1 << 7) >> 7;
    dns->z = (flags & 1 << 6) >> 6;
    dns->ad = (flags & 1 << 5) >> 5;
    dns->cd = (flags & 1 << 4) >> 4;
    dns->rcode = flags & 0xF;

    return dns;
}

static struct dns_query *buf2query(char *buf)
{
    struct dns_query *query = (struct dns_query *) buf;
    query->class = ntohs(query->class);
    query->type = ntohs(query->type);

    return query;
}

/**
 * Parses the dns (having len size) answers starting from s
 */
static struct dns_res *dns_parse_response(struct dns_hdr *dns, int len, char *s)
{
    struct dns_res *res;
    struct dns_answer *ans;
    int i;
    uint16_t dlen, *ptr;

    ptr = (uint16_t *) s;
    res = dns_res_new(dns->ans_count);

    for (i = 0; i < dns->ans_count; i++) {
        ans = &res->answers[i];
        ans->name = dns2host_name(dns, len, (char *) ptr++);
        ans->type = ntohs(*ptr++);
        ans->class = ntohs(*ptr++);
        ans->ttl = ntohl(*(uint32_t *)ptr);
        ptr+=2;
        dlen = ntohs(*ptr++);
        switch (ans->type) {
            case DNS_T_A:
                ans->r.ip = ntohl(*(uint32_t *) ptr);
                break;
            case DNS_T_MX:
                ans->prio = ntohs(*ptr++);
                dlen-=2; // dlen include priority. Fall down, to get name
            case DNS_T_NS:
            case DNS_T_CNAME:
            case DNS_T_TXT:
                ans->r.name = dns2host_name(dns, len, (char *)ptr);
                break;
            default:
                net_dbg("Not handled %d type\n", ans->type);
                break;
        }
        res->len++;
        ptr = (uint16_t *)((char *)ptr + dlen);
    }

    return res;
}

static struct dns_res *dns_recv(sock_t *sock, uint16_t id, uint16_t type)
{
    struct dns_hdr *dns;
    struct dns_query *query;
    struct dns_res *res;
    char buf[512], *ptr;
    uint16_t len;

    len = udp_read(sock, buf, sizeof(buf));
    if (len < sizeof(*dns)) {
        net_dbg("dns_query: read too less %d bytes\n", len);
        return NULL;
    }
    dns = buf2dns(buf);
    if (dns->id != id) {
        net_dbg("dns_recv: invalid id %d, expected %d\n", dns->id, id);
        return NULL;
    }
    if (dns->rcode != DNS_RC_OK) {
        net_dbg("dns_recv: server responded with code: %d\n", dns->rcode);
        return NULL;
    }
    if (dns->q_count != 1) {
        net_dbg("dns_recv: expected 1 query count");
        return NULL;
    }
    ptr = (char *)dns->data;
    while (*ptr && ptr < buf + len)
        ptr++;
    if (*ptr++ != 0) {
        net_dbg("dns_recv: unexpected end in parsing response query name\n");
        return NULL;
    }
    query = buf2query(ptr);
    if (query->class != DNS_C_IN) {
        net_dbg("dns_recv: expected IN class query, got %d\n", query->class);
        return NULL;
    }
    if (query->type != type) {
        net_dbg("dns_recv: expected %d type, got %d\n", type, query->type);
        return NULL;
    }
    if (dns->ans_count > 20) {
        net_dbg("dns_recv: too many answers %d\n", dns->ans_count);
        return NULL;
    }
    ptr += sizeof(*query);

    res = dns_parse_response(dns, len, ptr);

    return res;
}

void dns_res_dump(struct dns_res *res) {
    int i;
    char ip[16];

    for (i = 0; i < res->len; i++) {
        struct dns_answer *a = &res->answers[i];
        kprintf("Name: %s, class: IN, type: %s, ttl: %ds, ",
                a->name, dns_types[a->type], a->ttl);
        if (a->type == DNS_T_A)
            kprintf("ip: %s\n", ip2str(a->r.ip, ip, sizeof(ip)));
        else
            kprintf("%s\n", a->r.name);
    }
}

/**
 * Builds and send the query type for host on connected (to DNS server)
 *  socket sock.
 * Transaction is identified by dns_id.
 * Returns number of bytes written or -1 on error
 */
static int dns_send(sock_t *sock, char *host, uint16_t dns_id, int type)
{
    char buf[512], *qname;
    uint16_t left, len, ret;
    struct dns_hdr *dns;
    struct dns_query *query;

    memset(buf, 0, sizeof(buf));
    dns = (struct dns_hdr *) buf;
    dns->id = dns_id;
    dns->qr = 0;
    dns->rd = 1;
    dns->q_count = 1;
    dns2buf(dns);

    left = sizeof(buf) - sizeof(struct dns_hdr);
    qname = (char *) dns->data;
    left = host2dns_name(host, qname, left);
    if (left < sizeof(*query)) {
        net_dbg("dns_query: buf too small\n");
        return -1;
    }
    len = sizeof(buf) - left;
    query = (struct dns_query *) (buf + len);
    query->type = htons(type);
    query->class = htons(DNS_C_IN);
    len += sizeof(*query);
    ret = udp_write(sock, buf, len);

    return ret;
}

struct dns_res *dns_query(char *host, int type)
{
    sock_t *sock;
    struct sock_addr addr;
    struct dns_res *res;
    uint16_t dns_id;

    if (!dns_servers[0]) {
        net_dbg("No DNS defined\n");
        return NULL;
    }
    if (strlen(host) > 255) {
        net_dbg("Hostname %s too long\n");
        return NULL;
    }
    sock = sock_alloc(S_UDP);
    addr.ip = dns_servers[0]; // cycle trough them
    addr.port = DNS_SRV_PORT;
    udp_connect(sock, &addr);

    dns_id = dns_make_id(host);
    if (dns_send(sock, host, dns_id, type) < 0) {
        net_dbg("Could not query %s host\n", host);
        return NULL;
    }

    res = dns_recv(sock, dns_id, type);

    udp_close(sock);

    return res;
}

ip_t dns_hostbyname(char *hostname)
{
    struct dns_res *res;
    ip_t ip;
    int i;

    if (!(res = dns_query(hostname, DNS_T_A))) {
        net_dbg("Could not solve %s\n", hostname);
        return -1;
    }
    /* We can also have a CNAME response */
    for (i = 0; i < res->len; i++) {
        if (res->answers[i].type == DNS_T_A) {
            ip = res->answers[i].r.ip;
            break;
        }
    }
    dns_res_del(res);

    return ip;
}

/**
 * TODO this should initialize the DNS cache.
 * We will not use caching for now...
 */
int dns_init()
{
#if 0
    // Some examples
    struct dns_res *res;
    ip_t ip = dns_hostbyname("www.google.com");
    char buf[16];
    kprintf("www.google.com has IP: %s\n", ip2str(ip, buf, 16));

    kprintf("Dumping MX servers for google.com\n");
    res = dns_query("google.com", DNS_T_MX);
    dns_res_dump(res);
    dns_res_del(res);

    kprintf("Dumping NS servers for google.com\n");
    res = dns_query("google.com", DNS_T_NS);
    dns_res_dump(res);
    dns_res_del(res);
#endif
    return 0;
}

