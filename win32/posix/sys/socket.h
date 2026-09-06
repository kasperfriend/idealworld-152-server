/* Windows shim: <sys/socket.h> over Winsock (also brings in the CRT
 * read/write/close/dup declarations this code expects from unistd/io.h). */
#ifndef _WP_SYS_SOCKET_H
#define _WP_SYS_SOCKET_H
#include "winposix.h"
#include <io.h>
#include <fcntl.h>
#ifndef SOCKET_ERROR
#define SOCKET_ERROR (-1)
#endif
#ifndef SOMAXCONN
#define SOMAXCONN 0x7fffffff
#endif
#endif
