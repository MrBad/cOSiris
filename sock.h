#ifndef _SOCK_H
#define _SOCK_H

#define ADDR_ANY 0

#define S_UDP 0
#define S_TCP 1
#define SOCK_MAX_BUFS 4 /* Max bufs to keep in sock before start dropping */

typedef struct sock_addr {
    uint32_t ip;
    uint16_t port;
} sock_addr_t;

typedef enum {
    SOCK_CREATED,
    SOCK_CONNECTED,
} sock_state_t;

typedef struct sock {
    uint8_t proto;
    sock_addr_t loc_addr;   // Local address {ip, port}
    sock_addr_t rem_addr;   // Remote address {ip, port}
    pid_t pid;              // id of the process this sock belongs to
    node_t *node;           // ptr to stab entry
    sock_state_t state;     // state of the sock
    spin_lock_t lock;       // locking
    list_t *bufs;           // list of buffered packets or something
} sock_t;

struct socks_table {
    spin_lock_t lock;
    list_t *socks;
};

extern struct socks_table *stab;
/**
 * Creates a new sock and adds it to sock table
 */
sock_t *sock_alloc(uint8_t proto);

/**
 * Remove the socket from list and frees it
 */
void sock_free(sock_t *sock);

/**
 * Set local address. Use it in bind.
 */
int sock_set_loc_addr(sock_t *sock, sock_addr_t *addr);

/**
 * Sets remote address. Use it in connect.
 */
int sock_set_rem_addr(sock_t *sock, sock_addr_t *addr);

/**
 * Initialize sock table
 */
int sock_init();

/**
 * Allocates a free port for protocol
 */
int port_alloc(int proto);

/**
 * Release the port
 */
void port_free(int proto, int port);

int sock_bind(sock_t *sock, sock_addr_t *addr);

void sock_table_dump();

#endif

