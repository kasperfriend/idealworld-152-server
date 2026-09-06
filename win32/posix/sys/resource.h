/* Windows shim: <sys/resource.h>
 *
 * Only the getrusage() subset used by the server code is provided.  The
 * rusage layout mirrors the glibc one so code that reads named members
 * (ru_utime/ru_stime/ru_maxrss/...) compiles unchanged.
 */
#ifndef _WP_SYS_RESOURCE_H
#define _WP_SYS_RESOURCE_H

#include <sys/types.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef RUSAGE_SELF
#define RUSAGE_SELF 0
#endif
#ifndef RUSAGE_CHILDREN
#define RUSAGE_CHILDREN (-1)
#endif
#ifndef RUSAGE_THREAD
#define RUSAGE_THREAD 1
#endif

#ifndef RLIM_INFINITY
#define RLIM_INFINITY (~0UL)
#endif

struct rusage
{
	struct timeval ru_utime;   /* user CPU time used */
	struct timeval ru_stime;   /* system CPU time used */
	long ru_maxrss;            /* maximum resident set size (KB) */
	long ru_ixrss;             /* integral shared memory size */
	long ru_idrss;             /* integral unshared data size */
	long ru_isrss;             /* integral unshared stack size */
	long ru_minflt;            /* page reclaims (soft page faults) */
	long ru_majflt;            /* page faults (hard page faults) */
	long ru_nswap;             /* swaps */
	long ru_inblock;           /* block input operations */
	long ru_oublock;           /* block output operations */
	long ru_msgsnd;            /* IPC messages sent */
	long ru_msgrcv;            /* IPC messages received */
	long ru_nsignals;          /* signals received */
	long ru_nvcsw;             /* voluntary context switches */
	long ru_nivcsw;            /* involuntary context switches */
};

struct rlimit
{
	unsigned long rlim_cur;
	unsigned long rlim_max;
};

#ifndef RLIMIT_CPU
#define RLIMIT_CPU 0
#endif
#ifndef RLIMIT_FSIZE
#define RLIMIT_FSIZE 1
#endif
#ifndef RLIMIT_DATA
#define RLIMIT_DATA 2
#endif
#ifndef RLIMIT_STACK
#define RLIMIT_STACK 3
#endif
#ifndef RLIMIT_CORE
#define RLIMIT_CORE 4
#endif
#ifndef RLIMIT_NOFILE
#define RLIMIT_NOFILE 7
#endif
#ifndef RLIMIT_AS
#define RLIMIT_AS 9
#endif

int getrusage(int who, struct rusage *usage);
int getrlimit(int resource, struct rlimit *rlim);
int setrlimit(int resource, const struct rlimit *rlim);

#ifdef __cplusplus
}
#endif

#endif /* _WP_SYS_RESOURCE_H */
