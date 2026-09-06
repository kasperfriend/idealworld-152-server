/*
	cmtest.cpp - runtime self test for the cmlib support library.

	cgame/common was written from the declarations in cgame/include because
	the prebuilt 32 bit libcommon.a that shipped with this drop cannot be
	linked into a 64 bit gameserver.  A successful link is not proof that the
	implementations do what the engine expects, so this test exercises the
	contract each piece has to honour:

		crc32 / custom_crc32 / crc16, base64, abase::strtok, ARandomGen,
		abase::fast_allocator, ONET::Conf, ONET::Thread::Pool, the spin
		locks and abase::timer (one shot, forever, re-arm and self remove
		from inside a callback, all of them driven by the timer thread).

	Run it with:  make -C cgame/common test
*/

#include "ASSERT.h"
#include "amemory.h"
#include "arandomgen.h"
#include "base64.h"
#include "conf.h"
#include "crc.h"
#include "interlocked.h"
#include "spinlock.h"
#include "strtok.h"
#include "threadpool.h"
#include "timer.h"
#include "verbose.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* gs/start.cpp declares these the same way; they live in threadpool.cpp */
namespace ONET
{
	extern int __thread_lock_count;
	extern int __thread_task_count;
}

static int failures = 0;
static int checks = 0;

#define CHECK(cond, ...)	do { ++checks; if (!(cond)) { ++failures; \
		printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); \
		printf("\n"); fflush(stdout); } } while (0)

static void msleep(int ms)
{
	struct timespec ts;
	ts.tv_sec = ms / 1000;
	ts.tv_nsec = (long)(ms % 1000) * 1000000L;
	nanosleep(&ts, NULL);
}

/* ------------------------------- crc / base64 ----------------------- */

static void test_crc()
{
	const char * v = "123456789";

	/* CRC-32 of the standard check word, the value zlib reports as well */
	CHECK(crc32(v, 9) == 0xCBF43926u, "crc32(\"123456789\") = %08x", crc32(v, 9));
	CHECK(crc32(NULL, 10) == 0, "empty crc32 must be 0");

	/* the incremental form has to agree with the one shot form */
	unsigned state = 0xFFFFFFFFu;
	for (int i = 0; i < 9; ++i)
		custom_crc32(&state, v + i, 1);
	CHECK(crc32(v, 9) == ~state, "custom_crc32 must agree with crc32");

	/* crc16 stamps item / equipment blobs that live in the database */
	const unsigned char * b = (const unsigned char *)v;
	unsigned short c1 = crc16(b, 9);
	CHECK(c1 == crc16(b, 9), "crc16 not deterministic");
	CHECK(crc16(b, 0) == 0xFFFF, "crc16 of nothing is the init value, got 0x%04x", (unsigned)crc16(b, 0));
	printf("crc32=0x%08x crc16=0x%04x\n", crc32(v, 9), (unsigned)c1);
	fflush(stdout);
}

static void test_base64()
{
	char enc[256];
	unsigned char dec[256];

	int n = base64_encode((unsigned char *)"any carnal pleasure", 19, enc);
	CHECK(n == 28, "encoded length %d", n);
	CHECK(strcmp(enc, "YW55IGNhcm5hbCBwbGVhc3VyZQ==") == 0, "encoded '%s'", enc);

	int m = base64_decode(enc, n, dec);
	CHECK(m == 19, "decoded length %d", m);
	CHECK(memcmp(dec, "any carnal pleasure", 19) == 0, "round trip mismatch");

	srand(12345);
	for (int round = 0; round < 200; ++round)
	{
		unsigned char raw[64];
		int len = rand() % 64;
		for (int i = 0; i < len; ++i) raw[i] = (unsigned char)(rand() & 0xff);
		char buf[256];
		int written = base64_encode(raw, len, buf);
		CHECK(written == ((len + 2) / 3) * 4, "length for %d bytes: %d", len, written);
		unsigned char back[256];
		int got = base64_decode(buf, written, back);
		CHECK(got == len, "decoded %d of %d", got, len);
		CHECK(len == 0 || memcmp(back, raw, len) == 0, "round trip byte mismatch");
	}

	char junk[5] = "a!bc";
	unsigned char out[8];
	CHECK(base64_decode(junk, 4, out) == -1, "a character outside the alphabet must be rejected");
}

/* ------------------------------- abase::strtok ------------------------ */

static void test_strtok()
{
	abase::strtok tok("1;2,,3", ";,");
	const char * t;
	int count = 0;
	const char * expect[4] = { "1", "2", "", "3" };
	while ((t = tok.token()) != NULL)
	{
		CHECK(count < 4, "too many tokens");
		if (count < 5) CHECK(strcmp(t, expect[count]) == 0, "token %d = '%s'", count, t);
		++count;
	}
	CHECK(count == 4, "token count %d", count);
	CHECK(tok.offset() == -1, "offset after exhaustion must be -1");

	/* the engine reuses the 128 byte scratch buffer, so it must be honoured */
	char big[400];
	memset(big, 'a', sizeof(big) - 1);
	big[sizeof(big) - 1] = 0;
	abase::strtok tok2(big, " ");
	const char * t2 = tok2.token();
	CHECK(t2 && strlen(t2) <= 127, "long token not truncated: %u", t2 ? (unsigned)strlen(t2) : 0u);

	abase::strtok tok3;
	tok3.reset("one two three", " ");
	char out[16];
	int seen = 0;
	while (tok3.token(out, sizeof(out))) ++seen;
	CHECK(seen == 3, "space separated count %d", seen);

	/* the delimiter set the world list uses, including \r\n endings */
	abase::strtok tok4("10\r\n20\r\n", ";,\r\n");
	int numbers[4] = { 0, 0, 0, 0 };
	int i = 0;
	while (i < 4 && (t2 = tok4.token()) != NULL)
	{
		if (!*t2) continue;
		numbers[i++] = atoi(t2);
	}
	CHECK(numbers[0] == 10 && numbers[1] == 20, "crlf split gave %d,%d", numbers[0], numbers[1]);
}

/* ------------------------------- ARandomGen --------------------------- */

static void test_random()
{
	ARandomGen ar(12345u);
	double sum = 0.0;
	const int n = 200000;
	int outside = 0;
	for (int i = 0; i < n; ++i)
	{
		double u = ar.RandomUniform();
		if (!(u > 0.0 && u < 1.0)) { ++outside; continue; }
		sum += u;
	}
	CHECK(outside == 0, "%d values outside (0,1)", outside);
	double mean = sum / n;
	CHECK(fabs(mean - 0.5) < 0.01, "mean = %f", mean);

	int bad = 0;
	int hist[5] = { 0, 0, 0, 0, 0 };
	for (int i = 0; i < 20000; ++i)
	{
		int v = ar.RandomInt(3, 7);		/* bounds are inclusive */
		if (v < 3 || v > 7) ++bad; else ++hist[v - 3];
	}
	CHECK(bad == 0, "RandomInt out of range");
	for (int k = 0; k < 5; ++k)
		CHECK(hist[k] > 2000, "bucket %d of RandomInt got %d draws", k, hist[k]);

	/* the shared generator, used through the inline helpers of arandomgen.h */
	int r = abase::Rand(1, 10);
	CHECK(r >= 1 && r <= 10, "abase::Rand = %d", r);
	double g = ar.RandomGaussian(100.0, 10.0);
	CHECK(g > 20.0 && g < 180.0, "gaussian outlier %f", g);

	/* the same seed has to give the same sequence: drop tables rely on it */
	ARandomGen a1(777u), a2(777u);
	for (int i = 0; i < 100; ++i)
		CHECK(a1.RandomUniform() == a2.RandomUniform(), "same seed, different sequence");
}

/* ------------------------------- allocator ---------------------------- */

static void test_allocator()
{
	using namespace abase;

	void * p1 = fast_allocator::alloc(64);
	void * p2 = fast_allocator::alloc(1000);
	void * p3 = fast_allocator::alloc(2048);
	CHECK(p1 && p2 && p3, "alloc returned NULL");
	memset(p1, 1, 64);
	memset(p2, 2, 1000);
	memset(p3, 3, 2048);
	CHECK(*(unsigned char *)p1 == 1 && *(unsigned char *)p2 == 2 && *(unsigned char *)p3 == 3,
		"returned memory is not usable");

	/* the aligned path and the small object path every ASmallObject uses */
	void * s = fast_allocator::align_alloc(63);
	CHECK(s != NULL && ((size_t)s % 4) == 0, "align_alloc");
	memset(s, 4, 63);
	fast_allocator::align_free(s, 63);
	void * s4 = fast_allocator::align_alloc(13, 16);
	CHECK(s4 != NULL && ((size_t)s4 % 16) == 0, "align_alloc with alignment");
	fast_allocator::align_free(s4, 13, 16);

	void * t = fast_alloc<1, 128>::allocate(40);
	CHECK(t != NULL, "fast_alloc<1,128>");
	memset(t, 5, 40);
	fast_alloc<1, 128>::deallocate(t, 40);

	void * u = abase::fastalloc(100);
	CHECK(u != NULL, "fastalloc");
	abase::fastfree(u, 100);

	fast_allocator::free(p1, 64);
	fast_allocator::free(p2, 1000);
	fast_allocator::free(p3, 2048);

	int leaks = 0;
	for (int i = 0; i < 20000; ++i)
	{
		size_t sz = 32 + (size_t)(i % 300);
		void * q = fast_allocator::alloc(sz);
		if (!q) { ++leaks; break; }
		memset(q, i & 0xff, sz);
		fast_allocator::free(q, sz);
	}
	CHECK(leaks == 0, "alloc/free churn failed");

	/* realloc keeps the prefix of the old block */
	void * r1 = fast_allocator::alloc(32);
	memset(r1, 0x41, 32);
	void * r2 = fast_allocator::realloc(r1, 64, 32);
	CHECK(r2 != NULL && memcmp(r2, r1, 0) == 0, "realloc");
	CHECK(((unsigned char *)r2)[0] == 0x41, "realloc lost the old content");
	fast_allocator::free(r2, 64);
}

/* ------------------------------- thread pool -------------------------- */

static int g_task_run = 0;

class count_task : public ONET::Thread::Runnable
{
public:
	virtual void Run() { interlocked_increment(&g_task_run); delete this; }
};

static void test_threadpool()
{
	ONET::Thread::Pool::CreatePool(4);
	const int total = 500;
	for (int i = 0; i < total; ++i)
		ONET::Thread::Pool::AddTask(new count_task());

	int waited = 0;
	while (ONET::Thread::Pool::QueueSize() > 0 && waited < 10000) { msleep(10); waited += 10; }
	msleep(50);
	CHECK(g_task_run == total, "ran %d of %d tasks", g_task_run, total);
	CHECK(ONET::Thread::Pool::GetTotalTaskCount() >= (size_t)total, "total task counter");
	CHECK(ONET::Thread::Pool::QueueSize() == 0, "queue must be empty again");
	CHECK(ONET::Thread::Pool::Size() == 4, "pool size");
	ONET::Thread::Pool::Dump();
}

/* ------------------------------- spin locks --------------------------- */

static int g_spin_counter = 0;
static int g_spin_done = 0;
static int g_spin_lock = 0;

class spin_task : public ONET::Thread::Runnable
{
public:
	virtual void Run()
	{
		for (int i = 0; i < 20000; ++i)
		{
			mutex_spinlock(&g_spin_lock);
			++g_spin_counter;
			mutex_spinunlock(&g_spin_lock);
		}
		interlocked_increment(&g_spin_done);
		delete this;
	}
};

static void test_spinlock()
{
	const int threads = 4;
	for (int i = 0; i < threads; ++i)
		ONET::Thread::Pool::AddTask(new spin_task());
	int waited = 0;
	while (g_spin_done < threads && waited < 30000) { msleep(20); waited += 20; }
	CHECK(g_spin_done == threads, "only %d of %d spin threads finished", g_spin_done, threads);
	CHECK(g_spin_counter == threads * 20000, "lost updates: %d", g_spin_counter);

	int lock = 0;
	CHECK(mutex_spinset(&lock) == 0, "first test and set must succeed");
	CHECK(mutex_spinset(&lock) != 0, "second test and set must report the lock");
	mutex_spinunlock(&lock);
	CHECK(mutex_spinset(&lock) == 0, "lock must be free again");
	mutex_spinunlock(&lock);
	CHECK(mutex_spinwait(&lock, 10) == 0, "uncontended timed wait");
	mutex_spinunlock(&lock);
	CHECK(mutex_spinlock2(&lock) >= 0, "spinlock2");
	mutex_spinunlock(&lock);

	/* the autolock wrapper used all over amemory.h / threadpool.h */
	{
		spin_autolock keeper(lock);
		CHECK(keeper.is_attached(), "autolock must be attached");
	}
	CHECK(mutex_spinset(&lock) == 0, "autolock must have released the lock");
	mutex_spinunlock(&lock);

	/* the interlocked helpers are the other half of the lock primitives */
	int counter = 0;
	CHECK(interlocked_increment(&counter) == 1, "increment returns the new value");
	CHECK(interlocked_add(&counter, 41) == 42, "add returns the new value");
	CHECK(interlocked_sub(&counter, 42) == 0, "sub returns the new value");
	CHECK(interlocked_decrement(&counter) == -1, "decrement returns the new value");
	CHECK(ONET::__thread_lock_count >= 0, "lock contention counter exists");
}

/* ------------------------------- Conf --------------------------------- */

static void test_conf()
{
	const char * path = "cmtest.conf";
	FILE * f = fopen(path, "w");
	CHECK(f != NULL, "cannot write %s", path);
	if (!f) return;
	fprintf(f,
		"# comment line\n"
		"; another comment\n"
		"[ General ]\n"
		"logic_threads = 6\n"
		"instance_servers = 1;2\n"
		"\n"
		"[Instance_1]\n"
		"  tag   = 11 \n"
		"name=alpha\n");
	fclose(f);

	ONET::Conf * conf = ONET::Conf::GetInstance(path);
	CHECK(conf != NULL, "GetInstance");
	CHECK(atoi(conf->find("General", "logic_threads").c_str()) == 6, "General/logic_threads");
	CHECK(conf->find("General", "instance_servers") == "1;2", "instance_servers");
	CHECK(atoi(conf->find("Instance_1", "tag").c_str()) == 11, "Instance_1/tag");
	CHECK(conf->find("Instance_1", "name") == "alpha", "Instance_1/name");
	/* sections and keys are compared with strcasecmp in this parser */
	CHECK(conf->find("general", "LOGIC_THREADS") == "6", "case insensitive lookup");
	/* gs reads missing keys as an empty string instead of crashing */
	CHECK(conf->find("General", "no_such_key").empty(), "missing key must be empty");

	const char * extra = "cmtest2.conf";
	f = fopen(extra, "w");
	if (f)
	{
		fprintf(f, "[Instance_2]\ntag = 22\n[General]\nlogic_threads = 9\n");
		fclose(f);
		ONET::Conf::AppendConfFile(extra);
		CHECK(atoi(conf->find("Instance_2", "tag").c_str()) == 22, "appended section");
		CHECK(atoi(conf->find("General", "logic_threads").c_str()) == 9, "appended value wins");
		remove(extra);
	}

	/* touching the file makes reload() re-read it, which is how ops reload */
	msleep(1100);			/* st_mtime only has a one second resolution */
	f = fopen(path, "w");
	if (f)
	{
		fprintf(f, "[General]\nlogic_threads = 12\n");
		fclose(f);
		conf = ONET::Conf::GetInstance(path);	/* this is what re-reads it */
		CHECK(atoi(conf->find("General", "logic_threads").c_str()) == 12, "reload after edit");
	}
	remove(path);
}

/* ------------------------------- timer ------------------------------- */

static int g_shot = 0;
static int g_shot_remain = -12345;
static int g_period = 0;
static int g_zero_remain = 0;
static int g_rearm = 0;
static int g_selfremove = 0;

static abase::timer * g_shared_timer = NULL;

static void shot_once(int index, void * obj, int remain)
{
	(void)index; (void)obj;
	++g_shot;
	g_shot_remain = remain;
}

static void shot_period(int index, void * obj, int remain)
{
	(void)index; (void)obj;
	if (remain == 0) ++g_zero_remain;	/* must never happen for a forever timer */
	++g_period;
}

static void shot_rearm(int index, void * obj, int remain)
{
	(void)remain;
	++g_rearm;
	abase::timer * tm = static_cast<abase::timer *>(obj);
	/* legal because the table is locked while a callback runs */
	tm->change_interval(index, obj, 2, true);
}

static void shot_selfremove(int index, void * obj, int remain)
{
	(void)remain;
	++g_selfremove;
	abase::timer * tm = static_cast<abase::timer *>(obj);
	tm->callback_remove_self(index);
}

static void shot_stopworld(int index, void * obj, int remain)
{
	(void)index; (void)remain;
	++g_period;
	abase::timer * tm = static_cast<abase::timer *>(obj);
	tm->stop_thread();
}

/* driven by hand: no thread, so the numbers below are exact */
static void test_timer()
{
	abase::timer tm(64, 4096);

	CHECK(tm.get_free_timer_count() == 4096, "a fresh table must have every slot free");
	int free_before = tm.get_free_timer_count();

	/* one shot: start_time is the delay in ticks before the first fire */
	int idx = tm.set_timer(1, 3, 1, shot_once, NULL);
	CHECK(idx >= 0, "set_timer returned %d", idx);
	for (int i = 0; i < 2; ++i) tm.timer_tick();	/* ticks 1 and 2: not due */
	CHECK(g_shot == 0, "fired before start_time elapsed (%d)", g_shot);
	tm.timer_tick();				/* tick 3: due */
	CHECK(g_shot == 1, "one shot fired %d times", g_shot);
	CHECK(g_shot_remain == 0, "the last fire must report rtimes == 0, got %d", g_shot_remain);
	tm.timer_tick();
	tm.timer_tick();
	CHECK(g_shot == 1, "one shot fired again");
	CHECK(tm.get_free_timer_count() == free_before, "slot not recycled (%d/%d)",
		tm.get_free_timer_count(), free_before);
	CHECK(tm.remove_timer(idx, NULL) == -1, "removing a finished timer must fail");

	/* times == 1 armed with no explicit start time also fires exactly once */
	g_shot = 0;
	tm.set_timer(2, -1, 1, shot_once, NULL);
	for (int i = 0; i < 2; ++i) tm.timer_tick();
	CHECK(g_shot == 1, "default start_time: fired %d times", g_shot);
	CHECK(g_shot_remain == 0, "default start_time remain %d", g_shot_remain);

	/* a finite timer counts its remaining fires down */
	g_shot = 0;
	tm.set_timer(1, 1, 3, shot_once, NULL);
	for (int i = 0; i < 5; ++i) tm.timer_tick();
	CHECK(g_shot == 3, "three fire timer ran %d times", g_shot);
	CHECK(g_shot_remain == 0, "last remain %d", g_shot_remain);
	CHECK(tm.get_free_timer_count() == free_before, "finite timer leaked its slot");

	/* forever: keeps firing, never reports 0 (timer_task keeps its handle) */
	g_period = 0;
	g_zero_remain = 0;
	int idx_period = tm.set_timer(1, 1, 0, shot_period, NULL);
	CHECK(idx_period >= 0, "periodic set_timer failed");
	for (int i = 0; i < 10; ++i) tm.timer_tick();
	CHECK(g_period == 10, "forever timer fired %d of 10 ticks", g_period);
	CHECK(g_zero_remain == 0, "a forever timer must never report rtimes == 0");
	CHECK(tm.get_timer_total_alloced() == 1, "one armed timer, got %d", tm.get_timer_total_alloced());

	int interval = 0, rtimes = 0;
	CHECK(tm.get_next_interval(idx_period, NULL, &interval, &rtimes) == 0, "get_next_interval");
	CHECK(interval == 1 && rtimes == 0, "interval %d rtimes %d", interval, rtimes);
	int left = -1;
	CHECK(tm.get_timer_left(idx_period, left) == 1, "one tick left, got %d", tm.get_timer_left(idx_period, left));

	/* remove_timer from the "outside" with the object as the guard */
	CHECK(tm.remove_timer(idx_period, &tm) == -1, "wrong object must be refused");
	CHECK(tm.remove_timer(idx_period, NULL) == 0, "remove_timer");
	int armed = g_period;
	for (int i = 0; i < 4; ++i) tm.timer_tick();
	CHECK(g_period == armed, "a removed timer kept firing");
	CHECK(tm.get_free_timer_count() == free_before, "remove_timer did not free the slot");

	/* a long interval: the bucket table is 64 wide, so this wraps and spills */
	g_shot = 0;
	tm.set_timer(1, 200, 1, shot_once, NULL);
	for (int i = 0; i < 199; ++i) tm.timer_tick();
	CHECK(g_shot == 0, "fired too early (%d)", g_shot);
	tm.timer_tick();
	CHECK(g_shot == 1, "wrapped timer did not fire (%d)", g_shot);
	CHECK(tm.get_free_timer_count() == free_before, "wrapped timer leaked its slot");

	/* out of slots has to be refused instead of corrupting the table */
	int maxc = 4096;
	for (int i = 0; i < maxc; ++i)
		CHECK(tm.set_timer(1, 1000 + i, 0, shot_period, NULL) >= 0, "arming %d", i);
	CHECK(tm.get_free_timer_count() == 0, "table must be full");
	CHECK(tm.set_timer(1, 1, 1, shot_once, NULL) == -1, "an overfull table must return -1");
	tm.reset();
	CHECK(tm.get_free_timer_count() == maxc, "reset must free every slot (%d)", tm.get_free_timer_count());
}

static void timer_driver()
{
	g_shared_timer->timer_thread();
}

/* the world runs it from a thread, which is where the races show up */
static void test_timer_thread()
{
	static abase::timer tm(64, 4096);
	g_shared_timer = &tm;

	int idx_period = tm.set_timer(1, 1, 0, shot_period, NULL);
	int idx_rearm = tm.set_timer(2, 1, 0, shot_rearm, &tm);
	int idx_self = tm.set_timer(1, 2, 0, shot_selfremove, &tm);
	CHECK(idx_period >= 0 && idx_rearm >= 0 && idx_self >= 0, "arming three timers");

	int free_before = tm.get_free_timer_count();
	ONET::Thread::Pool::CreateThread(timer_driver);
	/*
	 * 1.5 s at 50 ms a tick is ~30 ticks.  The margins are deliberately
	 * wide: this also runs on busy CI runners, and what matters here is
	 * that the wheel keeps ticking and that both callback styles survive
	 * running next to a ticking thread - not the exact count.
	 */
	msleep(1500);
	CHECK(g_period > 8, "forever timer fired %d times in 1500ms", g_period);
	CHECK(g_rearm > 2, "re-armed timer fired %d times", g_rearm);
	CHECK(g_selfremove == 1, "callback_remove_self ran %d times", g_selfremove);
	CHECK(g_zero_remain == 0, "forever timer reported rtimes 0 while the thread ran");
	CHECK(tm.get_free_timer_count() == free_before + 1,
		"callback_remove_self must free exactly one slot (%d -> %d)",
		free_before, tm.get_free_timer_count());
	CHECK(tm.get_timer_total_alloced() == 2, "2 timers left, got %d", tm.get_timer_total_alloced());

	/* interval changes from another thread while the timer thread ticks */
	for (int i = 0; i < 200; ++i)
	{
		tm.change_interval(idx_period, NULL, (i % 4) + 1, false);
		tm.change_interval_at_once(idx_period, NULL, (i % 3) + 1, false);
	}
	int interval = 0;
	CHECK(tm.get_timer_left(idx_period, interval) >= 0, "timer still alive after 200 changes");

	/* tick() may also be called from another thread than the timer thread */
	unsigned int tick_before = tm.get_tick();
	tm.timer_tick();
	CHECK(tm.get_tick() == tick_before + 1 || tm.get_tick() > tick_before,
		"get_tick must advance (%u -> %u)", tick_before, tm.get_tick());

	long systime = tm.get_systime();
	CHECK(systime > 1400000000L, "get_systime must be unix seconds, got %ld", systime);
	struct timeval tv;
	tm.get_systime(tv);
	CHECK((long)tv.tv_sec == systime || (long)tv.tv_sec + 1 == systime,
		"get_systime(timeval&) must agree with get_systime()");

	/* the heartbeat style timer used by global_manager.cpp: one tick, forever */
	g_shot = 0;
	int idx_shot = tm.set_timer(1, 0, 1, shot_stopworld, &tm);
	CHECK(idx_shot >= 0, "set_timer for the stop test");
	(void)idx_shot;
	msleep(300);				/* the callback stops the timer thread */
	CHECK(tm.get_tick() > 10, "the thread ticked %u times", tm.get_tick());

	tm.stop_thread();
	msleep(100);
	tm.pause_thread();
	tm.resume_thread();
	g_shared_timer = NULL;
}

/* ------------------------------- verbose ----------------------------- */

static void test_verbose()
{
	CHECK(glb_verbose == 0, "verbosity must be off by default");
	glb_verbose = 3;
	CHECK(glb_verbose == 3, "glb_verbose must be writable");
	set_verbose_level(VERBOSE_LEVEL_ALL, 3);
	verbose(1, "verbose() is alive");
	verbosef(1, "verbosef(%d) is alive", 1);
	set_verbose_mode(VERBOSE_NULL, NULL);
	glb_verbose = 0;
	set_verbose_level(VERBOSE_LEVEL_ALL, 0);
}

/* ---------------------------------------------------------------------- */

int main()
{
	printf("cmlib self test\n");
	fflush(stdout);

	test_crc();
	test_base64();
	test_strtok();
	test_random();
	test_allocator();
	test_threadpool();
	test_spinlock();
	test_conf();
	test_verbose();
	test_timer();
	test_timer_thread();

	printf("%d checks, %d failures\n", checks, failures);
	fflush(stdout);
	return failures ? 1 : 0;
}
