/* Windows shim: <netdb.h> */
#ifndef _WP_NETDB_H
#define _WP_NETDB_H
#include "winposix.h"
#ifndef NI_MAXHOST
#define NI_MAXHOST 1025
#endif
#ifndef NI_MAXSERV
#define NI_MAXSERV 32
#endif
#ifndef NI_NUMERICHOST
#define NI_NUMERICHOST 1
#endif
#ifndef NI_NUMERICSERV
#define NI_NUMERICSERV 2
#endif
#ifndef NI_NAMEREQD
#define NI_NAMEREQD 4
#endif
#ifndef NI_DGRAM
#define NI_DGRAM 16
#endif
#ifndef AI_PASSIVE
#define AI_PASSIVE 1
#endif
#ifndef AI_CANONNAME
#define AI_CANONNAME 2
#endif
#ifndef AI_NUMERICHOST
#define AI_NUMERICHOST 4
#endif
#ifndef AI_NUMERICSERV
#define AI_NUMERICSERV 8
#endif
#ifndef AI_ADDRCONFIG
#define AI_ADDRCONFIG 0x400
#endif
#ifndef EAI_NONAME
#define EAI_NONAME 200
#endif
#ifndef EAI_AGAIN
#define EAI_AGAIN 201
#endif
#ifndef EAI_FAIL
#define EAI_FAIL 202
#endif
#ifndef EAI_MEMORY
#define EAI_MEMORY 203
#endif
#endif
