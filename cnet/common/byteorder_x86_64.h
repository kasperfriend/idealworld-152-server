#ifndef __BYTEORDER_H
#define __BYTEORDER_H

#include "common/platform.h"

namespace GNET
{

#if defined BYTE_ORDER_BIG_ENDIAN
	#define byteorder_16(x)	(x)
	#define byteorder_32(x)	(x)
	#define byteorder_64(x)	(x)
#elif defined __GNUC__
	/*
	#define byteorder_16 htons
	#define byteorder_32 htonl
	*/
	inline unsigned short byteorder_16(unsigned short x)
	{
		register unsigned short v;
		__asm__ __volatile__("xchgb %b0, %h0" : "=a"(v) : "0"(x));
		return v;
	}
	inline unsigned int byteorder_32(unsigned int x)
	{
		register unsigned int v;
		__asm__ ("bswapl %0" : "=r"(v) : "0"(x));
		return v;
	}
	/* unsigned long long (not unsigned long): on LLP64 targets such as
	 * 64-bit Windows, unsigned long is only 32 bits wide, which both
	 * truncated the value and made clang reject the bswapq below.  The
	 * builtin is identical to the hand-written asm on every target. */
	inline unsigned long long byteorder_64(unsigned long long x)
	{
		return __builtin_bswap64(x);
	}

#elif defined WIN32
	inline unsigned __int16 byteorder_16(unsigned __int16 x)
	{
		__asm ror x, 8
		return x;
	}
	inline unsigned __int32 byteorder_32(unsigned __int32 x)
	{
		__asm mov eax, x
		__asm bswap eax
		__asm mov x, eax
		return x;
	}
	inline unsigned __int64 byteorder_64(unsigned __int64 x)
	{
		__asm mov rax, x
		__asm bswap rax
		__asm mov x,rax
		return x;
	}
#endif
};

#endif
