#include <arpa/inet.h>
#include "console.h"
#include "ip.h"
#include "tcp.h"

/**
 * Socket will hold a list of sent packets, in sock->buf_out
 * I need to be able to retransmit a packet.
 *  - after sending a packet, append it to sock->buf_out.
 *      - when appending a packet, check window size and remove old ones 
 *        -> advance window size
 * When receive an ack packet, remove it from sock->buf_out list;
 * Reading data:
 * Packets has real data start initially. 
 * I can use a pointer to move inside the in buffer for read.
 * For write - should i send the packet as fast as possible or wait?
 * Should i use a circular buffer where to write and wait for 1460 and send?
 * a socket priv union can be done - to store some kind of connection infos.
 * in udp we don't have connection. connection status / socket status.
 * remove the status from socket. in udp we don't use connection status.
 */

/* Connection info */
struct tcp_conn
{
    struct {
    } snd;
    struct {
    } rcv;
};

static struct tcp_hdr *buf2tcp(void *buf)
{
    uint16_t flags;
    struct tcp_hdr *tcp = buf;

    tcp->sport = ntohs(tcp->sport);
    tcp->dport = ntohs(tcp->dport);
    tcp->seq_no = ntohl(tcp->seq_no);
    tcp->ack_no = ntohl(tcp->ack_no);
    flags = ntohs(*((uint16_t *)tcp + 6));
    tcp->hdrl = (flags & 0xF000) >> 12;
    tcp->ns = (flags & (1 << 8)) >> 8;
    tcp->cwr = (flags & (1 << 7)) >> 7;
    tcp->ece = (flags & (1 << 6)) >> 6;
    tcp->urg = (flags & (1 << 5)) >> 5;
    tcp->ack = (flags & (1 << 4)) >> 4;
    tcp->psh = (flags & (1 << 3)) >> 3;
    tcp->rst = (flags & (1 << 2)) >> 2;
    tcp->syn = (flags & (1 << 1)) >> 1;
    tcp->fin = (flags & 1);
    tcp->win_size = ntohs(tcp->win_size);
    tcp->csum = ntohs(tcp->csum);
    tcp->urg_ptr = ntohs(tcp->urg_ptr);

    return tcp;
}

static void *tcp2buf(struct tcp_hdr *tcp)
{
    uint16_t *ptr, flags;

    tcp->sport = htons(tcp->sport);
    tcp->dport = htons(tcp->dport);
    tcp->seq_no = htonl(tcp->seq_no);
    tcp->ack_no = htonl(tcp->ack_no);
    flags = (tcp->hdrl << 12) | (tcp->ns << 8) | (tcp->cwr << 7) | 
            (tcp->ece << 6) | (tcp->urg << 5) | (tcp->ack << 4) |
            (tcp->psh << 3) | (tcp->rst << 2) | (tcp->syn << 1) | (tcp->fin);
    ptr = (uint16_t *) tcp + 6;
    *ptr = flags;
    tcp->win_size = htons(tcp->win_size);
    tcp->csum = htons(tcp->csum);
    tcp->urg_ptr = htons(tcp->urg_ptr);

    return tcp;
}

/**
 * Computes TCP csum. 
 * Expects TCP to be in network format and iph in host format
 */
uint16_t tcp_csum(struct tcp_hdr *tcp, ipv4_hdr_t *iph)
{
    uint32_t sum = 0, ip;
    uint16_t *ptr, i;
    uint16_t len = iph->len - sizeof(*iph);

    /* Computes 'pseudo' header sum */
    ip = htonl(iph->saddr);
    ptr = (uint16_t *) &ip;
    sum += ptr[0] + ptr[1];

    ip = htonl(iph->daddr);
    ptr = (uint16_t *) &ip;
    sum += ptr[0] + ptr[1];

    sum += htons(iph->proto);
    sum += htons(len);

    /* TCP data */
    ptr = (uint16_t *) tcp;
    for (i = 0; i < len / 2; i++)
        sum += *ptr++;
    if (len % 2)
        sum += *(uint8_t *) ptr;
    sum = (sum & 0xFFFF) + (sum >> 16);

    return ~sum;
}

/**
 * Return where the data starts in the tcp packet
 */
void *tcp_data(struct tcp_hdr *tcp)
{
    return (uint32_t *)tcp + tcp->hdrl;
}

/**
 * Dumps the tcp header
 */
static void tcp_hdr_dump(struct tcp_hdr *tcp, ipv4_hdr_t *iph)
{
    kprintf("dport: %d, sport: %d, seq_no: %x ack_no: %x, hdr_len: %d\n",
            tcp->dport, tcp->sport, tcp->seq_no, tcp->ack_no, tcp->hdrl << 2);
    kprintf("win_size: %x, csum: %x, urg_ptr: %x\n",
            tcp->win_size, tcp->csum, tcp->urg_ptr);
    kprintf("len: %d bytes\n", iph->len - sizeof(*iph));
    kprintf("flags: %s%s%s%s%s%s%s%s%s\n",
            tcp->ns  ? "NS  " : "", 
            tcp->cwr ? "CWR " : "",
            tcp->ece ? "ECE " : "",
            tcp->urg ? "URG " : "",
            tcp->ack ? "ACK " : "",
            tcp->psh ? "PSH " : "",
            tcp->rst ? "RST " : "",
            tcp->syn ? "SYN " : "",
            tcp->fin ? "FIN " : "");
}

/**
 * Like net_dbg, but prints local and remote ip:port
 */
static void tcp_dbg(ipv4_hdr_t *iph, struct tcp_hdr *tcp, char *str)
{
    char ipl[16], ipr[16];

    ip2str(iph->daddr, ipl, sizeof(ipl));
    ip2str(iph->saddr, ipr, sizeof(ipr));
    net_dbg("TCP: drop local %s:%d remote %s:%d. %s!\n",
            ipl, tcp->dport, ipr, tcp->sport, str);
}

int tcp_parse_opts(uint8_t *buf, uint32_t len)
{
    net_dbg("tcp_parse_opts\n");
}

/**
 * Process incoming TCP packet
 */
int tcp_process(struct net_buf *buf)
{
    eth_hdr_t *eth = buf->data;
    ipv4_hdr_t *iph = (ipv4_hdr_t *) eth->data;
    struct tcp_hdr *tcp = ip_data(iph);
    uint16_t csum = tcp_csum(tcp, iph);
    sock_t *sock;

    if ((csum = tcp_csum(tcp, iph)) != 0) {
        net_dbg("TCP: bad csum %x! %x\n", csum, tcp->csum);
        return -1;
    }
    buf2tcp(tcp);
    if (!(sock = sock_find(S_TCP, iph->daddr, tcp->dport, 0, 0))) {
        tcp_dbg(iph, tcp, "No socket match!");
        return -1;
    }
    tcp_hdr_dump(tcp, iph);

    return 0;
}

int tcp_connect(sock_t *sock, sock_addr_t *addr)
{
    net_dbg("tcp_connect");
}

int tcp_read(sock_t *sock, void *buffer, unsigned int length)
{
    net_dbg("tcp_read\n");
}

int tcp_write(sock_t *sock, void *buffer, unsigned int length)
{
    net_dbg("tcp_write\n");
}

int tcp_close(sock_t *sock)
{
    net_dbg("closing\n");
}

int tcp_init()
{
    net_dbg("tcp init\n");
    // testing //
    sock_t *sock = sock_alloc(S_TCP);
    sock_addr_t addr;

    addr.ip = ADDR_ANY;
    addr.port = 5000;
    sock_bind(sock, &addr);

    tcp_close(sock);

    return 0;
}

