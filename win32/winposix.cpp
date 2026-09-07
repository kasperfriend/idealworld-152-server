/*
 * winposix.cpp - Windows implementation of the POSIX/Winsock surface the
 * idealworld-152 server needs (see winposix.h).
 *
 * Sockets are returned as pseudo file descriptors >= WSOCK_FD_BASE so generic
 * code can keep passing them to read()/write()/close()/select()/poll().
 * Regular CRT descriptors (files, pipes, console) keep their normal small
 * integer values and fall through to the CRT.
 *
 * Compiled once into winposix.a and linked into every daemon.
 */
#include "winposix.h"

/* winposix.h routes POSIX-style send/recv/sendto/... calls to wp_* wrappers;
 * this file implements the underlying C symbols, so drop the aliases here. */
#undef send
#undef recv
#undef sendto
#undef recvfrom
#undef setsockopt
#undef getsockopt

#include <signal.h>
#include <fcntl.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <sys/poll.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <map>

#ifndef NSIG
#define NSIG 32
#endif
#ifndef SIGBREAK
#define SIGBREAK 21
#endif
#ifndef SIGTERM
#define SIGTERM 15
#endif
#ifndef SIGINT
#define SIGINT 2
#endif
#ifndef SIGABRT
#define SIGABRT 22
#endif
#ifndef SIGFPE
#define SIGFPE 8
#endif
#ifndef SIGSEGV
#define SIGSEGV 11
#endif
#ifndef SIGILL
#define SIGILL 4
#endif


/* MSVCRT's errno.h only defines a subset; the game code uses the Linux
 * values, so we map our Winsock results to those constants. */
#ifndef EBADF
#define EBADF 9
#endif
#ifndef EINTR
#define EINTR 4
#endif
#ifndef EAGAIN
#define EAGAIN 11
#endif
#ifndef EINVAL
#define EINVAL 22
#endif
#ifndef EMFILE
#define EMFILE 24
#endif
#ifndef EACCES
#define EACCES 13
#endif
#ifndef EFAULT
#define EFAULT 14
#endif
#ifndef ENOMEM
#define ENOMEM 12
#endif
#ifndef ENOENT
#define ENOENT 2
#endif
#ifndef EIO
#define EIO 5
#endif
#ifndef ESPIPE
#define ESPIPE 29
#endif
#ifndef EBUSY
#define EBUSY 16
#endif
#ifndef EEXIST
#define EEXIST 17
#endif
#ifndef ENOTDIR
#define ENOTDIR 20
#endif
#ifndef EISDIR
#define EISDIR 21
#endif
#ifndef EMLINK
#define EMLINK 31
#endif
#ifndef ERANGE
#define ERANGE 34
#endif
#ifndef EDEADLK
#define EDEADLK 35
#endif
#ifndef ENAMETOOLONG
#define ENAMETOOLONG 36
#endif
#ifndef ENOLCK
#define ENOLCK 37
#endif
#ifndef ENOTEMPTY
#define ENOTEMPTY 39
#endif
#ifndef ELOOP
#define ELOOP 40
#endif

#ifndef SIG_ERR
#define SIG_ERR ((void (*)(int))-1)
#endif

static void wsa_init(void);

/* Map a Winsock error code to a Linux-flavoured errno value. */
static int wp_wsa_to_errno(int wsa)
{
	switch (wsa)
	{
	case WSAEINTR:           return EINTR;
	case WSAEBADF:           return EBADF;
	case WSAEACCES:          return EACCES;
	case WSAEFAULT:          return EFAULT;
	case WSAEINVAL:          return EINVAL;
	case WSAEMFILE:          return EMFILE;
	case WSAEWOULDBLOCK:     return EAGAIN;
	case WSAEINPROGRESS:     return EINPROGRESS;
	case WSAEALREADY:        return EALREADY;
	case WSAENOTSOCK:        return ENOTSOCK;
	case WSAEDESTADDRREQ:    return EDESTADDRREQ;
	case WSAEMSGSIZE:        return EMSGSIZE;
	case WSAEPROTOTYPE:      return EPROTOTYPE;
	case WSAENOPROTOOPT:     return ENOPROTOOPT;
	case WSAEPROTONOSUPPORT: return EPROTONOSUPPORT;
	case WSAESOCKTNOSUPPORT: return ESOCKTNOSUPPORT;
	case WSAEOPNOTSUPP:      return EOPNOTSUPP;
	case WSAEPFNOSUPPORT:    return EAFNOSUPPORT;
	case WSAEAFNOSUPPORT:    return EAFNOSUPPORT;
	case WSAEADDRINUSE:      return EADDRINUSE;
	case WSAEADDRNOTAVAIL:   return EADDRNOTAVAIL;
	case WSAENETDOWN:        return EIO;
	case WSAENETUNREACH:     return ENETUNREACH;
	case WSAENETRESET:       return ECONNRESET;
	case WSAECONNABORTED:    return ECONNABORTED;
	case WSAECONNRESET:      return ECONNRESET;
	case WSAENOBUFS:         return ENOBUFS;
	case WSAEISCONN:         return EISCONN;
	case WSAENOTCONN:        return ENOTCONN;
	case WSAESHUTDOWN:       return EINVAL;
	case WSAETIMEDOUT:       return ETIMEDOUT;
	case WSAECONNREFUSED:    return ECONNREFUSED;
	case WSAEHOSTDOWN:       return EHOSTUNREACH;
	case WSAEHOSTUNREACH:    return EHOSTUNREACH;
	case WSAEPROCLIM:        return EMFILE;
	case WSAEUSERS:          return EACCES;
	case WSAEDQUOT:          return EACCES;
	default:                 return EIO;
	}
}


/* ---- pseudo fd table ----------------------------------------------------- */
#define WSOCK_FD_BASE 0x10000000

namespace
{
	struct FdTable
	{
		CRITICAL_SECTION cs;
		std::map<int, SOCKET> sock;
		int next;
		FdTable()
		{
			InitializeCriticalSection(&cs);
			next = 1;
		}
		~FdTable() { DeleteCriticalSection(&cs); }
		void lock()   { EnterCriticalSection(&cs); }
		void unlock() { LeaveCriticalSection(&cs); }

		bool is_sock(int fd)
		{
			bool r;
			lock();
			r = (sock.find(fd) != sock.end());
			unlock();
			return r;
		}
		int add(SOCKET s)
		{
			lock();
			int fd = WSOCK_FD_BASE + (next++);
			if (next > 0x4000000) next = 1;
			sock[fd] = s;
			unlock();
			return fd;
		}
		SOCKET get(int fd)
		{
			SOCKET s = INVALID_SOCKET;
			lock();
			std::map<int, SOCKET>::iterator it = sock.find(fd);
			if (it != sock.end())
				s = it->second;
			unlock();
			return s;
		}
		void remove(int fd)
		{
			lock();
			sock.erase(fd);
			unlock();
		}
		int fd_of(SOCKET s)
		{
			int found = -1;
			lock();
			for (std::map<int, SOCKET>::iterator it = sock.begin();
			     it != sock.end(); ++it)
			{
				if (it->second == s)
				{
					found = it->first;
					break;
				}
			}
			unlock();
			return found;
		}
	};
	FdTable g_fds;

	struct WsaInitOnce
	{
		WsaInitOnce() { wsa_init(); }
	} g_wsa_once;

	bool g_is_pseudo(int fd)
	{
		return fd >= WSOCK_FD_BASE && g_fds.is_sock(fd);
	}
}


/* direct pointers to the real ws2_32 functions (our definitions shadow the
 * import-lib symbols, so calls inside this file must go through these) */
namespace
{
	struct Ws
	{
		SOCKET (WINAPI *socket_)(int, int, int);
		int     (WINAPI *bind_)(SOCKET, const struct sockaddr *, int);
		int     (WINAPI *listen_)(SOCKET, int);
		SOCKET  (WINAPI *accept_)(SOCKET, struct sockaddr *, int *);
		int     (WINAPI *connect_)(SOCKET, const struct sockaddr *, int);
		int     (WINAPI *send_)(SOCKET, const char *, int, int);
		int     (WINAPI *recv_)(SOCKET, char *, int, int);
		int     (WINAPI *sendto_)(SOCKET, const char *, int, int,
		                          const struct sockaddr *, int);
		int     (WINAPI *recvfrom_)(SOCKET, char *, int, int,
		                            struct sockaddr *, int *);
		int     (WINAPI *shutdown_)(SOCKET, int);
		int     (WINAPI *setsockopt_)(SOCKET, int, int, const char *, int);
		int     (WINAPI *getsockopt_)(SOCKET, int, int, char *, int *);
		int     (WINAPI *getsockname_)(SOCKET, struct sockaddr *, int *);
		int     (WINAPI *getpeername_)(SOCKET, struct sockaddr *, int *);
		int     (WINAPI *closesocket_)(SOCKET);
		int     (WINAPI *select_)(int, fd_set *, fd_set *, fd_set *,
		                          const struct timeval *);
		int     (WINAPI *gethostname_)(char *, int);
		int     (WINAPI *ioctlsocket_)(SOCKET, long, u_long *);
		int     (WINAPI *WSAGetLastError_)(void);
		int     (WINAPI *WSAStartup_)(WORD, LPWSADATA);
		int     (WINAPI *WSADuplicateSocketW_)(SOCKET, DWORD, LPWSAPROTOCOL_INFOW);
		SOCKET  (WINAPI *WSASocketW_)(int, int, int, LPWSAPROTOCOL_INFOW, DWORD, DWORD);
		u_short (WINAPI *htons_)(u_short);
		u_short (WINAPI *ntohs_)(u_short);
		u_long  (WINAPI *htonl_)(u_long);
		u_long  (WINAPI *ntohl_)(u_long);
		unsigned long (WINAPI *inet_addr_)(const char *);
		char *(WINAPI *inet_ntoa_)(struct in_addr);
		int     (WINAPI *__WSAFDIsSet_)(SOCKET, fd_set *);
		int     (WINAPI *inet_pton_)(int, const char *, void *);
		struct hostent *(WINAPI *gethostbyname_)(const char *);
		Ws()
		{
			HMODULE m = GetModuleHandleA("ws2_32.dll");
			socket_       = (SOCKET (WINAPI *)(int,int,int))GetProcAddress(m, "socket");
			bind_         = (int (WINAPI *)(SOCKET,const struct sockaddr*,int))GetProcAddress(m, "bind");
			listen_       = (int (WINAPI *)(SOCKET,int))GetProcAddress(m, "listen");
			accept_       = (SOCKET (WINAPI *)(SOCKET,struct sockaddr*,int*))GetProcAddress(m, "accept");
			connect_      = (int (WINAPI *)(SOCKET,const struct sockaddr*,int))GetProcAddress(m, "connect");
			send_         = (int (WINAPI *)(SOCKET,const char*,int,int))GetProcAddress(m, "send");
			recv_         = (int (WINAPI *)(SOCKET,char*,int,int))GetProcAddress(m, "recv");
			sendto_       = (int (WINAPI *)(SOCKET,const char*,int,int,const struct sockaddr*,int))GetProcAddress(m, "sendto");
			recvfrom_     = (int (WINAPI *)(SOCKET,char*,int,int,struct sockaddr*,int*))GetProcAddress(m, "recvfrom");
			shutdown_     = (int (WINAPI *)(SOCKET,int))GetProcAddress(m, "shutdown");
			setsockopt_   = (int (WINAPI *)(SOCKET,int,int,const char*,int))GetProcAddress(m, "setsockopt");
			getsockopt_   = (int (WINAPI *)(SOCKET,int,int,char*,int*))GetProcAddress(m, "getsockopt");
			getsockname_  = (int (WINAPI *)(SOCKET,struct sockaddr*,int*))GetProcAddress(m, "getsockname");
			getpeername_  = (int (WINAPI *)(SOCKET,struct sockaddr*,int*))GetProcAddress(m, "getpeername");
			closesocket_  = (int (WINAPI *)(SOCKET))GetProcAddress(m, "closesocket");
			select_       = (int (WINAPI *)(int,fd_set*,fd_set*,fd_set*,const struct timeval*))GetProcAddress(m, "select");
			gethostname_  = (int (WINAPI *)(char*,int))GetProcAddress(m, "gethostname");
			ioctlsocket_  = (int (WINAPI *)(SOCKET,long,u_long*))GetProcAddress(m, "ioctlsocket");
			WSAGetLastError_ = (int (WINAPI *)(void))GetProcAddress(m, "WSAGetLastError");
			WSAStartup_ = (int (WINAPI *)(WORD,LPWSADATA))GetProcAddress(m, "WSAStartup");
			WSADuplicateSocketW_ = (int (WINAPI *)(SOCKET,DWORD,LPWSAPROTOCOL_INFOW))GetProcAddress(m, "WSADuplicateSocketW");
			WSASocketW_ = (SOCKET (WINAPI *)(int,int,int,LPWSAPROTOCOL_INFOW,DWORD,DWORD))GetProcAddress(m, "WSASocketW");
			htons_ = (u_short (WINAPI *)(u_short))GetProcAddress(m, "htons");
			ntohs_ = (u_short (WINAPI *)(u_short))GetProcAddress(m, "ntohs");
			htonl_ = (u_long (WINAPI *)(u_long))GetProcAddress(m, "htonl");
			ntohl_ = (u_long (WINAPI *)(u_long))GetProcAddress(m, "ntohl");
			inet_addr_ = (unsigned long (WINAPI *)(const char *))GetProcAddress(m, "inet_addr");
			inet_ntoa_ = (char *(WINAPI *)(struct in_addr))GetProcAddress(m, "inet_ntoa");
			__WSAFDIsSet_ = (int (WINAPI *)(SOCKET,fd_set *))GetProcAddress(m, "__WSAFDIsSet");
			inet_pton_ = (int (WINAPI *)(int,const char *,void *))GetProcAddress(m, "inet_pton");
			gethostbyname_ = (struct hostent *(WINAPI *)(const char *))GetProcAddress(m, "gethostbyname");
		}
	} g_ws;
	int wp_fd(SOCKET s) { return (int)(intptr_t)s; }
}

static void wsa_init()
{
	WSADATA wd;
	if (g_ws.WSAStartup_(MAKEWORD(2, 2), &wd) != 0)
		abort();
}

void wp_map_wsaerr(void)
{
	errno = wp_wsa_to_errno(g_ws.WSAGetLastError_());
}

/* ---- socket API (shadows the ws2_32 imports so pseudo fds work) ---------- */

namespace
{
	int wp_socket_err(void)
	{
		wp_map_wsaerr();
		return -1;
	}
}

extern "C" {

SOCKET socket(int domain, int type, int protocol)
{
	SOCKET s;
	if (domain == PF_UNIX)
		s = g_ws.socket_(AF_INET, type, 0); /* AF_UNIX unsupported: keep a valid fd */
	else
		s = g_ws.socket_(domain, type, protocol);
	if (s == INVALID_SOCKET)
		return INVALID_SOCKET;
	return (SOCKET)(intptr_t)g_fds.add(s);
}

int bind(SOCKET s, const struct sockaddr *addr, int addrlen)
{
	SOCKET real = g_fds.get(wp_fd(s));
	if (real == INVALID_SOCKET) { errno = EBADF; return SOCKET_ERROR; }
	if (g_ws.bind_(real, addr, addrlen) == SOCKET_ERROR)
		return wp_socket_err();
	return 0;
}

int listen(SOCKET s, int backlog)
{
	SOCKET real = g_fds.get(wp_fd(s));
	if (real == INVALID_SOCKET) { errno = EBADF; return SOCKET_ERROR; }
	if (g_ws.listen_(real, backlog) == SOCKET_ERROR)
		return wp_socket_err();
	return 0;
}

SOCKET accept(SOCKET s, struct sockaddr *addr, int *addrlen)
{
	SOCKET real = g_fds.get(wp_fd(s));
	if (real == INVALID_SOCKET) { errno = EBADF; return INVALID_SOCKET; }
	SOCKET ns = g_ws.accept_(real, addr, addrlen);
	if (ns == INVALID_SOCKET)
	{
		wp_map_wsaerr();
		return INVALID_SOCKET;
	}
	return (SOCKET)(intptr_t)g_fds.add(ns);
}

int connect(SOCKET s, const struct sockaddr *addr, int addrlen)
{
	SOCKET real = g_fds.get(wp_fd(s));
	if (real == INVALID_SOCKET) { errno = EBADF; return SOCKET_ERROR; }
	if (g_ws.connect_(real, addr, addrlen) == SOCKET_ERROR)
		return wp_socket_err();
	return 0;
}

int send(SOCKET s, const char *buf, int len, int flags)
{
	SOCKET real = g_fds.get(wp_fd(s));
	if (real == INVALID_SOCKET) { errno = EBADF; return SOCKET_ERROR; }
	int r = g_ws.send_(real, buf, len, flags);
	if (r == SOCKET_ERROR)
		return wp_socket_err();
	return r;
}

int recv(SOCKET s, char *buf, int len, int flags)
{
	SOCKET real = g_fds.get(wp_fd(s));
	if (real == INVALID_SOCKET) { errno = EBADF; return SOCKET_ERROR; }
	int r = g_ws.recv_(real, buf, len, flags);
	if (r == SOCKET_ERROR)
		return wp_socket_err();
	return r;
}

int sendto(SOCKET s, const char *buf, int len, int flags,
           const struct sockaddr *to, int tolen)
{
	SOCKET real = g_fds.get(wp_fd(s));
	if (real == INVALID_SOCKET) { errno = EBADF; return SOCKET_ERROR; }
	int r = g_ws.sendto_(real, buf, len, flags, to, tolen);
	if (r == SOCKET_ERROR)
		return wp_socket_err();
	return r;
}

int recvfrom(SOCKET s, char *buf, int len, int flags,
             struct sockaddr *from, int *fromlen)
{
	SOCKET real = g_fds.get(wp_fd(s));
	if (real == INVALID_SOCKET) { errno = EBADF; return SOCKET_ERROR; }
	int r = g_ws.recvfrom_(real, buf, len, flags, from, fromlen);
	if (r == SOCKET_ERROR)
		return wp_socket_err();
	return r;
}

int shutdown(SOCKET s, int how)
{
	SOCKET real = g_fds.get(wp_fd(s));
	if (real == INVALID_SOCKET) { errno = EBADF; return SOCKET_ERROR; }
	if (g_ws.shutdown_(real, how) == SOCKET_ERROR)
		return wp_socket_err();
	return 0;
}

int setsockopt(SOCKET s, int level, int optname, const char *optval, int optlen)
{
	SOCKET real = g_fds.get(wp_fd(s));
	if (real == INVALID_SOCKET) { errno = EBADF; return SOCKET_ERROR; }
	if (g_ws.setsockopt_(real, level, optname, optval, optlen) == SOCKET_ERROR)
		return wp_socket_err();
	return 0;
}

int getsockopt(SOCKET s, int level, int optname, char *optval, int *optlen)
{
	SOCKET real = g_fds.get(wp_fd(s));
	if (real == INVALID_SOCKET) { errno = EBADF; return SOCKET_ERROR; }
	if (g_ws.getsockopt_(real, level, optname, optval, optlen) == SOCKET_ERROR)
		return wp_socket_err();
	return 0;
}

int getsockname(SOCKET s, struct sockaddr *addr, int *addrlen)
{
	SOCKET real = g_fds.get(wp_fd(s));
	if (real == INVALID_SOCKET) { errno = EBADF; return SOCKET_ERROR; }
	if (g_ws.getsockname_(real, addr, addrlen) == SOCKET_ERROR)
		return wp_socket_err();
	return 0;
}

int getpeername(SOCKET s, struct sockaddr *addr, int *addrlen)
{
	SOCKET real = g_fds.get(wp_fd(s));
	if (real == INVALID_SOCKET) { errno = EBADF; return SOCKET_ERROR; }
	if (g_ws.getpeername_(real, addr, addrlen) == SOCKET_ERROR)
		return wp_socket_err();
	return 0;
}

int gethostname(char *name, int namelen)
{
	if (g_ws.gethostname_(name, namelen) == SOCKET_ERROR)
		return wp_socket_err();
	return 0;
}

int closesocket(SOCKET s)
{
	return g_ws.closesocket_(s);
}

int ioctlsocket(SOCKET s, long cmd, u_long *arg)
{
	SOCKET real = g_fds.get(wp_fd(s));
	if (real == INVALID_SOCKET) { errno = EBADF; return SOCKET_ERROR; }
	if (g_ws.ioctlsocket_(real, cmd, arg) == SOCKET_ERROR)
		return wp_socket_err();
	return 0;
}

u_short htons(u_short x) { return g_ws.htons_(x); }
u_short ntohs(u_short x) { return g_ws.ntohs_(x); }
u_long htonl(u_long x) { return g_ws.htonl_(x); }
u_long ntohl(u_long x) { return g_ws.ntohl_(x); }
unsigned long inet_addr(const char *s) { return g_ws.inet_addr_(s); }
char *inet_ntoa(struct in_addr a) { return g_ws.inet_ntoa_(a); }
int __WSAFDIsSet(SOCKET s, fd_set *f) { return g_ws.__WSAFDIsSet_(s, f); }
int inet_pton(int af, const char *s, void *d) { return g_ws.inet_pton_(af, s, d); }
struct hostent *gethostbyname(const char *s) { return g_ws.gethostbyname_(s); }

/* dllimport callers (winsock2.h declares __declspec(dllimport)) emit __imp_X
 * references.  Satisfy them here so no -lws2_32 is needed (its import thunks
 * would duplicate our bare interposers under both GNU ld and lld-link). */
void * __imp___WSAFDIsSet = (void *)__WSAFDIsSet;
void * __imp_accept = (void *)accept;
void * __imp_bind = (void *)bind;
void * __imp_closesocket = (void *)closesocket;
void * __imp_connect = (void *)connect;
void * __imp_gethostbyname = (void *)gethostbyname;
void * __imp_gethostname = (void *)gethostname;
void * __imp_getpeername = (void *)getpeername;
void * __imp_getsockname = (void *)getsockname;
void * __imp_getsockopt = (void *)getsockopt;
void * __imp_htonl = (void *)htonl;
void * __imp_htons = (void *)htons;
void * __imp_inet_addr = (void *)inet_addr;
void * __imp_inet_ntoa = (void *)inet_ntoa;
void * __imp_inet_pton = (void *)inet_pton;
void * __imp_ioctlsocket = (void *)ioctlsocket;
void * __imp_listen = (void *)listen;
void * __imp_ntohl = (void *)ntohl;
void * __imp_ntohs = (void *)ntohs;
void * __imp_recv = (void *)recv;
void * __imp_recvfrom = (void *)recvfrom;
void * __imp_select = (void *)select;
void * __imp_send = (void *)send;
void * __imp_sendto = (void *)sendto;
void * __imp_setsockopt = (void *)setsockopt;
void * __imp_shutdown = (void *)shutdown;
void * __imp_socket = (void *)socket;

/* ---- CRT descriptor layer ------------------------------------------------ */

int close(int fd)
{
	if (g_is_pseudo(fd))
	{
		SOCKET s = g_fds.get(fd);
		g_fds.remove(fd);
		if (s == INVALID_SOCKET)
		{
			errno = EBADF;
			return -1;
		}
		if (g_ws.closesocket_(s) == SOCKET_ERROR)
		{
			wp_map_wsaerr();
			return -1;
		}
		return 0;
	}
	if (_close(fd) == -1)
		return -1;
	return 0;
}

int read(int fd, void *buf, unsigned int len)
{
	if (g_is_pseudo(fd))
	{
		SOCKET s = g_fds.get(fd);
		if (s == INVALID_SOCKET)
		{
			errno = EBADF;
			return -1;
		}
		int r = g_ws.recv_(s, (char *)buf, (int)len, 0);
		if (r == SOCKET_ERROR)
			return wp_socket_err();
		return r;
	}
	return _read(fd, buf, len);
}

int write(int fd, const void *buf, unsigned int len)
{
	if (g_is_pseudo(fd))
	{
		SOCKET s = g_fds.get(fd);
		if (s == INVALID_SOCKET)
		{
			errno = EBADF;
			return -1;
		}
		int r = g_ws.send_(s, (const char *)buf, (int)len, 0);
		if (r == SOCKET_ERROR)
			return wp_socket_err();
		return r;
	}
	return _write(fd, buf, len);
}

int dup(int fd)
{
	if (g_is_pseudo(fd))
	{
		SOCKET s = g_fds.get(fd);
		if (s == INVALID_SOCKET)
		{
			errno = EBADF;
			return -1;
		}
		WSAPROTOCOL_INFOW info;
		memset(&info, 0, sizeof(info));
		if (g_ws.WSADuplicateSocketW_(s, GetCurrentProcessId(), &info) == SOCKET_ERROR)
			return wp_socket_err();
		SOCKET ns = g_ws.WSASocketW_(FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO,
		                       FROM_PROTOCOL_INFO, &info, 0, WSA_FLAG_OVERLAPPED);
		if (ns == INVALID_SOCKET)
			return wp_socket_err();
		return g_fds.add(ns);
	}
	return _dup(fd);
}

int dup2(int fd, int fd2)
{
	if (g_is_pseudo(fd))
	{
		(void)fd2;
		return dup(fd); /* best effort: cannot remap to an arbitrary fd */
	}
	return _dup2(fd, fd2);
}

int ioctl(int fd, unsigned long request, ...)
{
	va_list ap;
	va_start(ap, request);
	void *arg = va_arg(ap, void *);
	va_end(ap);

	unsigned long realreq = request;
	/* accept the Linux FIONBIO/FIONREAD spellings too */
	if (request == 0x5421UL)      realreq = FIONBIO;
	else if (request == 0x541BUL) realreq = FIONREAD;

	if (g_is_pseudo(fd))
	{
		SOCKET s = g_fds.get(fd);
		if (s == INVALID_SOCKET)
		{
			errno = EBADF;
			return -1;
		}
		if (realreq == FIONBIO)
		{
			u_long on = arg ? *(u_long *)arg : 0;
			if (ioctlsocket(s, FIONBIO, &on) == SOCKET_ERROR)
				return wp_socket_err();
			return 0;
		}
		if (realreq == FIONREAD)
		{
			u_long n = 0;
			if (ioctlsocket(s, FIONREAD, &n) == SOCKET_ERROR)
				return wp_socket_err();
			if (arg)
				*(int *)arg = (int)n;
			return 0;
		}
		errno = EINVAL;
		return -1;
	}
	errno = ENOTTY;
	return -1;
}

int fcntl(int fd, int cmd, ...)
{
	va_list ap;
	va_start(ap, cmd);
	long arg = va_arg(ap, long);
	va_end(ap);

	if (g_is_pseudo(fd))
	{
		SOCKET s = g_fds.get(fd);
		if (s == INVALID_SOCKET)
		{
			errno = EBADF;
			return -1;
		}
		switch (cmd)
		{
		case F_GETFD:
			return 0;
		case F_SETFD:
			return 0;
		case F_GETFL:
			return O_NONBLOCK; /* sockets are always non-blocking here */
		case F_SETFL:
		{
			u_long on = (arg & O_NONBLOCK) ? 1 : 0;
			if (ioctlsocket(s, FIONBIO, &on) == SOCKET_ERROR)
				return wp_socket_err();
			return 0;
		}
		case F_DUPFD:
			return dup(fd);
		default:
			errno = EINVAL;
			return -1;
		}
	}
	switch (cmd)
	{
	case F_GETFD:
	case F_SETFD:
	case F_GETFL:
	case F_SETFL:
		return 0;
	default:
		errno = EINVAL;
		return -1;
	}
}

int pipe(int fds[2])
{
	if (!fds)
	{
		errno = EFAULT;
		return -1;
	}
	if (_pipe(fds, 8192, _O_BINARY) == -1)
		return -1;
	return 0;
}

int select(int nfds, fd_set *r, fd_set *w, fd_set *e,
           const struct timeval *tv)
{
	(void)nfds;
	/* Winsock's own select is fetched directly to avoid recursion. */
	typedef int (WINAPI *WSASelectFn)(int, fd_set *, fd_set *, fd_set *,
	                                  const struct timeval *);
	static WSASelectFn pSelect = NULL;
	if (!pSelect)
	{
		HMODULE m = GetModuleHandleA("ws2_32.dll");
		if (m)
			pSelect = (WSASelectFn)(void *)GetProcAddress(m, "select");
	}
	if (!pSelect)
	{
		errno = ENOSYS;
		return -1;
	}

	/* Convert any pseudo fds present in the sets into real sockets. */
	fd_set rr, ww, ee;
	fd_set *pr = r, *pw = w, *pe = e;
	bool translated = (r || w || e);
	if (translated)
	{
		if (r) FD_ZERO(&rr);
		if (w) FD_ZERO(&ww);
		if (e) FD_ZERO(&ee);
		unsigned i;
		if (r) for (i = 0; i < r->fd_count; i++)
		{
			int fd = (int)(intptr_t)r->fd_array[i];
			SOCKET s = g_is_pseudo(fd) ? g_fds.get(fd) : (SOCKET)fd;
			if (s != INVALID_SOCKET) FD_SET(s, &rr);
		}
		if (w) for (i = 0; i < w->fd_count; i++)
		{
			int fd = (int)(intptr_t)w->fd_array[i];
			SOCKET s = g_is_pseudo(fd) ? g_fds.get(fd) : (SOCKET)fd;
			if (s != INVALID_SOCKET) FD_SET(s, &ww);
		}
		if (e) for (i = 0; i < e->fd_count; i++)
		{
			int fd = (int)(intptr_t)e->fd_array[i];
			SOCKET s = g_is_pseudo(fd) ? g_fds.get(fd) : (SOCKET)fd;
			if (s != INVALID_SOCKET) FD_SET(s, &ee);
		}
		pr = r ? &rr : NULL;
		pw = w ? &ww : NULL;
		pe = e ? &ee : NULL;
	}

	int rc = pSelect(0, pr, pw, pe, tv);
	if (rc == SOCKET_ERROR)
		return wp_socket_err();

	if (translated)
	{
		unsigned i;
		if (r) FD_ZERO(r);
		if (w) FD_ZERO(w);
		if (e) FD_ZERO(e);
		if (r) for (i = 0; i < rr.fd_count; i++)
		{
			int fd = g_fds.fd_of(rr.fd_array[i]);
			if (fd < 0) fd = (int)(intptr_t)rr.fd_array[i];
			FD_SET((SOCKET)(intptr_t)fd, r);
		}
		if (w) for (i = 0; i < ww.fd_count; i++)
		{
			int fd = g_fds.fd_of(ww.fd_array[i]);
			if (fd < 0) fd = (int)(intptr_t)ww.fd_array[i];
			FD_SET((SOCKET)(intptr_t)fd, w);
		}
		if (e) for (i = 0; i < ee.fd_count; i++)
		{
			int fd = g_fds.fd_of(ee.fd_array[i]);
			if (fd < 0) fd = (int)(intptr_t)ee.fd_array[i];
			FD_SET((SOCKET)(intptr_t)fd, e);
		}
	}
	return rc;
}

int poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
	if (nfds == 0)
	{
		if (timeout > 0) Sleep(timeout);
		return 0;
	}
	if (!fds)
	{
		errno = EFAULT;
		return -1;
	}

	typedef int (WINAPI *WSAPollFn)(void *, ULONG, INT);
	static WSAPollFn pWSAPoll = NULL;
	if (!pWSAPoll)
	{
		HMODULE m = GetModuleHandleA("ws2_32.dll");
		if (m)
			pWSAPoll = (WSAPollFn)(void *)GetProcAddress(m, "WSAPoll");
	}

	struct WPFD { SOCKET fd; short events; short revents; };
	static WPFD *wfds = NULL;
	static unsigned long wcap = 0;
	if (wcap < nfds)
	{
		delete[] wfds;
		wfds = new WPFD[nfds];
		wcap = nfds;
	}

	unsigned long i, nvalid = 0;
	for (i = 0; i < nfds; i++)
	{
		int fd = fds[i].fd;
		if (g_is_pseudo(fd))
		{
			wfds[nvalid].fd = g_fds.get(fd);
			wfds[nvalid].events = fds[i].events;
			wfds[nvalid].revents = 0;
			nvalid++;
		}
		else if (fd >= 0 && fd <= 2)
		{
			/* console handles cannot be polled by Winsock: mark them
			 * always ready so stdin-driven IO (rare) still functions */
			wfds[nvalid].fd = INVALID_SOCKET;
			wfds[nvalid].events = fds[i].events;
			wfds[nvalid].revents = (short)(fds[i].events & (POLLIN | POLLOUT));
			nvalid++;
		}
		else
		{
			fds[i].revents = POLLNVAL;
		}
	}

	int rc;
	if (pWSAPoll)
	{
		if (nvalid == 0)
		{
			if (timeout > 0) Sleep(timeout);
			return 0;
		}
		rc = pWSAPoll(wfds, (ULONG)nvalid, timeout);
		if (rc == SOCKET_ERROR)
			return wp_socket_err();
	}
	else
	{
		/* select() fallback */
		fd_set rds, wrs;
		FD_ZERO(&rds); FD_ZERO(&wrs);
		for (i = 0; i < nvalid; i++)
		{
			if (wfds[i].fd == INVALID_SOCKET) continue;
			if (wfds[i].events & POLLIN)  FD_SET(wfds[i].fd, &rds);
			if (wfds[i].events & POLLOUT) FD_SET(wfds[i].fd, &wrs);
		}
		struct timeval tv, *ptv = NULL;
		if (timeout >= 0)
		{
			tv.tv_sec = timeout / 1000;
			tv.tv_usec = (timeout % 1000) * 1000;
			ptv = &tv;
		}
		typedef int (WINAPI *WSASelectFn)(int, fd_set *, fd_set *, fd_set *,
		                                  const struct timeval *);
		static WSASelectFn pSelect = NULL;
		if (!pSelect)
		{
			HMODULE m = GetModuleHandleA("ws2_32.dll");
			if (m)
				pSelect = (WSASelectFn)(void *)GetProcAddress(m, "select");
		}
		rc = pSelect ? pSelect(0, &rds, &wrs, NULL, ptv) : SOCKET_ERROR;
		if (rc == SOCKET_ERROR)
			return wp_socket_err();
		for (i = 0; i < nvalid; i++)
		{
			if (wfds[i].fd == INVALID_SOCKET) continue;
			short rev = 0;
			if (FD_ISSET(wfds[i].fd, &rds)) rev |= POLLIN;
			if (FD_ISSET(wfds[i].fd, &wrs)) rev |= POLLOUT;
			wfds[i].revents = rev;
		}
	}

	/* copy results back into the caller's array */
	int count = 0;
	unsigned long j = 0;
	for (i = 0; i < nfds; i++)
	{
		int fd = fds[i].fd;
		if (g_is_pseudo(fd))
		{
			fds[i].revents = wfds[j].revents;
			if (fds[i].revents) count++;
			j++;
		}
		else if (fd >= 0 && fd <= 2)
		{
			fds[i].revents = wfds[j].revents;
			if (fds[i].revents) count++;
			j++;
		}
		else
		{
			fds[i].revents = POLLNVAL;
		}
	}
	return count;
}

ssize_t pread(int fd, void *buf, size_t len, long long off)
{
	if (g_is_pseudo(fd))
	{
		errno = ESPIPE;
		return -1;
	}
	if (_lseeki64(fd, off, SEEK_SET) == -1)
		return -1;
	if (len > 0x7fffffffUL) len = 0x7fffffffUL;
	return _read(fd, buf, (unsigned int)len);
}

ssize_t pwrite(int fd, const void *buf, size_t len, long long off)
{
	if (g_is_pseudo(fd))
	{
		errno = ESPIPE;
		return -1;
	}
	if (_lseeki64(fd, off, SEEK_SET) == -1)
		return -1;
	if (len > 0x7fffffffUL) len = 0x7fffffffUL;
	return _write(fd, buf, (unsigned int)len);
}

/* ---- signal emulation layer --------------------------------------------- */

namespace
{
	CRITICAL_SECTION g_sig_cs;
	HANDLE g_sig_event = NULL;
	volatile unsigned long g_sig_pending = 0;
	volatile long g_sig_consumers = 0;
	void (*g_handlers[NSIG])(int);
	bool g_sig_init = false;

	void sig_init()
	{
		if (!g_sig_init)
		{
			InitializeCriticalSection(&g_sig_cs);
			g_sig_event = CreateEventW(NULL, TRUE, FALSE, NULL);
			memset((void *)g_handlers, 0, sizeof(g_handlers));
			g_sig_init = true;
		}
	}

	void emit_signal(int sig)
	{
		if (sig < 1 || sig >= NSIG)
			return;
		sig_init();
		EnterCriticalSection(&g_sig_cs);
		g_sig_pending |= (1UL << sig);
		SetEvent(g_sig_event);
		LeaveCriticalSection(&g_sig_cs);
	}

	BOOL WINAPI ctrl_handler(DWORD type)
	{
		int sig = -1;
		switch (type)
		{
		case CTRL_C_EVENT:   sig = SIGINT;  break;
		case CTRL_BREAK_EVENT: sig = SIGBREAK; break;
		case CTRL_CLOSE_EVENT:
		case CTRL_LOGOFF_EVENT:
		case CTRL_SHUTDOWN_EVENT: sig = SIGTERM; break;
		default: break;
		}
		if (sig > 0 && sig < NSIG && g_handlers[sig] &&
		    g_handlers[sig] != (void (*)(int))SIG_DFL &&
		    g_handlers[sig] != (void (*)(int))SIG_IGN)
		{
			(*g_handlers[sig])(sig);
			return TRUE;
		}
		if (sig == SIGINT || sig == SIGTERM)
			return FALSE; /* let the default (terminate) handle it */
		return TRUE;
	}

	struct ConsoleInit
	{
		ConsoleInit()
		{
			sig_init();
			SetConsoleCtrlHandler(ctrl_handler, TRUE);
		}
	} g_console_init;
}

HANDLE wp_signal_event(void)
{
	sig_init();
	return g_sig_event;
}

unsigned long wp_signal_take(void)
{
	unsigned long bits;
	sig_init();
	EnterCriticalSection(&g_sig_cs);
	bits = g_sig_pending;
	g_sig_pending = 0;
	ResetEvent(g_sig_event);
	LeaveCriticalSection(&g_sig_cs);
	return bits;
}

/* NSIG for our table */
#ifndef NSIG
#define NSIG 32
#endif

void (*signal(int sig, void (*handler)(int)))(int)
{
	void (*old)(int);
	sig_init();
	if (sig < 1 || sig >= NSIG)
		return SIG_ERR;
	EnterCriticalSection(&g_sig_cs);
	old = g_handlers[sig];
	g_handlers[sig] = handler;
	LeaveCriticalSection(&g_sig_cs);
	return old;
}

int raise(int sig)
{
	void (*h)(int) = NULL;
	if (sig >= 1 && sig < NSIG)
	{
		EnterCriticalSection(&g_sig_cs);
		h = g_handlers[sig];
		LeaveCriticalSection(&g_sig_cs);
	}
	if (h && h != (void (*)(int))SIG_DFL && h != (void (*)(int))SIG_IGN)
	{
		(*h)(sig);
		return 0;
	}
	return kill(GetCurrentProcessId(), sig);
}

int kill(int pid, int sig)
{
	(void)pid;
	if (sig == 0)
		return 0;
	if (sig >= 1 && sig < NSIG)
	{
		emit_signal(sig);
		return 0;
	}
	errno = EINVAL;
	return -1;
}

int sigemptyset(sigset_t *set)
{
	if (!set) { errno = EINVAL; return -1; }
	memset(set, 0, sizeof(*set));
	return 0;
}

int sigfillset(sigset_t *set)
{
	if (!set) { errno = EINVAL; return -1; }
	memset(set, 0xFF, sizeof(*set));
	return 0;
}

int sigaddset(sigset_t *set, int sig)
{
	if (!set || sig < 1 || sig >= (int)(sizeof(sigset_t) * 8))
	{
		errno = EINVAL;
		return -1;
	}
	set->__bits[0] |= (1UL << sig);
	return 0;
}

int sigdelset(sigset_t *set, int sig)
{
	if (!set || sig < 1 || sig >= (int)(sizeof(sigset_t) * 8))
	{
		errno = EINVAL;
		return -1;
	}
	set->__bits[0] &= ~(1UL << sig);
	return 0;
}

int sigismember(const sigset_t *set, int sig)
{
	if (!set || sig < 1 || sig >= (int)(sizeof(sigset_t) * 8))
	{
		errno = EINVAL;
		return -1;
	}
	return (set->__bits[0] & (1UL << sig)) ? 1 : 0;
}

int sigprocmask(int how, const sigset_t *set, sigset_t *old)
{
	(void)how; (void)set;
	if (old)
		sigemptyset(old);
	return 0;
}

int sigaction(int sig, const struct sigaction *act, struct sigaction *oldact)
{
	/* register the handler through our table; masks are not enforced */
	if (oldact)
	{
		memset(oldact, 0, sizeof(*oldact));
		oldact->sa_handler = g_handlers[sig];
	}
	if (act)
	{
		void (*h)(int) = act->sa_handler;
		if (act->sa_flags & SA_SIGINFO && act->sa_sigaction)
			h = (void (*)(int))act->sa_sigaction;
		signal(sig, h);
	}
	return 0;
}

/* ---- time helpers --------------------------------------------------------- */

struct tm * localtime_r(const time_t *t, struct tm *buf)
{
	if (!t || !buf) { errno = EINVAL; return NULL; }
	if (localtime_s(buf, t) != 0)
		return NULL;
	return buf;
}


struct tm * gmtime_r(const time_t *t, struct tm *buf)
{
	if (!t || !buf) { errno = EINVAL; return NULL; }
	if (gmtime_s(buf, t) != 0)
		return NULL;
	return buf;
}

char *ctime_r(const time_t *t, char *buf)
{
	if (!t || !buf) { errno = EINVAL; return NULL; }
	if (ctime_s(buf, 26, t) != 0)
		return NULL;
	return buf;
}

/* NOTE: current mingw-w64 CRTs export clock_gettime(), so the native build
 * defines WP_HAVE_CLOCK_GETTIME and skips this.  The zig/clang build has no
 * usable CRT copy, so it keeps ours.  The declaration in winposix.h stays
 * unconditional (a duplicate identical C declaration is legal). */
#ifndef WP_HAVE_CLOCK_GETTIME
int clock_gettime(int clk_id, struct timespec *tp)
{
	if (!tp) { errno = EFAULT; return -1; }
	if (clk_id == CLOCK_MONOTONIC)
	{
		static LARGE_INTEGER freq = { 0, 0 };
		LARGE_INTEGER now;
		long long total_ns;
		if (freq.QuadPart == 0)
			QueryPerformanceFrequency(&freq);
		QueryPerformanceCounter(&now);
		total_ns = now.QuadPart * 1000000000LL / freq.QuadPart;
		tp->tv_sec = (long)(total_ns / 1000000000LL);
		tp->tv_nsec = (long)(total_ns % 1000000000LL);
		return 0;
	}
	{
		FILETIME ft;
		unsigned long long t;
		GetSystemTimeAsFileTime(&ft);
		t = (((unsigned long long)ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
		t -= 116444736000000000ULL;  /* 1601 -> 1970 */
		tp->tv_sec = (long)(t / 10000000ULL);
		tp->tv_nsec = (long)((t % 10000000ULL) * 100);
		return 0;
	}
}
#endif /* WP_HAVE_CLOCK_GETTIME */

int gettimeofday(struct timeval *tv, void *tz)
{
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft);
	unsigned long long t =
		(((unsigned long long)ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
	t -= 116444736000000000ULL;  /* 1601 -> 1970 */
	t /= 10;                      /* 100ns -> us */
	if (tv)
	{
		tv->tv_sec = (long)(t / 1000000ULL);
		tv->tv_usec = (long)(t % 1000000ULL);
	}
	(void)tz;
	return 0;
}

long sysconf(int name)
{
	switch (name)
	{
	case 84: /* _SC_NPROCESSORS_ONLN */
	{
		SYSTEM_INFO si;
		GetSystemInfo(&si);
		return (long)si.dwNumberOfProcessors;
	}
	case 30: /* _SC_PAGESIZE */
	{
		SYSTEM_INFO si;
		GetSystemInfo(&si);
		return (long)si.dwPageSize;
	}
	case 0:  /* _SC_ARG_MAX */
		return 32767;
	default:
		errno = EINVAL;
		return -1;
	}
}

int usleep(unsigned int usec)
{
	if (usec)
		Sleep(usec >= 1000 ? (usec + 999) / 1000 : 1);
	return 0;
}

unsigned int sleep(unsigned int secs)
{
	if (secs)
		Sleep(secs * 1000);
	return 0;
}

/* ---- interval timer emulation ---------------------------------------------
 * Linux code arms ITIMER_REAL and waits for SIGALRM.  On Windows a helper
 * thread fires the SIGALRM bit at the configured interval and the engine
 * loop (thread.cpp, patched) wakes on wp_signal_event(). */

namespace
{
	struct TimerConf
	{
		CRITICAL_SECTION cs;
		long long period_us;   /* 0 = disarmed */
		bool thread_started;
		TimerConf() : period_us(0), thread_started(false)
		{
			InitializeCriticalSection(&cs);
		}
	} wp_g_timer;
	volatile bool g_timer_stop = false;

	unsigned __stdcall wp_timer_thread_proc(void *arg)
	{
		(void)arg;
		for (;;)
		{
			long long us;
			EnterCriticalSection(&wp_g_timer.cs);
			us = wp_g_timer.period_us;
			LeaveCriticalSection(&wp_g_timer.cs);
			if (us <= 0)
			{
				Sleep(20);
				continue;
			}
			Sleep((DWORD)((us + 999) / 1000));
			emit_signal(SIGALRM);
		}
	}
}

int setitimer(int which, const struct itimerval *value,
              struct itimerval *ovalue)
{
	(void)which;
	if (ovalue)
	{
		memset(ovalue, 0, sizeof(*ovalue));
	}
	EnterCriticalSection(&wp_g_timer.cs);
	if (!value)
	{
		wp_g_timer.period_us = 0;
	}
	else
	{
		long long iv =
			((long long)value->it_value.tv_sec * 1000000LL) + value->it_value.tv_usec;
		long long per =
			((long long)value->it_interval.tv_sec * 1000000LL) + value->it_interval.tv_usec;
		wp_g_timer.period_us = per > 0 ? per : (iv > 0 ? iv : 0);
		if (!wp_g_timer.thread_started)
		{
			_beginthreadex(NULL, 0, wp_timer_thread_proc, NULL, 0, NULL);
			wp_g_timer.thread_started = true;
		}
	}
	LeaveCriticalSection(&wp_g_timer.cs);
	(void)g_timer_stop;
	return 0;
}

int getitimer(int which, struct itimerval *value)
{
	(void)which;
	if (value)
		memset(value, 0, sizeof(*value));
	return 0;
}

/* ---- misc ---------------------------------------------------------------- */

int fsync(int fd)
{
	HANDLE h;
	if (g_is_pseudo(fd))
	{
		errno = EINVAL;
		return -1;
	}
	h = (HANDLE)_get_osfhandle(fd);
	if (h == (HANDLE)-1)
		return -1;
	if (!FlushFileBuffers(h))
	{
		errno = EIO;
		return -1;
	}
	return 0;
}

int alphasort(const struct dirent **a, const struct dirent **b)
{
	return strcmp((*a)->d_name, (*b)->d_name);
}

int scandir(const char *dir, struct dirent ***namelist,
            int (*filter)(const struct dirent *),
            int (*compar)(const struct dirent **, const struct dirent **))
{
	DIR *d;
	struct dirent *ent;
	struct dirent **list = NULL;
	size_t count = 0, cap = 0;
	if (!dir || !namelist) { errno = EFAULT; return -1; }
	*namelist = NULL;
	d = opendir(dir);
	if (!d) return -1;
	while ((ent = readdir(d)) != NULL)
	{
		struct dirent *copy;
		if (filter && !filter(ent))
			continue;
		if (count == cap)
		{
			size_t ncap = cap ? cap * 2 : 32;
			struct dirent **nlist = (struct dirent **)realloc(list, ncap * sizeof(*nlist));
			if (!nlist) { closedir(d); errno = ENOMEM; return -1; }
			list = nlist;
			cap = ncap;
		}
		copy = (struct dirent *)malloc(sizeof(*copy));
		if (!copy) { closedir(d); errno = ENOMEM; return -1; }
		memcpy(copy, ent, sizeof(*copy));
		list[count++] = copy;
	}
	closedir(d);
	if (compar && count > 1)
		qsort(list, count, sizeof(*list),
		      (int (*)(const void *, const void *))compar);
	*namelist = list;
	return (int)count;
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt)
{
	ssize_t total = 0;
	int i;
	if (iovcnt < 0) { errno = EINVAL; return -1; }
	for (i = 0; i < iovcnt; i++)
	{
		ssize_t r = read(fd, iov[i].iov_base, (unsigned int)iov[i].iov_len);
		if (r < 0)
			return total ? total : -1;
		total += r;
		if ((size_t)r < iov[i].iov_len)
			break;
	}
	return total;
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt)
{
	ssize_t total = 0;
	int i;
	if (iovcnt < 0) { errno = EINVAL; return -1; }
	for (i = 0; i < iovcnt; i++)
	{
		ssize_t r = write(fd, iov[i].iov_base, (unsigned int)iov[i].iov_len);
		if (r < 0)
			return total ? total : -1;
		total += r;
		if ((size_t)r < iov[i].iov_len)
			break;
	}
	return total;
}

void *mmap(void *addr, size_t len, int prot, int flags, int fd, long long off)
{
	(void)addr; (void)len; (void)prot; (void)flags; (void)fd; (void)off;
	errno = ENOSYS;
	return (void *)-1;
}

int munmap(void *addr, size_t len)
{
	(void)addr; (void)len;
	return 0;
}

} /* extern "C" */

/* C++ linkage on purpose (see winposix.h): this overloads the 1-argument CRT
 * mkdir().  ftruncate below is extern "C" instead: storage TUs include
 * <unistd.h>, whose C declaration only merges with an identical C one. */
int mkdir(const char *path, int mode)
{
	(void)mode; /* Windows has no POSIX permission bits */
	if (!path) { errno = EFAULT; return -1; }
	return _mkdir(path);
}

/* NOTE: declared (int, off_t) in winposix.h so it merges with mingw's
 * <unistd.h>; defined here with long long so 64-bit WDB callers pass full
 * lengths (winposix.o itself builds with _FILE_OFFSET_BITS=64 so the two
 * spellings name the same type).  32-bit callers zero-extend, which is the
 * correct value for them.  First on the link line, so this wins over the
 * CRT's 32-bit ftruncate like the other interposers. */
extern "C" int ftruncate(int fd, long long len)
{
	HANDLE h;
	LARGE_INTEGER pos, cur;
	if (g_is_pseudo(fd))
	{
		errno = EINVAL;
		return -1;
	}
	if (len < 0) { errno = EINVAL; return -1; }
	h = (HANDLE)_get_osfhandle(fd);
	if (h == (HANDLE)-1)
		return -1;
	cur.QuadPart = 0;
	if (!SetFilePointerEx(h, cur, &pos, FILE_CURRENT))
		return -1;
	cur.QuadPart = len;
	if (!SetFilePointerEx(h, cur, NULL, FILE_BEGIN))
		return -1;
	if (!SetEndOfFile(h))
		return -1;
	SetFilePointerEx(h, pos, NULL, FILE_BEGIN);
	return 0;
}

/* setlinebuf(): gamedbd/gs ask for line-buffered stdout.  The UCRT honors
 * _IOLBF the same as _IOFBF (no true line buffering), which is the best
 * available and all these call sites need. */
extern "C" void setlinebuf(FILE *f)
{
	if (f) setvbuf(f, NULL, _IOLBF, 0);
}

/* C++-linkage overload (NOT extern "C"): gs passes long* on LLP64. */
struct tm *localtime(long *t)
{
	time_t tt = (time_t)*t;
	return ::localtime(&tt);
}
