/* Windows shim: <netinet/in.h> */
#ifndef _WP_NETINET_IN_H
#define _WP_NETINET_IN_H
#include "winposix.h"
#ifndef IPPROTO_TCP
#define IPPROTO_TCP 6
#endif
#ifndef INADDR_ANY
#define INADDR_ANY (unsigned long)0x00000000
#endif
#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK 0x7f000001
#endif
#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif
#ifndef INADDR_BROADCAST
#define INADDR_BROADCAST 0xffffffff
#endif
#ifndef INADDR_MAX_LOCAL_GROUP
#define INADDR_MAX_LOCAL_GROUP 0xe00000ff
#endif
#endif
