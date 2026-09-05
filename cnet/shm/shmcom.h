#ifndef    _SHMCOM_H_
#define    _SHMCOM_H_
#include <assert.h>
namespace pwrdshmm
{
/*
	macro
*/
#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((unsigned long) &((TYPE *)0)->MEMBER)
#endif

#define container_of(ptr, type, member) (\
	(type *)((char *)(ptr) - offsetof(type,member)))

#define list_entry(ptr, type, member) \
	container_of(ptr, type, member)

#define list_head(list, type, member)		\
	list_entry((list)->_next, type, member)

#define list_tail(list, type, member)		\
	list_entry((list)->_pre, type, member)


#ifdef __SHM_DEBUG__
#define SHM_BUG(x) assert(0)
#define SHM_ASSERT(x) assert(x)
#else
#define SHM_BUG(x) assert(0)
#define SHM_ASSERT(x) assert(x)
#endif

#define UNUSED_ARG(x) (void 0)(x)

#ifdef _REENTRANT_
#define SHM_MT(X) X
#else
#define SHM_MT(X)
#endif

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
/******************************************** macro end ********************************************/


/*
	struct define
*/
#pragma pack(1)
typedef struct list_head_t
{
	list_head_t* _pre;
	list_head_t* _next;

	list_head_t()
	{
		init();
	}
	inline void init()
	{
		_pre = _next = this;
	}
	inline void unlink()
	{
		_pre->_next = _next;
		_next->_pre = _pre;
		_pre = _next = this;
	}
	inline void push_front(list_head_t& list)
	{
		list_head_t* bk = list._pre;
		list_head_t* next = _next;
		_next = &list;
		list._pre = this;
		bk->_next = next;
		next->_pre = bk;
	}
	inline void push_back(list_head_t& list)
	{
		_pre->push_front(list);
	}
}list_head_t;

#pragma pack()
/******************************************** struct define end ********************************************/

/*
	inline fun
*/


/******************************************** inline end ********************************************/
};

#endif

