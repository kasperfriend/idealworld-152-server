
#ifndef _SHARE_PLATFORM_H_
#define _SHARE_PLATFORM_H_

#ifdef WIN32

#include <windows.h>

#ifndef _REENTRANT_
#define _REENTRANT_
#endif

#ifndef __i386__
#ifndef _WIN64
#define __i386__ 
#endif
#endif

#ifdef _WIN64
#ifndef __x86_64__
#define __x86_64__
#endif
#ifndef __amd64__
#define __amd64__
#endif
#endif

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

#ifndef int64_t
typedef long long int64_t;
#endif
#ifndef uint64_t
typedef unsigned long long uint64_t;
#endif
#ifndef int32_t
typedef int int32_t;
#endif
#ifndef uint32_t
typedef unsigned int uint32_t;
#endif

#else

#include <stdint.h>

#endif

#endif // _SHARE_PLATFORM_H_
