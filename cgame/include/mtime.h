/*
	mtime.h
	作者： 崔铭
	功能： 与时间相关的函数
        注释：
*/

#ifndef __MTIME_H__
#define __MTIME_H__

#ifdef __cplusplus
extern "C" {
#endif

/* The hand declarations below are only for MSVC, which has neither
 * <sys/time.h> nor <unistd.h>.  mingw-w64 and clang (this tree's Windows
 * server toolchains) provide both headers, together with
 * gettimeofday/usleep declarations that would clash with these. */
#if defined(WIN32) && defined(_MSC_VER)

struct timezone;
int  gettimeofday(struct timeval *tv,struct timezone *tz);
void usleep(unsigned long usec);

#else
#include <sys/time.h>
#include <unistd.h>
#endif

unsigned int 	gettickcount();		/*return millisecond*/
unsigned long 	timegettime();		/*return microsecond*/
int 		msleep(struct timeval *tv);

#ifdef __cplusplus
};
#endif
#endif

