/* clang/zig-only stub for <openssl/md5.h> - the MSYS2 build uses the real
 * OpenSSL headers from the mingw-w64-x86_64-openssl package. */
#ifndef _WP_OPENSSL_MD5_H
#define _WP_OPENSSL_MD5_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define MD5_DIGEST_LENGTH 16
void MD5(const unsigned char *d, size_t n, unsigned char *md);
#ifdef __cplusplus
}
#endif
#endif
