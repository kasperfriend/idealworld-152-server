/* zig-only <sched.h>.  Native builds use the real one; zig cc ships none.
 * The tree only uses sched_yield (cgame/common/spinlock.cpp). */
#ifndef WP_ZIG_SCHED_H
#define WP_ZIG_SCHED_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline int sched_yield(void)
{
    SwitchToThread();
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* WP_ZIG_SCHED_H */
