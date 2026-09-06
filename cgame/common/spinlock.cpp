/*
	spinlock.cpp - the spin locks of the cmlib (abase) support library.

	The engine build defines __THREAD_SPIN_LOCK__ and every ONET::Thread::Mutex
	(and abase::fast_allocator's per size buckets, the timer table, the random
	generator, ...) funnels down to these five entry points, so they are the
	only locking primitives the gameserver really uses.

	A lock is a plain `int`: 0 == free, non zero == taken.  mutex_spinset() is
	the raw test-and-set used by code that wants to skip work while somebody
	else holds the lock, mutex_spinlock2() is the quiet variant used inside
	loops that must not log (it also reports how many rounds it spun),
	mutex_spinlock() is the reporting variant and mutex_spinwait() gives up
	after a timeout instead of hanging forever.

	Contention counter: abase::fast_allocator / ONET::Thread::Pool users read
	ONET::__thread_lock_count every minute ("thread_lock_count(60sec)" in
	gs/start.cpp) to spot hot locks, so only *contended* acquisitions are
	counted there - a plain increment on every lock would just measure how
	often we lock.
*/

#include "spinlock.h"
#include "interlocked.h"

#include <sched.h>
#include <stdio.h>
#include <time.h>

namespace ONET
{
	extern int __thread_lock_count;
}

/* spin a little before touching the scheduler; ~2us on a modern core */
#define SPIN_ROUNDS	64

/* after this many rounds of full spinning we start yielding */
#define YIELD_ROUNDS	64

extern "C"
int mutex_spinset(int * __spinlock)
{
	/* returns the previous state: non zero means "already locked" */
	return __sync_lock_test_and_set(__spinlock, 1);
}

extern "C"
void mutex_spinunlock(int * __spinlock)
{
	__sync_lock_release(__spinlock);
}

extern "C"
int mutex_spinlock2(int * __spinlock)
{
	int spins = 0;
	while (__sync_lock_test_and_set(__spinlock, 1))
	{
		++spins;
		int round = 0;
		while (*__spinlock)
		{
			if (++round >= SPIN_ROUNDS)
			{
				round = 0;
				if ((spins % YIELD_ROUNDS) == 0)
					sched_yield();
				else
					__asm__ __volatile__("" ::: "memory");
			}
		}
	}
	if (spins)
		interlocked_add(&ONET::__thread_lock_count, spins);
	return spins;
}

extern "C"
void mutex_spinlock(int * __spinlock)
{
	int spins = 0;
	while (__sync_lock_test_and_set(__spinlock, 1))
	{
		++spins;
		int round = 0;
		while (*__spinlock)
		{
			if (++round >= SPIN_ROUNDS)
			{
				round = 0;
				if ((spins % YIELD_ROUNDS) == 0)
					sched_yield();
				else
					__asm__ __volatile__("" ::: "memory");
			}
		}
	}

	if (spins >= (1 << 14))
	{
		/*
			Spinning this long almost always means somebody forgot to
			unlock.  Say so once in a while, but keep spinning - the
			original library never aborted on a stuck lock and the
			engines rely on that.
		*/
		fprintf(stderr, "warning: mutex_spinlock spun %d rounds on lock %p\n",
			spins, (void *)__spinlock);
		fflush(stderr);
	}
}

extern "C"
int mutex_spinwait(int * __spinlock, int __timeout)
{
	/* returns 0 once the lock is held, non zero when it timed out */
	struct timespec deadline;
	clock_gettime(CLOCK_MONOTONIC, &deadline);
	long long left_ns = (long long)__timeout * 1000000LL;
	deadline.tv_sec += (time_t)(left_ns / 1000000000LL);
	deadline.tv_nsec += (long)(left_ns % 1000000000LL);
	if (deadline.tv_nsec >= 1000000000L)
	{
		deadline.tv_nsec -= 1000000000L;
		deadline.tv_sec += 1;
	}

	for (;;)
	{
		if (!mutex_spinset(__spinlock)) return 0;

		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		if (now.tv_sec > deadline.tv_sec ||
			(now.tv_sec == deadline.tv_sec && now.tv_nsec >= deadline.tv_nsec))
		{
			interlocked_add(&ONET::__thread_lock_count, 1);
			return -1;
		}

		int round = 0;
		while (*__spinlock)
		{
			if (++round >= SPIN_ROUNDS)
			{
				round = 0;
				sched_yield();
			}
		}
	}
}
