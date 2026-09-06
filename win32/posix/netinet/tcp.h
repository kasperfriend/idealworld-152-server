/* Windows shim: <netinet/tcp.h> */
#ifndef _WP_NETINET_TCP_H
#define _WP_NETINET_TCP_H
#include "winposix.h"
#ifndef TCP_NODELAY
#define TCP_NODELAY 0x0001
#endif
#ifndef TCP_KEEPALIVE
#define TCP_KEEPALIVE 3
#endif
#ifndef TCP_MAXSEG
#define TCP_MAXSEG 4
#endif
#ifndef SOL_TCP
#define SOL_TCP IPPROTO_TCP
#endif
#endif
