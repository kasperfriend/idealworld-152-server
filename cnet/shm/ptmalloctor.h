#ifndef    _PTMALLOCATOR_H_
#define    _PTMALLOCATOR_H_
#include "shmcom.h"
#include "lock.h"
#include <pthread.h>
#include <map>
#include <list>
#include <functional>
namespace pwrdshmm
{
/******************************************** struct ********************************************/
#pragma pack(1)
typedef struct fixchunk_t
{
        u16 _header;
        u16 _free_count;
}fixchunk_t;

typedef struct bins_t
{
	list_head_t _lru;
	u32 _id;
}bins_t;

typedef struct chunk_t
{
	u64 _pre_size;
	u64 _size;
	list_head_t _lru;
	list_head_t _lru2; //the same size
}chunk_t;
struct fixallocator_t;
typedef struct ptmalloc_t
{
	enum
	{
		MAX_BINS = 128,
		MAX_MAP_BLOCK = 5,
		BLOCK_BIT_MASK = 31,

		MAX_FAST_BINS = 10,
		//MAX_FAST_BINS_SIZE = 144,
		MAX_FAST_BINS_SIZE = 0,
		MIN_SORT_BINS_SIZE = 1024,
		MAX_FIX_ALLOC_SIZE = 1024,
		MIN_MALLOC_SIZE = 32,


                MAX_ITERS = 10000,

		CHUNK_HEAD_SIZE = 8,
		MALLOC_ALIGN_MASK = 15,

		MAX_PTMALLOC_SIZE = 16*1024,

                DYNAMIAC_PAGE_HEAD_SIZE = 16,
                HEAP_HEAD_SIZE = 32,
                KMALLOC_PAGE_SIZE = 1024*1024,
                MIN_KMALLOC_SIZE = KMALLOC_PAGE_SIZE - DYNAMIAC_PAGE_HEAD_SIZE - HEAP_HEAD_SIZE
                                        - 2*CHUNK_HEAD_SIZE,

	};

	enum
	{
		PRE_USED = 0x1,
		KMALLOC = 0x2,
		CHUNK_MARK= 0x4,

	};

	static const unsigned long long CHUNK_SIZE_LOW_MASK = 0xf;
	static const unsigned long long CHUNK_SIZE_HIGH_MASK = 0xfffffffffffffff0ull;

        list_head_t _lru;
        u8 _id;
        u8 _used;
        u64 _class_addr;
	bins_t _bins[MAX_BINS];
	bins_t _fast_bins[MAX_FAST_BINS];
	u32 _bins_map[MAX_MAP_BLOCK];
        u8  _has_fast_bins;
        CPthreadMutex _mutex;
        list_head_t _heap_lru;
        fixallocator_t* _fix_allocator_t[MAX_FIX_ALLOC_SIZE];
}ptmalloc_t;


//the name is from ptmalloc
typedef struct ptheap_t
{
        u64 _ptmalloc_t_addr;
        u64 _reserve;
        list_head_t _lru;
}ptheap_t;
#pragma pack()
/******************************************** end ********************************************/

class CFixAllocator;
class CPTMallocator
{
public:
      friend int PTCoverTest();
      friend int PTBaseTest();
      friend int PTRandomTest();
      friend int PTRandomOneThrTest();
public:
        static void Destory(void*p);
        static CPTMallocator* GetInstance();

        CPTMallocator();
        ~CPTMallocator();
	static void* Malloc(unsigned long long size);
	static void Free(void *p);
        static unsigned long long MallocSize(void *p);
        static void* RMalloc(void*p, unsigned long long size);
	static bool TestChunk();
        static void* FastMalloc(size_t size);
	static void FastFree(void *p, size_t size);
        void* MallocImpl(unsigned long long size);
	void FreeImpl(void *p);
        unsigned long long MallocSizeImpl(void *p);
        bool Init(int create, ptmalloc_t* ptmalloc);
        void ClearFastAllocator();
        void ClearFastAllocator(size_t size);

        inline ptmalloc_t* GetPTMalloc()
        {
                return _ptmalloc;
        }
        inline bool IsPthreadOwn()
        {
                return _pthread_own;
        }
        inline void ClearPthreadOwn()
        {
                SHM_ASSERT(_pthread_own && _tid == pthread_self());
                _pthread_own = false;
        }
        inline void SetPthreadOwn()
        {
                SHM_ASSERT(!_pthread_own);
                _pthread_own = true;
                _tid = pthread_self();
        }

        inline pthread_t GetOwnTid()
        {
                return _tid;
        }





private:
        static void PthreadInit();
        static void* Creat();

	inline chunk_t* PreChunk(chunk_t* chunk)
	{
		SHM_ASSERT(!(chunk->_size & ptmalloc_t::CHUNK_MARK) ||
                        chunk->_pre_size != 0);
		return (chunk_t*)((char*)chunk - chunk->_pre_size);
	}

	inline chunk_t* NextChunk(chunk_t* chunk)
	{
		SHM_ASSERT(!(chunk->_size & ptmalloc_t::CHUNK_MARK) ||
                        GetSize(chunk) != 0);
		return (chunk_t*)((char*)chunk + GetSize(chunk));
	}

	inline bool IsUsed(chunk_t* chunk)
	{
		return NextChunk(chunk)->_size & ptmalloc_t::PRE_USED;
	}

	inline void SetUsed(chunk_t* chunk)
	{
		SHM_ASSERT(!IsUsed(chunk));
		NextChunk(chunk)->_size |= ptmalloc_t::PRE_USED;
	}

	inline void SetFree(chunk_t* chunk)
	{
		SHM_ASSERT(IsUsed(chunk));
		NextChunk(chunk)->_size &=  ~ptmalloc_t::PRE_USED;
	}

	inline bool IsKMalloc(chunk_t* chunk)
	{
		return chunk->_size & ptmalloc_t::KMALLOC;
	}

	inline void SetKMalloc(chunk_t* chunk)
	{
		chunk->_size |= ptmalloc_t::KMALLOC;
	}

	inline void ClearKmalloc(chunk_t* chunk)
	{
		chunk->_size &= ~ptmalloc_t::KMALLOC;
	}

	inline unsigned long long GetSize(chunk_t* chunk)
	{
		return chunk->_size & ptmalloc_t::CHUNK_SIZE_HIGH_MASK;
	}

	inline void SetSize(chunk_t* chunk, unsigned long long size)
	{
		SHM_ASSERT(size >= ptmalloc_t::MIN_MALLOC_SIZE);
		SHM_ASSERT(!(size & ptmalloc_t::CHUNK_SIZE_LOW_MASK));
		chunk->_size = (size & ptmalloc_t::CHUNK_SIZE_HIGH_MASK) | (chunk->_size & ptmalloc_t::CHUNK_SIZE_LOW_MASK);
		NextChunk(chunk)->_pre_size = size;
	}

	inline chunk_t* Split(chunk_t* chunk, unsigned long long new_size)
	{
		SHM_ASSERT(new_size + ptmalloc_t::MIN_MALLOC_SIZE <= GetSize(chunk));
		SHM_ASSERT(!(new_size & ptmalloc_t::MALLOC_ALIGN_MASK));
		unsigned long long left_chunk_size =  GetSize(chunk) - new_size;
                SHM_ASSERT(!(left_chunk_size & ptmalloc_t::MALLOC_ALIGN_MASK));
		SetSize(chunk, new_size);
                chunk_t* next_chunk = NextChunk(chunk);
                next_chunk->_size = 0;
                next_chunk->_lru.init();
		SetSize(next_chunk, left_chunk_size);
                next_chunk->_size |= ptmalloc_t::PRE_USED;
                if (left_chunk_size >= ptmalloc_t::MIN_SORT_BINS_SIZE)
                {
                        next_chunk->_lru2.init();
                }
                SHM_ASSERT(IsUsed(chunk) && IsUsed(next_chunk));
                return next_chunk;
	}

	inline chunk_t* GetFromBins(bins_t* bins)
	{
		SHM_ASSERT(bins->_lru._next != &bins->_lru);
		chunk_t* chunk = list_head(&bins->_lru,chunk_t,_lru);
		return GetFromBins(chunk);
	}

	inline chunk_t* GetFromBins(chunk_t* chunk)
	{
                if (GetSize(chunk)< ptmalloc_t::MIN_SORT_BINS_SIZE)
		{
			chunk->_lru.unlink();
		}
                else
                {
                        list_head_t* pre_link = chunk->_lru._pre;
                        list_head_t* equal = chunk->_lru2._next;
                        if (pre_link == &chunk->_lru || equal == &chunk->_lru2)
                        {
                                chunk->_lru.unlink();
                                chunk->_lru2.unlink();
                        }
                        else
                        {
                                chunk->_lru.unlink();
                                chunk->_lru2.unlink();
                                chunk_t* other_chunk = list_entry(equal,chunk_t,_lru2);
                                pre_link->push_front(other_chunk->_lru);
                        }
                }

                SetUsed(chunk);
                return chunk;
	}

        inline void PutToBins(list_head_t& list, chunk_t* chunk)
	{
	        chunk->_lru.init();
                if (GetSize(chunk)>= ptmalloc_t::MIN_SORT_BINS_SIZE)
		{
			chunk->_lru2.init();
		}
		SetFree(chunk);
                list.push_front(chunk->_lru);
	}

        inline void PutToBins2(list_head_t& list, chunk_t* chunk)
        {
                SHM_ASSERT(GetSize(chunk)>= ptmalloc_t::MIN_SORT_BINS_SIZE);
                chunk->_lru.init();
		chunk->_lru2.init();
		SetFree(chunk);
                list.push_front(chunk->_lru2);
        }

	inline chunk_t* GetFromFastBins(bins_t* bins)
	{
		SHM_ASSERT(bins->_lru._next != &bins->_lru);
		chunk_t* chunk = list_head(&bins->_lru,chunk_t,_lru);
		chunk->_lru.unlink();
		return chunk;
	}

	inline chunk_t* GetFromFastBins(chunk_t* chunk)
	{
		chunk->_lru.unlink();
		return chunk;
	}

	inline void PutToFastBins(list_head_t& list, chunk_t* chunk)
	{
	        chunk->_lru.unlink();
		list.push_front(chunk->_lru);
                _ptmalloc->_has_fast_bins = 1;
	}

	//	  index		num		diff		        min		max
	//   1  ~   63		64		16                      16		1008(1023)
	//  64  ~   95		32		64			1024		3008(3071)
	//  96  ~  111		16		512			3072	   10752(11263)
	// 112  ~  119		8		4096	                11264	   36864(40959)
	// 120  ~  123		4		32768	                40960	  131072(163839)
	// 124  ~  125		2		262144                  163840	  262144(524287)
	// 126			1				        524288

	inline unsigned int GetBinsIndex(unsigned long long sz)
	{
		unsigned long long index =	sz < 1024  ?	sz >> 4 :
			(sz  >>  6) <=  47	?	48   +  (sz  >>  6) :		//64
			(sz  >>  9) <=  21	?	90   +  (sz  >>  9) :		//512
			(sz  >> 12) <=  9	?	110  +	(sz  >> 12) :		//4096
			(sz  >> 15) <=  4	?	119  +	(sz  >> 15) :		//32768
			(sz  >> 18) <=  1	?	124  +	(sz  >> 18) :		//262144
			                126;

                SHM_ASSERT(index >= 2);
		return (unsigned int)index;
	}

        inline void SetBinsMap(unsigned int index)
        {
                SHM_ASSERT(index < 127);
                _bins_map[index>>ptmalloc_t::MAX_MAP_BLOCK] |= (1 << (index&ptmalloc_t::BLOCK_BIT_MASK));
        }

        inline void ClearBinsMap(unsigned int index)
        {
                SHM_ASSERT(index < 127);
                _bins_map[index>>ptmalloc_t::MAX_MAP_BLOCK] &= ~(1 << (index&ptmalloc_t::BLOCK_BIT_MASK));
        }

        inline bool GetBinsMap(unsigned int index)
        {
                SHM_ASSERT(index < 127);
                return _bins_map[index>>ptmalloc_t::MAX_MAP_BLOCK] & (1 << (index&ptmalloc_t::BLOCK_BIT_MASK));
        }

        void* FastMallocImpl(size_t size);
        void FastFreeImpl(void *p, size_t size);
        void SetHeapInfo(unsigned long long addr);
        static CPTMallocator* GetClassFromHeapInfo(unsigned long long addr);
        void PutChunkToSortBins(chunk_t* chunk);
        chunk_t* GetChunkFromSortBins(unsigned long long sz);
        chunk_t* NewChunk(unsigned long long sz);
        void PutChunk(chunk_t* chunk);
        chunk_t* GetChunk(unsigned long long sz);
        chunk_t* MergeChunk(chunk_t* chunk);
        void CleanFastBins();
        void PrintBins(int index);
        void PrintFastBins(int index);
	bool TestChunkImpl(void*point);
private:
	CPTMallocator(const CPTMallocator& lhs);
	CPTMallocator& operator = (const CPTMallocator& lhs);
private:
        static pthread_once_t _initflag;
        static pthread_key_t _pthread_key;
        static const unsigned long long NEW_ADDR_MASK = ptmalloc_t::KMALLOC_PAGE_SIZE - 1;
private:
        ptmalloc_t* _ptmalloc;
	bins_t* _bins;
        unsigned int* _bins_map;
	bins_t* _fast_bins;
        CPthreadMutex* _mutex;
        unsigned long long _new_addr_mask;
        pthread_t _tid;
        bool _pthread_own;
        CFixAllocator* _fix_allocator[ptmalloc_t::MAX_FIX_ALLOC_SIZE];
};


/******************************************** CFixAllocator ********************************************/
template <class _Tp>
class CSTLMalloc
{
public:
        typedef size_t     size_type;
        typedef ptrdiff_t  difference_type;
        typedef _Tp*       pointer;
        typedef const _Tp* const_pointer;
        typedef _Tp&       reference;
        typedef const _Tp& const_reference;
        typedef _Tp        value_type;


        template<typename _Tp1>
        struct rebind
        {
                typedef CSTLMalloc<_Tp1> other;
        };

        CSTLMalloc()
        {
              _ptmalloc = CPTMallocator::GetInstance()->GetPTMalloc();
              SHM_ASSERT(_ptmalloc);
        }
        CSTLMalloc(const CSTLMalloc&rhs):_ptmalloc(rhs._ptmalloc) {}
        CSTLMalloc& operator = (const CSTLMalloc&rhs)
        {
                _ptmalloc = rhs._ptmalloc;
        }
        template <class _Tp1>
        CSTLMalloc(const CSTLMalloc<_Tp1>&rhs)
        {
                _ptmalloc = rhs.GetAlloc();
        }

        ~CSTLMalloc(){}

        _Tp* allocate(size_t __n)
        {
                unsigned long long sz = sizeof(_Tp)*__n;
                CPTMallocator* alloc = (CPTMallocator*)_ptmalloc->_class_addr;
                SHM_ASSERT(alloc);
                return (_Tp*)alloc->MallocImpl(sz);
        }

        void deallocate(void* __p, size_t /* __n */)
        {
                CPTMallocator* alloc = (CPTMallocator*)_ptmalloc->_class_addr;
                SHM_ASSERT(alloc);
                alloc->FreeImpl(__p);
        }

        void construct(pointer p, const_reference value)
        {
                new (p) _Tp(value);
        }

        void destroy(pointer p)
        {
                p->~_Tp();
        }

        pointer address(reference __x) const
        {
                return &__x;
        }
        const_pointer address(const_reference __x) const
        {
                return &__x;
        }

        size_type max_size() const
        {
                return size_t(-1)/sizeof(_Tp);
        }

        ptmalloc_t* GetAlloc() const
        {
                return _ptmalloc;
        }
private:
        ptmalloc_t* _ptmalloc;
};

/// allocator<void> specialization.
template<>
class CSTLMalloc<void>
{
public:
        typedef size_t      size_type;
        typedef ptrdiff_t   difference_type;
        typedef void*       pointer;
        typedef const void* const_pointer;
        typedef void        value_type;

        template<typename _Tp1>
        struct rebind
        {
                typedef CSTLMalloc<_Tp1> other;
        };
};


/******************************************** struct ********************************************/
#pragma pack(1)
typedef struct fixallocator_t
{
        u32 _element_size;
        u32 _count;
        typedef std::map<unsigned char*, fixchunk_t*, std::greater<unsigned char*>, CSTLMalloc< std::pair<const unsigned char*,fixchunk_t*> > > used_map;
        used_map _used_map;
        typedef std::list<fixchunk_t*, CSTLMalloc<fixchunk_t*> >  free_list;
        free_list _free_list;
}fixallocator_t;
#pragma pack()
/******************************************** end ********************************************/

class CFixAllocator
{
public:
        enum
        {
                MAX_COUNT = 1024,
        };
public:
        explicit CFixAllocator(CPTMallocator* alloc,fixallocator_t* fixallocator_data);
        explicit CFixAllocator(CPTMallocator* alloc,fixallocator_t* fixallocator_data, int element_size);
        ~CFixAllocator() {}
        void* operator new (size_t size);
        void operator delete (void* p);

        inline void * Malloc()
        {
                if (_fixallocator_data->_free_list.empty())
                {
                        AllocNewChunk();
                }
                fixchunk_t* pchunk = _fixallocator_data->_free_list.front();
                void *tmp = AllocFromChunk(pchunk);
                if (IsEmpty(pchunk))
                {
                        OnChunkEmpty(pchunk);
                }
                _fixallocator_data->_count--;
                return tmp;
        }

        inline void Free(void *buf)
        {
                fixchunk_t* pchunk = FindChunk(buf);
                SHM_ASSERT(pchunk);
                FreeToChunk(pchunk, buf);
                _fixallocator_data->_count++;
                if (IsFull(pchunk))
                {
                        OnChunkFull(pchunk);
                }
        }

        inline int ObjCount()
        {
                return TotalCount() - _fixallocator_data->_count;
        }

        inline int TotalCount()
        {
                return (_fixallocator_data->_used_map.size()
                        + _fixallocator_data->_free_list.size()) * MAX_COUNT;
        }

        inline void ClearFreeList()
        {
                while (!_fixallocator_data->_free_list.empty())
                {
                        fixchunk_t*pchunk =  _fixallocator_data->_free_list.back();
                        if (IsFull(pchunk))
                        {
                                _fixallocator_data->_free_list.pop_back();
                                ReleaseChunk(pchunk);
                                _fixallocator_data->_count -= MAX_COUNT;
                        }
                        else
                        {
                                SHM_ASSERT(_fixallocator_data->_free_list.size() == 1);
                                return;
                        }
                }
        }

private:
        inline fixchunk_t* NewChunk()
        {
                size_t count = _element_size * MAX_COUNT;
                fixchunk_t* pchunk = (fixchunk_t*)_alloc->MallocImpl(count+sizeof(fixchunk_t));
                SHM_ASSERT(pchunk);
                pchunk->_header = 0;
                pchunk->_free_count = MAX_COUNT;
                unsigned char *buf = GetBuf(pchunk);
                size_t offset  = 0;
                for(size_t i = 0; i < MAX_COUNT; i++, offset += _element_size)
		{
			*(unsigned short*)(buf + offset) = (i + 1) & 0xffff;
		}
                return pchunk;
        }

        inline void ReleaseChunk(fixchunk_t* pchunk)
        {
                SHM_ASSERT(pchunk->_free_count == MAX_COUNT);
                pchunk->_free_count = 0;
                pchunk->_header = 0xffff;
                _alloc->FreeImpl(pchunk);
        }

        inline unsigned char* GetBuf(fixchunk_t* pchunk)
        {
                SHM_ASSERT(pchunk);
                return (unsigned char*)(pchunk+1);
        }

        inline bool IsEmpty(fixchunk_t* pchunk)
	{
		return !pchunk->_free_count;
	}

	inline bool IsFull(fixchunk_t* pchunk)
	{
		return pchunk->_free_count == MAX_COUNT;
	}

        inline bool IsInside(fixchunk_t* pchunk, void * tmp)
	{
	        unsigned char* buf = GetBuf(pchunk);
		return (unsigned char *)tmp>= buf && (unsigned char *)tmp < (buf + _element_size* MAX_COUNT);
	}

        inline void * AllocFromChunk(fixchunk_t* pchunk)
	{
		if(pchunk->_free_count > 0)
		{
		        unsigned char* buf = GetBuf(pchunk);
			void * tmp = buf + pchunk->_header * _element_size;
			pchunk->_header = *(unsigned short *)tmp;
			--pchunk->_free_count;
			return tmp;
		}
		SHM_ASSERT(0);
		return 0;
	}

	inline void  FreeToChunk(fixchunk_t* pchunk, void * tmp)
	{
	        unsigned char* buf = GetBuf(pchunk);
		SHM_ASSERT((unsigned char *)tmp >= buf && (unsigned char *)tmp < (buf + _element_size* MAX_COUNT) );
		int offset = (unsigned char *)tmp - buf;
		SHM_ASSERT(offset % _element_size == 0);
		*(unsigned short*)tmp = pchunk->_header;
		pchunk->_header = offset / _element_size;
		++pchunk->_free_count;
                SHM_ASSERT(pchunk->_free_count <= MAX_COUNT);
	}

        inline void PushToUsedList(fixchunk_t* pchunk)
        {
                SHM_ASSERT(pchunk->_free_count == 0);
                _fixallocator_data->_used_map.insert(std::make_pair<unsigned char*,fixchunk_t*>(GetBuf(pchunk),pchunk));
        }

        inline void AllocNewChunk()
	{
		SHM_ASSERT( _fixallocator_data->_free_list.empty());
		fixchunk_t* pchunk = NewChunk();
		_fixallocator_data->_free_list.push_back(pchunk);
		_fixallocator_data->_count += MAX_COUNT;
	}

        inline void OnChunkEmpty(fixchunk_t* pchunk)
        {
                SHM_ASSERT(pchunk->_free_count == 0
                        && pchunk == _fixallocator_data->_free_list.front());
                _fixallocator_data->_free_list.pop_front();
                PushToUsedList(pchunk);
        }

        inline void OnChunkFull(fixchunk_t* pchunk)
        {
                SHM_ASSERT(pchunk->_free_count == MAX_COUNT);
                if (!_fixallocator_data->_free_list.empty() &&
                        pchunk == _fixallocator_data->_free_list.front())
                {
                        return;
                }
                fixallocator_t::used_map::iterator it = _fixallocator_data->_used_map.find(GetBuf(pchunk));
                if (it != _fixallocator_data->_used_map.end())
                {
                        _fixallocator_data->_used_map.erase(it);
                }
                else
                {
                        SHM_ASSERT(0);
                }
                if (_fixallocator_data->_free_list.size() > _fixallocator_data->_used_map.size()/4 + 1)
                {
                        ReleaseChunk(pchunk);
                        _fixallocator_data->_count -= MAX_COUNT;
                }
                else
                {
                        _fixallocator_data->_free_list.push_back(pchunk);
                }
        }


        inline fixchunk_t* FindChunk(void *buf)
        {
                if (!_fixallocator_data->_free_list.empty()
                        && IsInside(_fixallocator_data->_free_list.front(),buf))
                {
                        return _fixallocator_data->_free_list.front();
                }
                fixallocator_t::used_map::iterator it = _fixallocator_data->_used_map.lower_bound((unsigned char*)buf);
                if (it != _fixallocator_data->_used_map.end() &&  IsInside(it->second,buf))
                {
                        return it->second;
                }
                SHM_ASSERT(0);
                return 0;
        }



private:
        CPTMallocator* _alloc;
        fixallocator_t* _fixallocator_data;
        unsigned int _element_size;
};
};
#endif
