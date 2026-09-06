/*
 * pcre.h - compile/link stand-in for PCRE 8.x used only by the zig cross
 * toolchain.  The MSYS2/OpenSSL (native) build includes the real PCRE
 * headers from the mingw-w64-x86_64-pcre package and real semantics.
 *
 * The implementation wraps std::regex (ECMAScript).  It exists so local
 * iteration can compile and link the daemons that use pcre for name/chat
 * filtering; behaviour parity is not guaranteed.
 */
#ifndef _WP_PCRE_H
#define _WP_PCRE_H

#include <cstddef>

#define PCRE_CASELESS      0x00000001
#define PCRE_MULTILINE     0x00000002
#define PCRE_DOTALL        0x00000004
#define PCRE_UTF8          0x00000800
#define PCRE_NO_UTF8_CHECK 0x20000000

#define PCRE_ERROR_NOMATCH (-1)

#define PCRE_MAJOR 8
#define PCRE_MINOR 45

typedef struct real_pcre pcre;
typedef struct pcre_extra pcre_extra;

#ifdef __cplusplus
extern "C" {
#endif

extern void *(*pcre_malloc)(size_t);
extern void (*pcre_free)(void *);

pcre *pcre_compile(const char *pattern, int options, const char **errptr,
                   int *erroffset, const unsigned char *tableptr);
int pcre_exec(const pcre *code, const pcre_extra *extra, const char *subject,
              int length, int startoffset, int options, int *ovector,
              int ovecsize);

#ifdef __cplusplus
}
#endif
#endif
