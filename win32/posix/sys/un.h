/* Windows shim: <sys/un.h> - AF_UNIX address layout for the game code.
 *
 * The only provider of these in the Windows build (winposix.h deliberately
 * does not include <afunix.h>, which older mingw sysroots lack).  The layout
 * matches Linux (family + 108-byte path) so sizeof/strncpy call sites in
 * log.h/activeio.h/passiveio.h behave identically.  Runtime AF_UNIX use is
 * best-effort, see winposix.cpp socket().
 */
#ifndef _WP_SYS_UN_H
#define _WP_SYS_UN_H
#include "winposix.h"
#ifndef AF_UNIX
#define AF_UNIX 1
#endif
#ifndef PF_UNIX
#define PF_UNIX AF_UNIX
#endif
#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif
struct sockaddr_un
{
	unsigned short sun_family;
	char sun_path[108];
};
#endif
