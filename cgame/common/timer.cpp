/*
	timer.cpp - abase::timer, the 50 ms resolution timer wheel used by the
	gameserver (cgame/include/timer.h).

	gs/world.cpp owns the one and only instance, `abase::timer g_timer(1000,
	300000)`: 1000 buckets and up to 300000 simultaneously armed timers.  The
	world_manager heartbeat is a plain timer (set_timer(1, 0, 0, ...)) whose
	callback posts one task per tick onto ONET::Thread::Pool, so everything
	that happens in the game world - skill sessions, regeneration, monster
	spawns, buff expiry - depends on this wheel ticking 20 times a second.

	Semantics implemented here, taken from the header and from the call sites
	in gs/actsession.cpp and gs/global_manager.cpp:

	  * a tick is TICKTIME microseconds (50 ms); interval and start_time are
	    in ticks (actsession converts milliseconds with "/= 50").
	  * times == 0 means "repeat forever"; times == n means "fire n times".
	    The callback receives the number of fires still to come after this
	    one, so a one shot timer is called with rtimes == 0.
	  * timer_task::DoTimer() drops the handle as soon as rtimes == 0, hence a
	    forever timer must never report 0: it reports -1, which keeps
	    ChangeIntervalInCallback() legal on the next call.
	  * the table is locked while callbacks run.  That is what makes the
	    nolock / callback_* entry points safe; the lock is recursive so the
	    same thread may still call the locking variants (RemoveTimer(),
	    SetTimer()) from inside a callback without deadlocking.
	  * entries are hashed by absolute due tick; a timer whose due time is
	    further away than the bucket table wraps around and is simply
	    postponed once per lap, which is why the constructor documents
	    idx_tab_size as "how far ahead the table can look".
*/

#include "timer.h"
#include "spinlock.h"
#include "interlocked.h"
#include "ASSERT.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>

namespace abase
{

/* ------------------------------------------------------------------ */
/* a spin lock that a thread may take again while it already owns it	*/
/* ------------------------------------------------------------------ */

namespace
{

class recursive_spinlock
{
	int		_lock;
	pthread_t	_owner;
	int		_count;

public:
	recursive_spinlock()
	{
		_lock = 0;
		pthread_mutex_t dummy;	/* keep pthread_t a plain value type */
		(void)dummy;
		memset(&_owner, 0, sizeof(_owner));
		_count = 0;
	}

	void lock()
	{
		if (_count > 0 && pthread_equal(_owner, pthread_self()))
		{
			++_count;
			return;
		}
		mutex_spinlock(&_lock);
		_owner = pthread_self();
		_count = 1;
	}

	void unlock()
	{
		ASSERT(_count > 0);
		if (--_count == 0)
		{
			memset(&_owner, 0, sizeof(_owner));
			mutex_spinunlock(&_lock);
		}
	}
};

class scoped_lock
{
	recursive_spinlock & _m;
public:
	explicit scoped_lock(recursive_spinlock & m) : _m(m) { _m.lock(); }
	~scoped_lock() { _m.unlock(); }
};

inline long long now_microseconds()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (long long)tv.tv_sec * 1000000LL + tv.tv_usec;
}

}

/* ------------------------------------------------------------------ */
/* the implementation behind timer::		__imp			*/
/* ------------------------------------------------------------------ */

class timer_imp
{
public:
	struct entry
	{
		int		index;		/* == position in the entry array */
		bool		used;
		int		interval;	/* ticks between two fires */
		int		times;		/* fires left, 0 == forever */
		int		next_tick;	/* absolute tick we fire on */
		timer_callback	routine;
		void *		object;
		int		bucket;		/* bucket we are linked in, -1 while firing */
		int		prev;
		int		next;
	};

	timer_imp(int idx_tab_size, int max_timer_count);
	~timer_imp();

	int		set_timer(int interval, int start_time, int times,
				timer_callback routine, void * obj);
	int		remove_timer(int index, void * obj);
	int		callback_remove_self(int index);
	int		get_next_interval(int index, void * obj, int * interval, int * rtimes);
	int		change_interval(int index, void * obj, int interval, bool nolock, bool at_once);
	int		get_timer_left(int index, int & interval);

	void		timer_tick();
	void		timer_thread(int ticktime, int mintime);
	void		stop_thread()	{ _stopped = true; }
	void		pause_thread()	{ _paused = true; }
	void		resume_thread()	{ _paused = false; }
	void		reset();

	unsigned int	get_tick() const { return (unsigned int)_current_tick; }

	int		free_count() const { return _free_top; }
	int		alloced_count() const { return _max_count - _free_top; }

private:
	void		link(entry & e);
	void		unlink(entry & e);
	void		release(entry & e);
	entry *	fetch(int index, void * obj);

	recursive_spinlock	_mutex;

	entry *	_entries;
	int *	_buckets;
	int *	_free_stack;
	int		_tab_size;
	int		_max_count;
	int		_free_top;
	int		_current_tick;

	volatile bool	_running;
	volatile bool	_paused;
	volatile bool	_stopped;
};

timer_imp::timer_imp(int idx_tab_size, int max_timer_count)
{
	_tab_size = idx_tab_size < 16 ? 16 : idx_tab_size;
	_max_count = max_timer_count < 16 ? 16 : max_timer_count;

	_entries = new entry[_max_count];
	_buckets = new int[_tab_size];
	_free_stack = new int[_max_count];

	for (int i = 0; i < _max_count; ++i)
	{
		entry & e = _entries[i];
		memset(&e, 0, sizeof(e));
		e.index = i;
		e.used = false;
		e.bucket = -1;
		e.prev = -1;
		e.next = -1;
		_free_stack[_max_count - 1 - i] = i;	/* first pop yields index 0 */
	}
	for (int b = 0; b < _tab_size; ++b)
		_buckets[b] = -1;

	_free_top = _max_count;
	_current_tick = 0;
	_running = false;
	_paused = false;
	_stopped = false;
}

timer_imp::~timer_imp()
{
	delete [] _entries;
	delete [] _buckets;
	delete [] _free_stack;
	_entries = NULL;
	_buckets = NULL;
	_free_stack = NULL;
}

timer_imp::entry * timer_imp::fetch(int index, void * obj)
{
	if (index < 0 || index >= _max_count) return NULL;
	entry & e = _entries[index];
	if (!e.used) return NULL;
	if (obj && e.object != obj) return NULL;	/* stale handle of somebody else */
	return &e;
}

void timer_imp::link(entry & e)
{
	int b = (int)(((unsigned int)e.next_tick) % (unsigned int)_tab_size);
	e.bucket = b;
	e.prev = -1;
	e.next = _buckets[b];
	if (_buckets[b] >= 0)
		_entries[_buckets[b]].prev = e.index;
	_buckets[b] = e.index;
}

void timer_imp::unlink(entry & e)
{
	if (e.bucket < 0)
	{
		e.prev = e.next = -1;
		return;
	}
	if (e.prev >= 0)
		_entries[e.prev].next = e.next;
	else if (_buckets[e.bucket] == e.index)
		_buckets[e.bucket] = e.next;
	if (e.next >= 0)
		_entries[e.next].prev = e.prev;
	e.bucket = -1;
	e.prev = e.next = -1;
}

void timer_imp::release(entry & e)
{
	unlink(e);
	e.used = false;
	e.routine = NULL;
	e.object = NULL;
	e.times = 0;
	e.interval = 0;
	e.next_tick = 0;
	if (_free_top < _max_count)
		_free_stack[_free_top++] = e.index;
}

int timer_imp::set_timer(int interval, int start_time, int times,
			 timer_callback routine, void * obj)
{
	if (!routine) return -1;
	if (interval < 1) interval = 1;
	if (times < 0) times = 0;

	int delay = (start_time < 0) ? interval : start_time;
	if (delay < 0) delay = 0;

	scoped_lock keeper(_mutex);
	if (_free_top <= 0)
	{
		fprintf(stderr, "timer: no free entry left (%d armed), timer dropped\n", _max_count);
		fflush(stderr);
		return -1;
	}

	entry & e = _entries[_free_stack[--_free_top]];
	e.used = true;
	e.interval = interval;
	e.times = times;
	e.routine = routine;
	e.object = obj;
	e.next_tick = _current_tick + (delay > 0 ? delay : 1);
	link(e);
	return e.index;
}

int timer_imp::remove_timer(int index, void * obj)
{
	scoped_lock keeper(_mutex);
	entry * pe = fetch(index, obj);
	if (!pe) return -1;
	release(*pe);
	return 0;
}

/*
	Only meaningful from inside OnTimer(): the entry is off the bucket list
	while its callback runs.  It is harmless anywhere else because the lock
	is recursive and unlink() tolerates an entry that is not linked.
*/
int timer_imp::callback_remove_self(int index)
{
	scoped_lock keeper(_mutex);
	entry * pe = fetch(index, NULL);
	if (!pe) return -1;
	release(*pe);
	return 0;
}

int timer_imp::get_next_interval(int index, void * obj, int * interval, int * rtimes)
{
	scoped_lock keeper(_mutex);
	entry * pe = fetch(index, obj);
	if (!pe) return -1;
	if (interval) *interval = pe->interval;
	if (rtimes) *rtimes = pe->times;
	return 0;
}

int timer_imp::get_timer_left(int index, int & interval)
{
	scoped_lock keeper(_mutex);
	entry * pe = fetch(index, NULL);
	if (!pe) return -1;
	interval = pe->interval;
	int left = pe->next_tick - _current_tick;
	return left < 0 ? 0 : left;
}

int timer_imp::change_interval(int index, void * obj, int interval, bool nolock, bool at_once)
{
	(void)nolock;		/* the lock is recursive, taking it is always correct */

	scoped_lock keeper(_mutex);
	entry * pe = fetch(index, obj);
	if (!pe) return -1;

	if (interval < 1) interval = 1;
	pe->interval = interval;

	if (pe->bucket < 0)
	{
		/* called from the callback of our own tick: re-arm it */
		pe->next_tick = _current_tick + (at_once ? 1 : interval);
		link(*pe);
	}
	else
	{
		unlink(*pe);
		pe->next_tick = _current_tick + (at_once ? 1 : interval);
		link(*pe);
	}
	return 0;
}

void timer_imp::timer_tick()
{
	scoped_lock keeper(_mutex);

	++_current_tick;

	int idx = _buckets[(unsigned int)_current_tick % (unsigned int)_tab_size];
	while (idx >= 0)
	{
		entry & e = _entries[idx];
		const int next = e.next;	/* the callback may relink this entry */

		if (e.next_tick != _current_tick)
		{
			idx = next;		/* wrapped around, wait for the next lap */
			continue;
		}

		unlink(e);			/* e.bucket = -1 while it runs */

		int remain;
		if (e.times > 0)
		{
			--e.times;
			remain = e.times;	/* 0 on the very last fire */
		}
		else
		{
			remain = -1;		/* forever: never 0, see the header note */
		}

		timer_callback routine = e.routine;
		void * object = e.object;
		const int index = e.index;

		if (routine) routine(index, object, remain);

		if (remain == 0)
		{
			if (e.used) release(e);	/* the callback may have removed it */
		}
		else if (e.used && e.bucket < 0)
		{
			/*
			 * still armed and nobody re-armed it from the callback
			 * (ChangeIntervalInCallback does that itself)
			 */
			e.next_tick = _current_tick + (e.interval > 0 ? e.interval : 1);
			link(e);
		}

		idx = next;
	}
}

void timer_imp::reset()
{
	scoped_lock keeper(_mutex);

	for (int b = 0; b < _tab_size; ++b)
		_buckets[b] = -1;
	for (int i = 0; i < _max_count; ++i)
	{
		entry & e = _entries[i];
		e.used = false;
		e.routine = NULL;
		e.object = NULL;
		e.times = 0;
		e.interval = 0;
		e.next_tick = 0;
		e.bucket = -1;
		e.prev = e.next = -1;
		_free_stack[i] = _max_count - 1 - i;
	}
	_free_top = _max_count;
}

void timer_imp::timer_thread(int ticktime, int mintime)
{
	if (ticktime <= 0) ticktime = timer::TICKTIME;
	if (mintime <= 0 || mintime > ticktime) mintime = timer::MINTIME;

	_stopped = false;
	_paused = false;
	_running = true;

	const long long tick_us = ticktime;
	long long base = now_microseconds() - (long long)get_tick() * tick_us;

	while (!_stopped)
	{
		if (_paused)
		{
			/* do not let the schedule run away while we are paused */
			usleep(mintime);
			base = now_microseconds() - (long long)get_tick() * tick_us;
			continue;
		}

		const long long due = base + ((long long)get_tick() + 1) * tick_us;
		long long now = now_microseconds();
		while (now < due && !_stopped)
		{
			long long left = due - now;
			if (left > mintime) left = mintime;
			usleep((useconds_t)left);
			now = now_microseconds();
		}
		if (_stopped) break;

		timer_tick();

		/*
		 * Catch up on lost ticks, but never in a storm: after a long
		 * suspend (laptop, stop+kill -CONT) jumping the tick counter is
		 * better than firing thousands of queued timers at once.
		 */
		now = now_microseconds();
		long long behind = (now - (base + (long long)get_tick() * tick_us)) / tick_us;
		if (behind > 0)
		{
			if (behind > 8)
			{
				scoped_lock keeper(_mutex);
				_current_tick += (int)behind;
			}
			else
			{
				for (long long i = 0; i < behind && !_stopped; ++i)
					timer_tick();
			}
		}
	}

	_running = false;
}

/* ------------------------------------------------------------------ */
/* the public shell: every method just forwards to the implementation	*/
/* ------------------------------------------------------------------ */

timer::timer(int idx_tab_size, int max_timer_count)
{
	__imp = new timer_imp(idx_tab_size, max_timer_count);
}

timer::~timer()
{
	timer_imp * p = (timer_imp *)__imp;
	if (p) delete p;
	__imp = NULL;
}

unsigned int timer::get_tick()
{
	return ((timer_imp *)__imp)->get_tick();
}

long timer::get_systime()
{
	return (long)time(NULL);
}

void timer::get_systime(timeval & tv)
{
	gettimeofday(&tv, NULL);
}

int timer::get_free_timer_count()
{
	return ((timer_imp *)__imp)->free_count();
}

int timer::get_timer_total_alloced()
{
	return ((timer_imp *)__imp)->alloced_count();
}

int timer::get_timer_left(int index, int & interval)
{
	return ((timer_imp *)__imp)->get_timer_left(index, interval);
}

int timer::set_timer(int interval, int start_time, int times, timer_callback routine, void * obj)
{
	return ((timer_imp *)__imp)->set_timer(interval, start_time, times, routine, obj);
}

int timer::remove_timer(int index, void * object)
{
	return ((timer_imp *)__imp)->remove_timer(index, object);
}

int timer::callback_remove_self(int index)
{
	return ((timer_imp *)__imp)->callback_remove_self(index);
}

int timer::get_next_interval(int index, void * obj, int * interval, int * rtimes)
{
	return ((timer_imp *)__imp)->get_next_interval(index, obj, interval, rtimes);
}

int timer::change_interval(int index, void * obj, int interval, bool nolock)
{
	return ((timer_imp *)__imp)->change_interval(index, obj, interval, nolock, false);
}

int timer::change_interval_at_once(int index, void * obj, int interval, bool nolock)
{
	return ((timer_imp *)__imp)->change_interval(index, obj, interval, nolock, true);
}

void timer::timer_thread(int ticktime, int mintime)
{
	((timer_imp *)__imp)->timer_thread(ticktime, mintime);
}

void timer::stop_thread()
{
	((timer_imp *)__imp)->stop_thread();
}

void timer::pause_thread()
{
	((timer_imp *)__imp)->pause_thread();
}

void timer::resume_thread()
{
	((timer_imp *)__imp)->resume_thread();
}

void timer::reset()
{
	((timer_imp *)__imp)->reset();
}

void timer::timer_tick()
{
	((timer_imp *)__imp)->timer_tick();
}

}
