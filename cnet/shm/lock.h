#ifndef    _SHM_LOCK_H__
#define    _SHM_LOCK_H__
#include <pthread.h>
#include "shmcom.h"
namespace pwrdshmm
{
#ifdef _REENTRANT_ 
class CPthreadMutex
{
public:
	CPthreadMutex();
	bool Init();
	bool Lock();
	bool Unlock();
	bool Trylock();
private:
	CPthreadMutex(const CPthreadMutex& lhs);
	CPthreadMutex& operator= (const CPthreadMutex& lhs);
private:
//	pthread_mutex_t  _mutex;
	int	_mutex;
};
#else 
class CPthreadMutex
{
public:
	CPthreadMutex() {}; 
	inline bool Init() 
	{
		return true;
	}
	inline bool Lock()
	{
		return true;
	}
	inline bool Unlock()
	{
		return true;
	}
	inline bool Trylock()
	{
		return true;
	}
private:
	CPthreadMutex(const CPthreadMutex& lhs);
	CPthreadMutex& operator= (const CPthreadMutex& lhs);
private:
//	pthread_mutex_t  _mutex;
	int	_mutex;
};
#endif

class CAutoLock
{
public:
	CAutoLock(CPthreadMutex* mutex)
	:_mutex(mutex)
	{
	}

	~CAutoLock()
	{
		SHM_ASSERT(Unlock());
	}

	bool Lock()
	{
		return _mutex->Lock();
	}
	bool Unlock()
	{
		return _mutex->Unlock();
	}
	bool Trylock()
	{
		return _mutex->Trylock();
	}
private:
	CPthreadMutex* _mutex;
};


#define SHM_GUARD(MUTEX, OBJ, ACTION) \
	CAutoLock OBJ(MUTEX); \
	if (!OBJ.Lock()) { ACTION; }

#define SHM_GUARD_LOCKED(MUTEX, OBJ, ACTION) \
	CAutoLock OBJ(MUTEX); \
	if (OBJ.Trylock()) { ACTION; }


};

#endif
