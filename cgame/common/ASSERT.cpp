/*
	ASSERT.cpp - ASSERT_FAIL() for the cmlib (abase) support library.

	cgame/include/ASSERT.h expands a failed assertion into

		(void)((expr) ? 1 : (*ASSERT_FAIL(#expr, __FILE__, __LINE__) = 0))

	so ASSERT_FAIL() has to return the address of a writable int (the macro
	dereferences it) and it must NOT abort the process: the engines keep the
	server running after an assertion fires and only log the failure.  That is
	deliberate - gs touches player data all the time and a crashed game server
	corrupts more than a logged assert does.
*/

#include "ASSERT.h"

#include <stdio.h>

static int assert_fail_sink = 0;	/* the int the macro writes its 0 into */

extern "C"
int * ASSERT_FAIL(const char * msg, const char * filename, int line)
{
	fprintf(stderr, "ASSERT FAILED: %s (%s:%d)\n",
		msg ? msg : "", filename ? filename : "?", line);
	fflush(stderr);
	return &assert_fail_sink;
}
