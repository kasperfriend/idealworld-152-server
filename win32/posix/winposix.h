/*
 * winposix.h - shared private header of the Windows POSIX shim layer.
 *
 * The shim makes the Berkeley-socket + POSIX code of this server compile on
 * Windows without touching every call site:
 *
 *   - network sockets are handed out as "pseudo fds" (>= WSOCK_FD_BASE)
 *     mapping to real Winsock SOCKET handles;
 *   - read/write/close/dup/ioctl/fcntl/poll dispatch on the fd: pseudo fds go
 *     to Winsock, everything else falls through to the CRT;
 *   - a few extra POSIX services are emulated (pipe, setitimer, usleep,
 *     syslog, iconv subset, signal layer, ...) in winposix.cpp & friends.
 *
 * Functions whose declarations are already provided by mingw headers
 * (read/write/close/dup via <io.h>, winsock APIs via <winsock2.h>) are not
 * re-declared here to avoid conflicting prototypes.
 */
#ifndef _WINPOSIX_H_
#define _WINPOSIX_H_

/* Pick the modern Windows API surface (AF_UNIX needs 0x0A00). */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef WINVER
#define WINVER 0x0A00
#endif
#ifndef NTDDI_VERSION
#define NTDDI_VERSION 0x0A000000
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
/* NOTE: no <afunix.h> here - older mingw sysroots lack it.  AF_UNIX and
 * struct sockaddr_un come from our own <sys/un.h> instead. */
/* The game server has its own `struct MSG` (cgame/common/message.h); keep
 * winuser.h from claiming that name for the Win32 message struct.  The
 * PMSG/LPMSG typedefs are unaffected, and nothing here uses the bare
 * Win32 MSG type. */
#define MSG WP_WinMSG
#include <windows.h>
#undef MSG
/* autoteamman.h has an enum member called WAIT_TIMEOUT (600); the Win32
 * wait-status macro would rewrite it to 258.  Nothing in this tree uses the
 * Win32 meaning, so drop the macro right after the headers are done. */
#undef WAIT_TIMEOUT
#include <io.h>
#include <direct.h>
#include <process.h>
#include <errno.h>
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <sys/types.h>

/* NOTE: no struct timezone here - current mingw-w64 <time.h> already defines
 * it, and a second definition breaks every translation unit. */

#ifdef __cplusplus
extern "C" {
#endif

/* ---- extra errno values ------------------------------------------------ */
#ifndef EWOULDBLOCK
#define EWOULDBLOCK EAGAIN
#endif
#ifndef EPIPE
#define EPIPE 32
#endif
#ifndef ENOTSOCK
#define ENOTSOCK 88
#endif
#ifndef EINPROGRESS
#define EINPROGRESS 112
#endif
#ifndef EALREADY
#define EALREADY 114
#endif
#ifndef ECONNREFUSED
#define ECONNREFUSED 111
#endif
#ifndef EDESTADDRREQ
#define EDESTADDRREQ 89
#endif
#ifndef EPROTOTYPE
#define EPROTOTYPE 91
#endif
#ifndef ENOBUFS
#define ENOBUFS 105
#endif
#ifndef EISCONN
#define EISCONN 106
#endif
#ifndef ENOTCONN
#define ENOTCONN 107
#endif
#ifndef ETIMEDOUT
#define ETIMEDOUT 110
#endif
#ifndef EOPNOTSUPP
#define EOPNOTSUPP 95
#endif
#ifndef EAFNOSUPPORT
#define EAFNOSUPPORT 97
#endif
#ifndef EADDRINUSE
#define EADDRINUSE 98
#endif
#ifndef EADDRNOTAVAIL
#define EADDRNOTAVAIL 99
#endif
#ifndef ENETUNREACH
#define ENETUNREACH 101
#endif
#ifndef EHOSTUNREACH
#define EHOSTUNREACH 113
#endif
#ifndef ECONNABORTED
#define ECONNABORTED 103
#endif
#ifndef ECONNRESET
#define ECONNRESET 104
#endif
#ifndef EMSGSIZE
#define EMSGSIZE 90
#endif
#ifndef EPROTONOSUPPORT
#define EPROTONOSUPPORT 93
#endif
#ifndef ESOCKTNOSUPPORT
#define ESOCKTNOSUPPORT 94
#endif
#ifndef ENOPROTOOPT
#define ENOPROTOOPT 92
#endif
#ifndef ENOTTY
#define ENOTTY 25
#endif
#ifndef ENOMSG
#define ENOMSG 42
#endif
#ifndef EILSEQ
#define EILSEQ 84
#endif
#ifndef ENOSYS
#define ENOSYS 38
#endif

/* ---- ssize_t / socklen_t ----------------------------------------------- */
#ifndef _SSIZE_T_DEFINED
typedef intptr_t ssize_t;
#define _SSIZE_T_DEFINED
#endif
#ifndef _SOCKLEN_T
typedef int socklen_t;
#define _SOCKLEN_T
#endif

/* ---- BSD-style aliases used by the game headers ------------------------ */
#ifndef __WP_BSD_TYPES_DEFINED
#define __WP_BSD_TYPES_DEFINED
#ifndef _CADDR_T_DEFINED
typedef char * caddr_t;
#define _CADDR_T_DEFINED
#endif
#ifndef _SSIZE_T_DEFINED2
typedef unsigned char   u_char;
typedef unsigned short  u_short;
typedef unsigned int    u_int;
typedef unsigned long   u_long;
#endif
typedef uint8_t  u_int8_t;
typedef uint16_t u_int16_t;
typedef uint32_t u_int32_t;
typedef uint64_t u_int64_t;
#endif

/* ---- extra signal numbers / helpers ------------------------------------ */
#ifndef SIGHUP
#define SIGHUP 1
#endif
#ifndef SIGQUIT
#define SIGQUIT 3
#endif
#ifndef SIGKILL
#define SIGKILL 9
#endif
#ifndef SIGPIPE
#define SIGPIPE 13
#endif
#ifndef SIGALRM
#define SIGALRM 14
#endif
#ifndef SIGCHLD
#define SIGCHLD 17
#endif
#ifndef SIGUSR1
#define SIGUSR1 10
#endif
#ifndef SIGUSR2
#define SIGUSR2 12
#endif
#ifndef SIGBUS
#define SIGBUS 7
#endif
#ifndef SIGSTOP
#define SIGSTOP 23
#endif
#ifndef SIGCONT
#define SIGCONT 25
#endif
#ifndef SIGSYS
#define SIGSYS 31
#endif
#ifndef SIGURG
#define SIGURG 23
#endif

#ifndef SA_RESTART
#define SA_RESTART 0
#endif
#ifndef SA_NOCLDSTOP
#define SA_NOCLDSTOP 0
#endif

#ifndef SIG_BLOCK
#define SIG_BLOCK 0
#endif
#ifndef SIG_UNBLOCK
#define SIG_UNBLOCK 1
#endif
#ifndef SIG_SETMASK
#define SIG_SETMASK 2
#endif

/* ---- POSIX fcntl constants missing from MSVCRT ------------------------- */
#ifndef F_DUPFD
#define F_DUPFD 0
#endif
#ifndef F_GETFD
#define F_GETFD 1
#endif
#ifndef F_SETFD
#define F_SETFD 2
#endif
#ifndef F_GETFL
#define F_GETFL 3
#endif
#ifndef F_SETFL
#define F_SETFL 4
#endif
#ifndef FD_CLOEXEC
#define FD_CLOEXEC 1
#endif
#ifndef O_NONBLOCK
#define O_NONBLOCK 0x1000
#endif
#ifndef O_SYNC
#define O_SYNC 0x2000
#endif
#ifndef O_DSYNC
#define O_DSYNC O_SYNC
#endif
#ifndef O_RSYNC
#define O_RSYNC O_SYNC
#endif
#ifndef O_DIRECTORY
#define O_DIRECTORY 0x4000
#endif
#ifndef O_NOCTTY
#define O_NOCTTY 0
#endif
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

/* ---- ioctl() constants -------------------------------------------------- */
#ifndef FIONREAD
#define FIONREAD 0x4004667F
#endif
#ifndef FIONBIO
#define FIONBIO 0x8004667E
#endif
#ifndef FIONWRITE
#define FIONWRITE 0x40046677
#endif

/* ---- MSG_* extras ------------------------------------------------------- */
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
#ifndef MSG_DONTWAIT
#define MSG_DONTWAIT 0
#endif

/* ---- poll() event bits (Winsock WSAPoll values) ------------------------- */
#ifndef POLLIN
#define POLLIN   0x0300
#define POLLPRI  0x0400
#define POLLOUT  0x0010
#define POLLERR  0x0001
#define POLLHUP  0x0002
#define POLLNVAL 0x0004
#define POLLRDNORM 0x0100
#define POLLRDBAND 0x0200
#define POLLWRNORM 0x0010
#define POLLWRBAND 0x0020
#endif

#ifndef SHUT_RD
#define SHUT_RD 0
#define SHUT_WR 1
#define SHUT_RDWR 2
#endif

/* Windows maps this via sockaddr_in6; keep name visible */
#ifndef socklen_t
#endif

/* ---- extra fcntl open() flags (MSVCRT lacks the Linux extras) ------------ */
#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif
#ifndef FD_CLOEXEC
#define FD_CLOEXEC 1
#endif
#ifndef O_NOCTTY
#define O_NOCTTY 0
#endif

/* ---- 64-bit file offsets ------------------------------------------------- */
/* NOTE: no lseek/getuid macros here on purpose.  A function-style macro would
 * also rewrite mingw's own later declarations (e.g. <unistd.h> pulled in by
 * game files), producing conflicting prototypes.  The two live lseek call
 * sites (tranlog.h) call _lseeki64() directly on Windows, and the single
 * getuid check (worldmanager.cpp) is compiled out. */

/* ---- functions implemented by winposix.cpp ------------------------------ */
int         ioctl(int fd, unsigned long request, ...);
int         fcntl(int fd, int cmd, ...);
int         pipe(int fds[2]);
struct tm * localtime_r(const time_t *t, struct tm *buf);
struct tm * gmtime_r(const time_t *t, struct tm *buf);
char      * ctime_r(const time_t *t, char *buf);
int         gettimeofday(struct timeval *tv, void *tz);
int         clock_gettime(int clk_id, struct timespec *tp);
ssize_t     pread(int fd, void *buf, size_t len, long long off);
ssize_t     pwrite(int fd, const void *buf, size_t len, long long off);
/* NOTE: this prototype is spelled (int, off_t) - identical to mingw
 * <unistd.h>'s - because storage TUs include both headers and two C
 * declarations may only share a name when they share a signature.
 * off_t is 64-bit in every TU that needs it (the WDB steps build with
 * _FILE_OFFSET_BITS=64); the definition takes long long so it also
 * serves 32-bit callers (zero-extended) correctly. */
int         ftruncate(int fd, off_t len);
void        setlinebuf(FILE *f);
ssize_t     readv(int fd, const struct iovec *iov, int iovcnt);
ssize_t     writev(int fd, const struct iovec *iov, int iovcnt);
int         fsync(int fd);
int         scandir(const char *dir, struct dirent ***namelist,
                    int (*filter)(const struct dirent *),
                    int (*compar)(const struct dirent **,
                                  const struct dirent **));
int         alphasort(const struct dirent **a, const struct dirent **b);
long        sysconf(int name);

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#endif
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

/* private helpers used by the patched engine loop (thread.cpp) */
HANDLE      wp_signal_event(void);
unsigned long wp_signal_take(void);
void        wp_map_wsaerr(void);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
/* ---- POSIX-prototype wrappers over the Winsock interposers -------------- */
/* The game code was written against the Linux <sys/socket.h> prototypes
 * (void* buffers, int fds).  Winsock declares the same C symbols with
 * char buffers and SOCKET handles; winposix.cpp defines the
 * interposing implementations.  These inline wrappers give the game code a
 * Linux-compatible surface, and the macros below route every call through
 * them.  winposix.cpp #undefs the macros after including this header so it
 * can define the real interposers. */
inline int wp_send(int fd, const void *buf, int len, int flags)
{
	return ::send((SOCKET)(intptr_t)fd, (const char *)buf, len, flags);
}
inline int wp_recv(int fd, void *buf, int len, int flags)
{
	return ::recv((SOCKET)(intptr_t)fd, (char *)buf, len, flags);
}
inline int wp_sendto(int fd, const void *buf, int len, int flags,
                     const struct sockaddr *to, int tolen)
{
	return ::sendto((SOCKET)(intptr_t)fd, (const char *)buf, len, flags,
	                to, tolen);
}
inline int wp_recvfrom(int fd, void *buf, int len, int flags,
                       struct sockaddr *from, int *fromlen)
{
	return ::recvfrom((SOCKET)(intptr_t)fd, (char *)buf, len, flags,
	                  from, fromlen);
}
inline int wp_setsockopt(int fd, int level, int optname,
                         const void *optval, int optlen)
{
	return ::setsockopt((SOCKET)(intptr_t)fd, level, optname,
	                    (const char *)optval, optlen);
}
inline int wp_getsockopt(int fd, int level, int optname,
                         void *optval, int *optlen)
{
	return ::getsockopt((SOCKET)(intptr_t)fd, level, optname,
	                    (char *)optval, optlen);
}

#define send         wp_send
#define recv         wp_recv
#define sendto       wp_sendto
#define recvfrom     wp_recvfrom
#define setsockopt   wp_setsockopt
#define getsockopt   wp_getsockopt

/* ---- C++-linkage overloads (must NOT be extern "C") ---------------------- */
/* MSVCRT already exports 1-argument mkdir(); the two-argument POSIX form used
 * by db.h/accessdb.cpp/storagewdb.h overloads it.  C++ linkage keeps the
 * overload legal (two extern "C" functions may not share a name).  ftruncate
 * used to live here too, but storage TUs include <unistd.h> after this
 * header, and a C declaration may not follow a C++ overload - so ftruncate
 * is declared in the extern "C" block above with mingw's exact signature
 * and our definition interposes at link time like the socket shims. */
int mkdir(const char *path, int mode);

/* inet_aton() is missing from Winsock; gdeliveryd parses listener addresses
 * with it.  Full dotted quads go through InetPton, short forms fall back to
 * inet_addr, matching the glibc acceptance. */
inline int wp_inet_aton(const char *cp, struct in_addr *inp)
{
	if (cp && inp)
	{
		if (InetPtonA(AF_INET, cp, inp) == 1)
			return 1;
		{
			unsigned long a = inet_addr(cp);
			if (a != INADDR_NONE)
			{
				inp->s_addr = a;
				return 1;
			}
		}
	}
	return 0;
}
#define inet_aton wp_inet_aton
#endif

#endif /* _WINPOSIX_H_ */
