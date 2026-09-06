/* Windows shim: <sys/mman.h> (stub - only diagnostics use it) */
#ifndef _WP_SYS_MMAN_H
#define _WP_SYS_MMAN_H
#include "winposix.h"
#ifndef MAP_FAILED
#define MAP_FAILED ((void*)-1)
#endif
#ifndef PROT_READ
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
#define PROT_NONE 0
#define MAP_SHARED 1
#define MAP_PRIVATE 2
#define MAP_ANONYMOUS 0x20
#define MAP_ANON MAP_ANONYMOUS
#define MAP_FIXED 0x10
#define MS_ASYNC 1
#define MS_SYNC 2
#define MS_INVALIDATE 4
#endif
#ifdef __cplusplus
extern "C" {
#endif
void *mmap(void *addr, size_t len, int prot, int flags, int fd, long long off);
int munmap(void *addr, size_t len);
#ifdef __cplusplus
}
#endif
#endif
