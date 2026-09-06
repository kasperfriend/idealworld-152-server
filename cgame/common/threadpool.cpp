/*
	threadpool.cpp - ONET::Thread::Pool, the worker pool the gameserver runs
	its world logic on (cgame/include/threadpool.h).

	gs/start.cpp creates the pool with Pool::CreatePool(logic_threads) and
	every tick the timer thread posts one World_Tick_Task through
	Pool::AddTask; Pool::CreateThread() is used for the long running loops
	(the timer thread and PollIO).  Everything the pool needs besides
	RunThread() itself lives inline in the header, so this file is only the
	queue, its spinlock, the counters and the worker loop.

	Accounting notes:
	  - AddTask() bumps s_task_size while holding s_mutex_tasks, so the
	    matching decrement is done under the same lock here instead of the
	    interlocked form the header uses for the (32 bit) counters - mixing a
	    64 bit ++ with a 32 bit atomic decrement on the same variable would
	    corrupt it on a carry.
	  - s_task_count counts tasks that were taken off the queue, which is what
	    Pool::GetTotalTaskCount() is asked for in the periodic status log.
	  - ONET::__thread_task_count counts tasks *currently* executing (the same
	    log line prints it as "exec_task_count"), ONET::__thread_lock_count is
	    bumped by the spin locks in spinlock.cpp.

	When the queue runs dry the worker sleeps a millisecond rather than
	spinning: the world tick is only 20 Hz, and a full speed empty loop on
	16 cores costs more than the latency it saves.
*/

#include "threadpool.h"
#include "interlocked.h"

#include <deque>
#include <stdio.h>
#include <unistd.h>

namespace ONET
{

int __thread_lock_count = 0;
int __thread_task_count = 0;

namespace Thread
{

Pool::TaskQueue		Pool::s_tasks;
int			Pool::s_mutex_tasks = 0;

size_t			Pool::s_task_size = 0;
size_t			Pool::s_thread_count = 0;
size_t			Pool::s_task_count = 0;

void * Pool::RunThread(void * arg)
{
	/* created without an id we ever join on */
	pthread_detach(pthread_self());

	const size_t index = (size_t)arg;
	(void)index;

	for (;;)
	{
		Runnable * task = NULL;

		mutex_spinlock2(&s_mutex_tasks);
		if (!s_tasks.empty())
		{
			task = s_tasks.front();
			s_tasks.pop_front();
			if (s_task_size > 0) --s_task_size;
			++s_task_count;
		}
		mutex_spinunlock(&s_mutex_tasks);

		if (!task)
		{
			usleep(1000);
			continue;
		}

		interlocked_increment(&__thread_task_count);
		task->Run();			/* the task deletes itself when done */
		interlocked_decrement(&__thread_task_count);
	}

	return NULL;
}

void Pool::Dump()
{
	printf("thread pool: %lu threads, queued %lu, done %lu, running %d\n",
		(unsigned long)s_thread_count, (unsigned long)s_task_size,
		(unsigned long)s_task_count, __thread_task_count);
	fflush(stdout);
}

}
}
