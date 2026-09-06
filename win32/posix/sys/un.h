/* Windows shim: <sys/un.h> (Winsock AF_UNIX, Windows 10 1803+) */
#ifndef _WP_SYS_UN_H
#define _WP_SYS_UN_H
#include "winposix.h"
#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif
#endif
