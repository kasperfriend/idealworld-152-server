/*
	interlocked.cpp - atomic helpers of the cmlib (abase) support library.

	cgame/include/interlocked.h declares these out of line unless the user
	defines __INTERLOCKED_USE_INLINE__, which the engine build does not.  The
	inline x86-32 variant in the header cannot be used on x86-64 at all (its
	"=A" output constraint is a 32-bit only thing), so this file is the real
	implementation for the 64 bit build.

	Semantics taken from the inline variant in the header: every function
	returns the NEW value of the counter, exactly like the gcc/Intel
	"lock xaddl" wrapper there (old + delta / old - delta).
*/

#include "interlocked.h"

extern "C"
{

int interlocked_increment(int * value)
{
	return __sync_add_and_fetch(value, 1);
}

int interlocked_decrement(int * value)
{
	return __sync_add_and_fetch(value, -1);
}

int interlocked_add(int * value, int addval)
{
	return __sync_add_and_fetch(value, addval);
}

int interlocked_sub(int * value, int subval)
{
	return __sync_sub_and_fetch(value, subval);
}

}
