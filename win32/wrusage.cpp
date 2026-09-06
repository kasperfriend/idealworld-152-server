/*
 * wrusage.cpp - Windows implementation of the getrusage() surface declared
 * in win32/posix/sys/resource.h.
 *
 * ru_utime / ru_stime come from GetProcessTimes / GetThreadTimes (100ns
 * FILETIME units), ru_maxrss from the process working-set information.
 * Compiled into the shim archive (winposix.a on the native path, added
 * explicitly on the zig path) and linked into every daemon.
 */
#include "winposix.h"

#include <psapi.h>
#include <sys/resource.h>
#include <string.h>
#include <errno.h>

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS 0
#endif

static void filetime_to_timeval(const FILETIME *ft, struct timeval *tv)
{
	ULARGE_INTEGER u;
	unsigned long long us;

	u.LowPart = ft->dwLowDateTime;
	u.HighPart = ft->dwHighDateTime;
	/* FILETIME is in 100 ns units; convert to microseconds. */
	us = u.QuadPart / 10;
	tv->tv_sec = (long)(us / 1000000ULL);
	tv->tv_usec = (long)(us % 1000000ULL);
}

int getrusage(int who, struct rusage *usage)
{
	FILETIME create, exit, kernel, user;
	HANDLE h;

	if (!usage)
	{
		errno = EFAULT;
		return -1;
	}
	memset(usage, 0, sizeof(*usage));

	if (who == RUSAGE_THREAD)
	{
		h = GetCurrentThread();
		if (!GetThreadTimes(h, &create, &exit, &kernel, &user))
		{
			errno = EINVAL;
			return -1;
		}
	}
	else if (who == RUSAGE_SELF)
	{
		h = GetCurrentProcess();
		if (!GetProcessTimes(h, &create, &exit, &kernel, &user))
		{
			errno = EINVAL;
			return -1;
		}
	}
	else
	{
		/* RUSAGE_CHILDREN: not tracked on Windows. */
		errno = EINVAL;
		return -1;
	}

	filetime_to_timeval(&user, &usage->ru_utime);
	filetime_to_timeval(&kernel, &usage->ru_stime);

	/* Peak working set (bytes) -> ru_maxrss (KB). */
	{
		PROCESS_MEMORY_COUNTERS pmc;
		if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
			usage->ru_maxrss = (long)(pmc.PeakWorkingSetSize / 1024);
	}

	return 0;
}

int getrlimit(int resource, struct rlimit *rlim)
{
	(void)resource;
	if (!rlim)
	{
		errno = EFAULT;
		return -1;
	}
	/* The server code never calls getrlimit(); report "unlimited" so a
	 * future caller gets benign values instead of a hard failure. */
	rlim->rlim_cur = RLIM_INFINITY;
	rlim->rlim_max = RLIM_INFINITY;
	return 0;
}

int setrlimit(int resource, const struct rlimit *rlim)
{
	(void)resource;
	(void)rlim;
	return 0;
}
