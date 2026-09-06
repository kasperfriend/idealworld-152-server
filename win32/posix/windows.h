/* windows.h shim: force the lean & non-min/max-polluting Windows API, and
 * keep <winsock.h> (old) out so that winsock2.h can be included later in the
 * same translation unit without the classic ordering conflict. */
#ifndef _WP_WINDOWS_H
#define _WP_WINDOWS_H
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
/* The game owns `struct MSG` (cgame/common/message.h); winuser.h must not
 * claim that name.  winposix.h does the same dance; both nest safely. */
#define MSG WP_WinMSG
#include_next <windows.h>
#undef MSG
#endif
