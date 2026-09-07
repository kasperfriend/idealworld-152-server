/* Windows shim: <sys/uio.h> - vectored I/O over plain read/write.
 * Only the struct + declarations live here; winposix.cpp implements them. */
#ifndef _WP_SYS_UIO_H
#define _WP_SYS_UIO_H
#include "winposix.h"
#include <stddef.h>
#ifndef _WP_IOVEC_DEFINED
#define _WP_IOVEC_DEFINED
struct iovec
{
	void *iov_base;
	size_t iov_len;
};
#endif
#endif
