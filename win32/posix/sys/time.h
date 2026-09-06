/* Windows shim: <sys/time.h> - layer over the mingw one, adds itimerval */
#ifndef _WP_SYS_TIME_H
#define _WP_SYS_TIME_H
#include_next <sys/time.h>

#ifndef ITIMER_REAL
#define ITIMER_REAL 0
#define ITIMER_VIRTUAL 1
#define ITIMER_PROF 2
#endif

#ifndef _WP_ITIMERVAL_DEFINED
#define _WP_ITIMERVAL_DEFINED
struct itimerval
{
	struct timeval it_interval; /* next value */
	struct timeval it_value;    /* current value */
};
#endif

#ifdef __cplusplus
extern "C" {
#endif
int setitimer(int which, const struct itimerval *value,
              struct itimerval *ovalue);
int getitimer(int which, struct itimerval *value);
#ifdef __cplusplus
}
#endif
#endif
