/* Windows shim: <sys/poll.h> over WSAPoll
 *
 * winposix.h pulls in <winsock2.h> with _WIN32_WINNT >= 0x0600, which already
 * declares `struct pollfd`/WSAPOLLFD plus the POLL* event bits.  This shim
 * therefore only adds the POSIX name `poll()` (implemented in winposix.cpp)
 * and whatever the toolchain does not define, without re-declaring pollfd.
 */
#ifndef _WP_SYS_POLL_H
#define _WP_SYS_POLL_H

#include "winposix.h"
/* NOTE: there is no system <poll.h> on Windows (neither MSVC nor mingw
 * ship one); struct pollfd/WSAPOLLFD and the POLL* bits come from
 * <winsock2.h> via winposix.h.  Only nfds_t is missing and provided here. */
#ifndef WP_NFDS_T_DEFINED
#define WP_NFDS_T_DEFINED
typedef unsigned int nfds_t;
#endif

#ifndef POLLIN
#define POLLIN   0x0300
#endif
#ifndef POLLPRI
#define POLLPRI  0x0400
#endif
#ifndef POLLOUT
#define POLLOUT  0x0010
#endif
#ifndef POLLERR
#define POLLERR  0x0001
#endif
#ifndef POLLHUP
#define POLLHUP  0x0002
#endif
#ifndef POLLNVAL
#define POLLNVAL 0x0004
#endif
#ifndef POLLRDNORM
#define POLLRDNORM 0x0100
#endif
#ifndef POLLRDBAND
#define POLLRDBAND 0x0200
#endif
#ifndef POLLWRNORM
#define POLLWRNORM 0x0010
#endif
#ifndef POLLWRBAND
#define POLLWRBAND 0x0020
#endif

#ifndef INFTIM
#define INFTIM (-1)
#endif

#ifdef __cplusplus
extern "C" {
#endif
#ifndef WP_HAS_POLL_DECL
#define WP_HAS_POLL_DECL
int poll(struct pollfd *fds, nfds_t nfds, int timeout);
#endif
#ifdef __cplusplus
}
#endif

#endif /* _WP_SYS_POLL_H */
