#include "lock.h"
namespace pwrdshmm
{
#include <sched.h>
#include <unistd.h>
enum
{
	MUTEX_SPIN_COUNT = 50,
	MUTEX_SPINSLEEP_DURATION = 1001,
};
static inline int test_and_set (int *spinlock)
{
	int ret;

	__asm__ __volatile__(
			"xchgl %0, %1"
			: "=r"(ret), "+m"(*spinlock)
			: "0"(1), "m"(*spinlock)
			: "memory");

	return ret;
}

int  mutex_spinset(int *__spinlock)
{
	return test_and_set(__spinlock);
}	

void mutex_spinlock(int *__spinlock)
{
	int cnt = 0;
	int icount = 0;
	while( test_and_set(__spinlock)) 
	{
		if(cnt < MUTEX_SPIN_COUNT + 1) 
		{
			sched_yield();
			cnt++;
		} 
		else 
		{
			usleep(MUTEX_SPINSLEEP_DURATION);
			cnt = 1;
			icount ++;
			if(icount > 4096)
			{
				SHM_ASSERT(0);
			}
		}
	}
	return;
}
void mutex_spinunlock(int *spinlock)
{
	*spinlock = 0;
}

#ifdef _REENTRANT_ 
CPthreadMutex::CPthreadMutex()
{
}

bool CPthreadMutex::Init()
{
	//return pthread_mutex_init(&_mutex, NULL) ? false : true;
	_mutex = 0;
	return true;
}

bool CPthreadMutex::Lock()
{
	//return pthread_mutex_lock(&_mutex) ? false : true;
	mutex_spinlock(&_mutex);
	return true;
}

bool CPthreadMutex::Unlock()
{
	//return pthread_mutex_unlock(&_mutex) ? false : true;
	mutex_spinunlock(&_mutex);
	return true;
}

bool CPthreadMutex::Trylock()
{
	return test_and_set(&_mutex) ? false : true;
	//return true;
}
#endif
};

