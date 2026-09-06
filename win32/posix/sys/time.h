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

#ifndef timerisset
#define timerisset(tvp) ((tvp)->tv_sec || (tvp)->tv_usec)
#endif
#ifndef timerclear
#define timerclear(tvp) ((tvp)->tv_sec = (tvp)->tv_usec = 0)
#endif
#ifndef timercmp
#define timercmp(a, b, CMP) (((a)->tv_sec == (b)->tv_sec) ? \
                             ((a)->tv_usec CMP (b)->tv_usec) : \
                             ((a)->tv_sec CMP (b)->tv_sec))
#endif
#ifndef timeradd
#define timeradd(a, b, result) do { \
        (result)->tv_sec = (a)->tv_sec + (b)->tv_sec; \
        (result)->tv_usec = (a)->tv_usec + (b)->tv_usec; \
        if ((result)->tv_usec >= 1000000) { \
            ++(result)->tv_sec; \
            (result)->tv_usec -= 1000000; \
        } \
    } while (0)
#endif
#ifndef timersub
#define timersub(a, b, result) do { \
        (result)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
        (result)->tv_usec = (a)->tv_usec - (b)->tv_usec; \
        if ((result)->tv_usec < 0) { \
            --(result)->tv_sec; \
            (result)->tv_usec += 1000000; \
        } \
    } while (0)
#endif
#endif
