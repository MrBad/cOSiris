#ifndef _ARPA_INET_H
#define _ARPA_INET_H

#include <sys/types.h>

uint16_t ntohs(uint16_t netshort);
uint16_t htons(uint16_t hostshort);
uint32_t ntohl(uint32_t netlong);
uint32_t htonl(uint32_t hostlong);

#endif

