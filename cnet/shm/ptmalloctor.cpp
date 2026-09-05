#include "ptmalloctor.h"
#include "kmalloc.h"
#include <string.h>
#include <stdio.h> //test
#include <malloc.h>
namespace pwrdshmm
{

pthread_once_t CPTMallocator::_initflag = PTHREAD_ONCE_INIT;
pthread_key_t CPTMallocator::_pthread_key;


CPTMallocator::CPTMallocator()
:_ptmalloc(0),_bins(0),_bins_map(0),_fast_bins(0),_new_addr_mask(0),
_tid(0),_pthread_own(false)
{
        for (int ix = 0; ix < ptmalloc_t::MAX_FIX_ALLOC_SIZE; ix++)
        {
                _fix_allocator[ix] = 0;
        }
}

CPTMallocator::~CPTMallocator()
{
        /*
        if (_ptmalloc)
        {
                CKMalloc::GetInstance().Free((void*)_ptmalloc);
        }
        _ptmalloc = 0;
        */
        _ptmalloc = 0;
        _bins = 0;
        _bins_map = 0;
        _fast_bins = 0;
        _new_addr_mask = 0;
        _tid = 0;
        _pthread_own = false;
        for (int ix = 0; ix < ptmalloc_t::MAX_FIX_ALLOC_SIZE; ix++)
        {
                if (_fix_allocator[ix])
                {
                       delete _fix_allocator[ix];
                }
        }
}

bool CPTMallocator::Init(int create, ptmalloc_t * ptmalloc)
{
        if (create)
        {
                SHM_ASSERT(!ptmalloc);
                SHM_ASSERT(ptmalloc_t::MIN_KMALLOC_SIZE > 2*ptmalloc_t::MIN_MALLOC_SIZE + sizeof(*ptmalloc) + ptmalloc_t::HEAP_HEAD_SIZE);
                unsigned long long heap_addr = (unsigned long long)CKMalloc::GetInstance().Malloc(ptmalloc_t::MIN_KMALLOC_SIZE + ptmalloc_t::HEAP_HEAD_SIZE + 2*ptmalloc_t::CHUNK_HEAD_SIZE);
                if (!heap_addr)
                {
                        return false;
                }

                _ptmalloc = (ptmalloc_t*)(heap_addr+ptmalloc_t::HEAP_HEAD_SIZE);
		_ptmalloc->_heap_lru.init();
                _bins = _ptmalloc->_bins;
                _bins_map = _ptmalloc->_bins_map;
                _fast_bins = _ptmalloc->_fast_bins;

                for (int ix = 0; ix < ptmalloc_t::MAX_BINS; ix++)
                {

                        _bins[ix]._id = ix;
                        _bins[ix]._lru.init();
                }

                for (int ix = 0; ix < ptmalloc_t::MAX_MAP_BLOCK; ix++)
                {
                        _bins_map[ix] = 0;
                }

                for (int ix = 0; ix < ptmalloc_t::MAX_FAST_BINS; ix++)
                {

                        _fast_bins[ix]._id = ix;
                        _fast_bins[ix]._lru.init();
                }

                SetHeapInfo(heap_addr);
                chunk_t* chunk = (chunk_t*)((char*)_ptmalloc + ptmalloc_t::MAX_PTMALLOC_SIZE);
                unsigned long long chunk_size = ptmalloc_t::MIN_KMALLOC_SIZE - ptmalloc_t::MAX_PTMALLOC_SIZE;

                chunk->_lru.init();
                chunk->_lru2.init();
                chunk->_pre_size = ptmalloc_t::MAX_PTMALLOC_SIZE + ptmalloc_t::HEAP_HEAD_SIZE;
                chunk->_size = chunk_size | ptmalloc_t::PRE_USED;

                chunk_t* next_chunk = NextChunk(chunk);
                next_chunk->_pre_size = chunk_size;
                next_chunk->_size = ptmalloc_t::PRE_USED | ptmalloc_t::CHUNK_MARK;
                PutChunkToSortBins(chunk);
                _ptmalloc->_has_fast_bins = 0;

                if (!_ptmalloc->_mutex.Init())
                {
                        return false;
                }
                _ptmalloc->_lru.init();
                _ptmalloc->_class_addr = 0;
                for (int ix = 0; ix < ptmalloc_t::MAX_FIX_ALLOC_SIZE; ix++)
                {
                        _ptmalloc->_fix_allocator_t[ix] = 0;
                }
        }
        else
        {
                _ptmalloc = ptmalloc;
                _bins = _ptmalloc->_bins;
                _bins_map = _ptmalloc->_bins_map;
                _fast_bins = _ptmalloc->_fast_bins;

        }
        _mutex = &_ptmalloc->_mutex;
        SHM_ASSERT(!_ptmalloc->_class_addr);
        _ptmalloc->_class_addr = (unsigned long long)this;
        _new_addr_mask = NEW_ADDR_MASK;

        for (int ix = 0; ix < ptmalloc_t::MAX_FIX_ALLOC_SIZE; ix++)
        {
                if (_ptmalloc->_fix_allocator_t[ix])
                {
                        _fix_allocator[ix] = new CFixAllocator(this, _ptmalloc->_fix_allocator_t[ix]);
                }
        }
        return true;
}

void CPTMallocator::PutChunk( chunk_t* chunk )
{
        if (IsKMalloc(chunk))
        {
		ptheap_t*pheap = (ptheap_t*)((char*)chunk-ptmalloc_t::HEAP_HEAD_SIZE);
		pheap->_lru.unlink();
                CKMalloc::GetInstance().Free((void*)((char*)chunk-ptmalloc_t::HEAP_HEAD_SIZE));
                return;
        }

        chunk = MergeChunk(chunk);
        if (chunk)
        {
                unsigned long long sz = GetSize(chunk);
                if (sz <= ptmalloc_t::MAX_FAST_BINS_SIZE)
                {
                        unsigned int index = GetBinsIndex(sz);
                        SHM_ASSERT(index >= 2 && index < ptmalloc_t::MAX_FAST_BINS);
                        PutToFastBins(_fast_bins[index]._lru, chunk);
                }
                else
                {
	                PutToBins(_bins->_lru, chunk);
                }
        }
}

chunk_t* CPTMallocator::GetChunk(unsigned long long sz)
{
        unsigned long long malloc_size = sz + ptmalloc_t::CHUNK_HEAD_SIZE;
        malloc_size = (malloc_size + ptmalloc_t::MALLOC_ALIGN_MASK) & ~ptmalloc_t::MALLOC_ALIGN_MASK;

        if (malloc_size < ptmalloc_t::MIN_MALLOC_SIZE)
        {
                malloc_size = ptmalloc_t::MIN_MALLOC_SIZE;
        }

        if (malloc_size <= sz)
        {
                return 0;
        }

        if (malloc_size >= ptmalloc_t::MIN_KMALLOC_SIZE)
        {
                chunk_t* chunk = NewChunk(malloc_size);
                if (!chunk)
                {
                        return 0;
                }

                chunk->_size |= ptmalloc_t::KMALLOC;
                SHM_ASSERT(malloc_size <= GetSize(chunk));
                return chunk;
        }
        else if (_ptmalloc->_has_fast_bins&&
                malloc_size <= ptmalloc_t::MAX_FAST_BINS_SIZE)
        {
                unsigned int index = GetBinsIndex(malloc_size);
                if (_fast_bins[index]._lru._next != &(_fast_bins[index]._lru))
                {
                        chunk_t* chunk = GetFromFastBins(&_fast_bins[index]);
                        return chunk;
                }
        }
        else if (malloc_size < ptmalloc_t::MIN_SORT_BINS_SIZE)
        {
                unsigned int index = GetBinsIndex(malloc_size);
                if (_bins[index]._lru._next != &(_bins[index]._lru))
                {
                        chunk_t* chunk = GetFromBins(_bins+index);
                        return chunk;
                }
        }

        for (int ix = 0; ix < ptmalloc_t::MAX_ITERS; ix++)
        {
                if (_bins->_lru._next == &_bins->_lru)
                {
                        break;
                }
                chunk_t* chunk = list_entry(_bins->_lru._next, chunk_t, _lru);
                chunk = GetFromBins(chunk);
                if (GetSize(chunk)== malloc_size)
                {
                        return chunk;
                }
                PutChunkToSortBins(chunk);
        }

        if (_ptmalloc->_has_fast_bins)
        {
                CleanFastBins();
        }

        chunk_t* chunk = GetChunkFromSortBins(malloc_size);
        if (chunk)
        {
                SHM_ASSERT(malloc_size <= GetSize(chunk));
        }
        return chunk;
}

chunk_t* CPTMallocator::NewChunk(unsigned long long sz)
{
        if (sz < ptmalloc_t::MIN_KMALLOC_SIZE || (sz & ptmalloc_t::MALLOC_ALIGN_MASK))
        {
                return 0;
        }

        unsigned long long malloc_size = sz +  2*ptmalloc_t::CHUNK_HEAD_SIZE + ptmalloc_t::HEAP_HEAD_SIZE;
        if (malloc_size <= sz)
        {
                return 0;
        }

        unsigned long long addr = (unsigned long long)CKMalloc::GetInstance().Malloc(malloc_size);
        if (!addr)
        {
                return 0;
        }

        SHM_ASSERT(!((addr-ptmalloc_t::DYNAMIAC_PAGE_HEAD_SIZE)&_new_addr_mask));

        SetHeapInfo(addr);
        chunk_t* chunk = (chunk_t*)(addr+ptmalloc_t::HEAP_HEAD_SIZE);
        chunk->_lru.init();
        chunk->_lru2.init();
        chunk->_pre_size = 0;
        chunk->_size = sz | ptmalloc_t::PRE_USED | ptmalloc_t::CHUNK_MARK;

        chunk_t* next_chunk = NextChunk(chunk);
        next_chunk->_pre_size = sz;
        next_chunk->_size = ptmalloc_t::PRE_USED | ptmalloc_t::CHUNK_MARK;

        return chunk;
}

chunk_t* CPTMallocator::MergeChunk(chunk_t* chunk)
{
        SHM_ASSERT(!IsKMalloc(chunk));

        if (!(chunk->_size & ptmalloc_t::PRE_USED) && !(chunk->_size & ptmalloc_t::CHUNK_MARK))
        {
                chunk_t* pre_chunk = PreChunk(chunk);
                pre_chunk = GetFromBins(pre_chunk);
                unsigned int index = GetBinsIndex(GetSize(pre_chunk));
                if (_bins[index]._lru._next == &_bins[index]._lru)
                {
                        ClearBinsMap(index);
                }
                SetSize(pre_chunk,GetSize(pre_chunk) + GetSize(chunk));
                chunk = pre_chunk;
        }

        chunk_t* next_chunk = NextChunk(chunk);
        if (!(next_chunk->_size & ptmalloc_t::CHUNK_MARK) && !IsUsed(next_chunk))
        {
                next_chunk = GetFromBins(next_chunk);
                unsigned int index = GetBinsIndex(GetSize(next_chunk));
                if (_bins[index]._lru._next == &_bins[index]._lru)
                {
                        ClearBinsMap(index);
                }
                SetSize(chunk, GetSize(chunk) + GetSize(next_chunk));
        }

        if (chunk->_size & ptmalloc_t::CHUNK_MARK)
        {
                next_chunk = NextChunk(chunk);
                if (next_chunk->_size & ptmalloc_t::CHUNK_MARK)
                {
                        SHM_ASSERT(IsUsed(chunk));
                        unsigned long long malloc_addr = (unsigned long long)((char*)chunk - ptmalloc_t::HEAP_HEAD_SIZE);
                        SHM_ASSERT(!((malloc_addr - ptmalloc_t::DYNAMIAC_PAGE_HEAD_SIZE)
                                & _new_addr_mask));
			((ptheap_t*)malloc_addr)->_lru.unlink();
                        CKMalloc::GetInstance().Free((void*)malloc_addr);
                        return 0;
                }
        }

        return chunk;
}


void CPTMallocator::PutChunkToSortBins(chunk_t* chunk)
{
        SHM_ASSERT(chunk);

        unsigned long long chunk_size = GetSize(chunk);
        unsigned int index = GetBinsIndex(chunk_size);

        if (chunk_size < ptmalloc_t::MIN_SORT_BINS_SIZE)
        {
                PutToBins(_bins[index]._lru, chunk);
                return;
        }

        if (_bins[index]._lru._next == &_bins[index]._lru)
        {
                PutToBins(_bins[index]._lru, chunk);
                SetBinsMap(index);
        }
        else
        {
                chunk_t* other = 0;
                list_head_t* plist = &_bins[index]._lru;
		while (plist->_next != &_bins[index]._lru)
		{
		        plist = plist->_next;
                        other = list_entry(plist, chunk_t, _lru);
                        if (chunk_size <= GetSize(other))
                        {
                                break;
                        }
                }

                unsigned long long other_size = GetSize(other);
                if (chunk_size > other_size)
                {
                        SHM_ASSERT(plist->_next == &_bins[index]._lru);
                        PutToBins(other->_lru, chunk);
                }
                else if (chunk_size == other_size)
                {
                        PutToBins2(other->_lru2, chunk);
                }
                else
                {
                        PutToBins(*other->_lru._pre, chunk);
                }

        }
}

chunk_t* CPTMallocator::GetChunkFromSortBins(unsigned long long sz)
{
        SHM_ASSERT(sz < ptmalloc_t::MIN_KMALLOC_SIZE);
        unsigned int index = GetBinsIndex(sz);

        chunk_t* other = 0;
        list_head_t* plist = &_bins[index]._lru;
	while (plist->_next != &_bins[index]._lru)
	{
	        plist = plist->_next;
                other = list_entry(plist, chunk_t, _lru);
                if (sz <= GetSize(other))
                {
                        break;
                }
        }

        if (other && sz <= GetSize(other) && sz + ptmalloc_t::MIN_MALLOC_SIZE > GetSize(other))
        {
                chunk_t* chunk = GetFromBins(other);
                if (_bins[index]._lru._next == &_bins[index]._lru)
                {
                        ClearBinsMap(index);
                }
                return chunk;
        }
        else if (other && sz + ptmalloc_t::MIN_MALLOC_SIZE <= GetSize(other))
        {
                chunk_t* chunk = GetFromBins(other);
                chunk_t* remainder = Split(chunk, sz);
                if (_bins[index]._lru._next == &_bins[index]._lru)
                {
                        ClearBinsMap(index);
                }
                PutChunkToSortBins(remainder);
                return chunk;
        }

        index++;
        unsigned int block = index>>ptmalloc_t::MAX_MAP_BLOCK;
        unsigned int bit = 1 << (index&ptmalloc_t::BLOCK_BIT_MASK);

        for(;block < ptmalloc_t::MAX_MAP_BLOCK; block++, bit=1)
        {
                if (_bins_map[block] == 0 || bit > _bins_map[block])
                {
                        continue;
                }

                while ((_bins_map[block] & bit) == 0)
                {
                        bit <<= 1;
                }

                SHM_ASSERT(bit<=_bins_map[block]);

                index = block << ptmalloc_t::MAX_MAP_BLOCK;
                bit >>= 1;
                while(bit)
                {
                        bit>>=1;
                        index++;
                }

                SHM_ASSERT(_bins[index]._lru._next != &_bins[index]._lru);

                chunk_t* chunk = GetFromBins(_bins+index);
                if (_bins[index]._lru._next == &_bins[index]._lru)
                {
                        ClearBinsMap(index);
                }

                if (sz + ptmalloc_t::MIN_MALLOC_SIZE <= GetSize(chunk))
                {
                        chunk_t*left_chunk = Split(chunk, sz);
                        PutChunkToSortBins(left_chunk);
                }

                return chunk;
        }



        chunk_t* chunk = NewChunk(ptmalloc_t::MIN_KMALLOC_SIZE);
        if (!chunk)
        {
                return 0;
        }
        if (sz + ptmalloc_t::MIN_MALLOC_SIZE <= GetSize(chunk))
        {
                chunk_t*left_chunk = Split(chunk, sz);
                PutChunkToSortBins(left_chunk);
        }
        return chunk;
}

void* CPTMallocator::MallocImpl(unsigned long long size)
{

        chunk_t* chunk = GetChunk(size);
        if (!chunk)
        {
                return 0;
        }
        return (char*)chunk+2*ptmalloc_t::CHUNK_HEAD_SIZE;
}

void CPTMallocator::FreeImpl(void * p)
{
        if (!p)
        {
                return;
        }
        chunk_t* chunk = (chunk_t*)((char*)p-2*ptmalloc_t::CHUNK_HEAD_SIZE);
        unsigned long long sz = GetSize(chunk);
        SHM_ASSERT(!(sz & ptmalloc_t::MALLOC_ALIGN_MASK));
        SHM_ASSERT(sz >= ptmalloc_t::MIN_MALLOC_SIZE && IsUsed(chunk));
        chunk->_lru.init();
        if (sz >= ptmalloc_t::MIN_SORT_BINS_SIZE)
        {
                chunk->_lru2.init();
        }
        SetSize(chunk,sz);
        PutChunk(chunk);
}

void CPTMallocator::CleanFastBins()
{
        SHM_ASSERT(_ptmalloc->_has_fast_bins);
        for (unsigned int ix = 2; ix < ptmalloc_t::MAX_FAST_BINS; ix++)
        {
                list_head_t* plist = &_fast_bins[ix]._lru;
	        while (plist->_next != &_fast_bins[ix]._lru)
	        {
                        chunk_t* chunk = list_entry(plist->_next, chunk_t, _lru);
                        chunk = GetFromFastBins(chunk);
                        chunk = MergeChunk(chunk);
                        if (chunk)
                        {
                                PutChunkToSortBins(chunk);
                        }
                }
        }
        _ptmalloc->_has_fast_bins = 0;
}


unsigned long long CPTMallocator::MallocSizeImpl(void *p)
{
        if (!p)
        {
                return 0;
        }
        chunk_t* chunk = (chunk_t*)((char*)p-2*ptmalloc_t::CHUNK_HEAD_SIZE);
        unsigned long long sz = GetSize(chunk);
        SHM_ASSERT(!(sz & ptmalloc_t::MALLOC_ALIGN_MASK));
        SHM_ASSERT(sz >= ptmalloc_t::MIN_MALLOC_SIZE && IsUsed(chunk));
        return sz-ptmalloc_t::CHUNK_HEAD_SIZE;
}

void CPTMallocator::SetHeapInfo(unsigned long long addr)
{
        SHM_ASSERT(!((addr-ptmalloc_t::DYNAMIAC_PAGE_HEAD_SIZE)&_new_addr_mask));
        ptheap_t* phead = (ptheap_t*)addr;
        phead->_ptmalloc_t_addr = (unsigned long long)_ptmalloc;;
	phead->_lru.init();
	_ptmalloc->_heap_lru.push_back(phead->_lru);
}

CPTMallocator* CPTMallocator::GetClassFromHeapInfo(unsigned long long addr)
{

        unsigned long long heap_addr = (addr & ~NEW_ADDR_MASK)
                                        + ptmalloc_t::DYNAMIAC_PAGE_HEAD_SIZE;
        ptheap_t* phead = (ptheap_t*)heap_addr;
        ptmalloc_t* ptmalloc = (ptmalloc_t*)phead->_ptmalloc_t_addr;
        ptmalloc->_mutex.Lock();
        if (!ptmalloc->_class_addr)
        {
                return CKMalloc::GetInstance().GetPTMallocator(ptmalloc->_id);
        }
        else
        {
                return (CPTMallocator*)ptmalloc->_class_addr;
        }

}




void CPTMallocator::PrintBins(int index)
{

        if (index < 0 || index >= ptmalloc_t::MAX_BINS)
        {
                return;
        }

        list_head_t* plist = &_bins[index]._lru;
	while (plist->_next != &_bins[index]._lru)
	{
	        plist = plist->_next;
                chunk_t* chunk = list_entry(plist, chunk_t, _lru);
                printf("%p 0x%llx 0x%0x 0x%0x\n", chunk, GetSize(chunk),
                        (unsigned int)(chunk->_size & ptmalloc_t::CHUNK_SIZE_LOW_MASK),
                        (unsigned int)((NextChunk(chunk)->_size) & ptmalloc_t::CHUNK_SIZE_LOW_MASK));
                if (GetSize(chunk) >= ptmalloc_t::MIN_SORT_BINS_SIZE)
                {
                        list_head_t* plist2 = &chunk->_lru2;
                        while (plist2->_next != &chunk->_lru2)
                        {
                                plist2 = plist2->_next;
                                chunk_t* chunk2 = list_entry(plist2, chunk_t, _lru2);
                                 printf("    %p 0x%llx 0x%0x 0x%0x\n", chunk2, GetSize(chunk2),
                                        (unsigned int)(chunk2->_size & ptmalloc_t::CHUNK_SIZE_LOW_MASK),
                                        (unsigned int)((NextChunk(chunk2)->_size) & ptmalloc_t::CHUNK_SIZE_LOW_MASK));
                        }
                }
        }
}

void CPTMallocator::PrintFastBins(int index)
{

        if (index < 0 || index >= ptmalloc_t::MAX_FAST_BINS)
        {
                return;
        }

        list_head_t* plist = &_fast_bins[index]._lru;
	while (plist->_next != &_fast_bins[index]._lru)
	{
	        plist = plist->_next;
                chunk_t* chunk = list_entry(plist, chunk_t, _lru);
                printf("%p 0x%llx 0x%0x 0x%0x\n", chunk, GetSize(chunk),
                        (unsigned int)(chunk->_size & ptmalloc_t::CHUNK_SIZE_LOW_MASK),
                        (unsigned int)((NextChunk(chunk)->_size) & ptmalloc_t::CHUNK_SIZE_LOW_MASK));
                if (GetSize(chunk) >= ptmalloc_t::MIN_SORT_BINS_SIZE)
                {
                        list_head_t* plist2 = &chunk->_lru2;
                        while (plist2->_next != &chunk->_lru2)
                        {
                                plist2 = plist2->_next;
                                chunk_t* chunk2 = list_entry(plist2, chunk_t, _lru2);
                                 printf("    %p 0x%llx 0x%0x 0x%0x\n", chunk2, GetSize(chunk2),
                                        (unsigned int)(chunk2->_size & ptmalloc_t::CHUNK_SIZE_LOW_MASK),
                                        (unsigned int)((NextChunk(chunk2)->_size) & ptmalloc_t::CHUNK_SIZE_LOW_MASK));
                        }
                }
        }
}

void* CPTMallocator::Creat()
{
        return CKMalloc::GetInstance().MallocPTMallocator();
}

void CPTMallocator::Destory(void*p)
{
        if (p)
        {
                CPTMallocator* pt = (CPTMallocator*)p;
                SHM_MT(SHM_GUARD(pt->_mutex, OBJ, SHM_ASSERT(0)));
                if (pt->_ptmalloc->_has_fast_bins)
                {
                        pt->CleanFastBins();
                }
                CKMalloc::GetInstance().FreePTMallocator(pt);
        }
}

void CPTMallocator::PthreadInit()
{
        pthread_key_create(&_pthread_key, CPTMallocator::Destory);
}

CPTMallocator* CPTMallocator::GetInstance()
{
#ifdef _REENTRANT_
        static bool first = true;
        if (first)
        {
                first = false;
                pthread_once(&_initflag,CPTMallocator::PthreadInit);
        }

        void* p = pthread_getspecific(_pthread_key);
        if (!p)
        {
                p = Creat();
                SHM_ASSERT(p);
                pthread_setspecific(_pthread_key,p);
                p = pthread_getspecific(_pthread_key);
        }
        SHM_ASSERT(p);
        return (CPTMallocator*)p;
#else
	static CPTMallocator* inst = 0;
	if (!inst)	
	{
		inst = (CPTMallocator*)Creat();
	}
	return inst;
#endif
}

void* CPTMallocator::Malloc(unsigned long long size)
{
        if (size == 0)
        {

                return 0;
        }
        CPTMallocator* ptmallocator = GetInstance();
        SHM_MT(SHM_GUARD(ptmallocator->_mutex, OBJ, SHM_ASSERT(0)));
        return ptmallocator->MallocImpl(size);
}

void* CPTMallocator::RMalloc(void *pold, unsigned long long size)
{
        CPTMallocator* ptmallocator = GetInstance();
        SHM_MT(SHM_GUARD(ptmallocator->_mutex, OBJ, SHM_ASSERT(0)));
        unsigned long long old_size = ptmallocator->MallocSize(pold);
        if (old_size == size)
        {
                return pold;
        }
        void* pnew = ptmallocator->MallocImpl(size);
        if (pnew)
        {
                unsigned long long copy_sz = old_size > size ? size : old_size;
                memcpy(pnew, pold, copy_sz);
        }
        ptmallocator->FreeImpl(pold);
        return pnew;
}
void CPTMallocator::Free(void *p)
{
        if (p == 0)
        {
                return;
        }

        CPTMallocator* ptmallocator = GetClassFromHeapInfo((unsigned long long)p);
        SHM_MT(SHM_GUARD_LOCKED(ptmallocator->_mutex, OBJ, SHM_ASSERT(0)));
        ptmallocator->FreeImpl(p);
}

unsigned long long CPTMallocator::MallocSize(void *p)
{
        if (p == 0)
        {
                return 0;
        }
        CPTMallocator* ptmallocator = GetClassFromHeapInfo((unsigned long long)p);
        SHM_MT(SHM_GUARD_LOCKED(ptmallocator->_mutex, OBJ, SHM_ASSERT(0)));
        return ptmallocator->MallocSizeImpl(p);
}

bool CPTMallocator::TestChunk()
{
        CPTMallocator* ptmallocator = GetInstance();
        SHM_MT(SHM_GUARD(ptmallocator->_mutex, OBJ, SHM_ASSERT(0)));
	ptmalloc_t* ptmalloc = ptmallocator->_ptmalloc;
        SHM_ASSERT(ptmalloc->_class_addr== (unsigned long long)ptmallocator);
	list_head_t* plist = &ptmalloc->_heap_lru;
	while (plist->_next != &ptmalloc->_heap_lru)
	{
		plist = plist->_next;
                ptheap_t* pheap = list_entry(plist, ptheap_t, _lru);
		SHM_ASSERT(pheap->_ptmalloc_t_addr == (unsigned long long)ptmalloc);
		ptmallocator->TestChunkImpl(pheap);
	}
	return true;
}

void* CPTMallocator::FastMalloc(size_t size)
{
        CPTMallocator* ptmallocator = GetInstance();
        SHM_MT(SHM_GUARD(ptmallocator->_mutex, OBJ, SHM_ASSERT(0)));
        return ptmallocator->FastMallocImpl(size);
}

void CPTMallocator::FastFree(void *p, size_t size)
{
        if (p == 0)
        {
                return;
        }
        CPTMallocator* ptmallocator = GetClassFromHeapInfo((unsigned long long)p);
        SHM_MT(SHM_GUARD_LOCKED(ptmallocator->_mutex, OBJ, SHM_ASSERT(0)));
        ptmallocator->FastFreeImpl(p, size);
}


bool CPTMallocator::TestChunkImpl(void*point)
{
        ptheap_t* pheap = (ptheap_t*)point;
        chunk_t* pchunk = 0;
        if (pheap->_ptmalloc_t_addr == ((unsigned long long)point + ptmalloc_t::HEAP_HEAD_SIZE))
        {
                pchunk = (chunk_t*)(pheap->_ptmalloc_t_addr + ptmalloc_t::MAX_PTMALLOC_SIZE);
        }
        else
        {
                pchunk = (chunk_t*)((char*)point + ptmalloc_t::HEAP_HEAD_SIZE);
                SHM_ASSERT(pchunk->_size & ptmalloc_t::CHUNK_MARK);
        }

        chunk_t* pnext_chunk = NextChunk(pchunk);
        while (!(pnext_chunk->_size & ptmalloc_t::CHUNK_MARK))
        {
                if (!IsUsed(pchunk))
                {
                        SHM_ASSERT(pnext_chunk->_pre_size == GetSize(pchunk));
                }
                pchunk = pnext_chunk;
                pnext_chunk =  NextChunk(pchunk);
        }
        if (!IsUsed(pchunk))
        {
                SHM_ASSERT(pnext_chunk->_pre_size == GetSize(pchunk));
        }
        SHM_ASSERT(!(pnext_chunk->_size&ptmalloc_t::CHUNK_SIZE_HIGH_MASK));
        return true;
}

void* CPTMallocator::FastMallocImpl(size_t size)
{
	if (size <= 1)
	{
		size = 2;
	}

	if (size >= ptmalloc_t::MAX_FIX_ALLOC_SIZE)
	{
		return MallocImpl(size);
	}

	if (!_fix_allocator[size])
	{
		SHM_ASSERT(!_ptmalloc->_fix_allocator_t[size]);
		_ptmalloc->_fix_allocator_t[size] = (fixallocator_t*)MallocImpl(sizeof(fixallocator_t));
		if (!_ptmalloc->_fix_allocator_t[size])
		{
			return 0;
		}

		_ptmalloc->_fix_allocator_t[size] = new ((void*)_ptmalloc->_fix_allocator_t[size]) fixallocator_t;

		if (_ptmalloc->_fix_allocator_t[size])
		{
			_fix_allocator[size] = new CFixAllocator(this, _ptmalloc->_fix_allocator_t[size], size);
		}
	}
	SHM_ASSERT(_fix_allocator[size]);

	return _fix_allocator[size]->Malloc();
}


void CPTMallocator::FastFreeImpl(void *p, size_t size)
{
        if (size <= 1)
	{
		size = 2;
	}

        if (size >= ptmalloc_t::MAX_FIX_ALLOC_SIZE)
        {
                return FreeImpl(p);
        }

        SHM_ASSERT(_fix_allocator[size]);
        _fix_allocator[size]->Free(p);
}


void CPTMallocator::ClearFastAllocator()
{
        SHM_ASSERT(!_ptmalloc->_fix_allocator_t[0] && !_ptmalloc->_fix_allocator_t[1]);
        for (int ix = 2; ix < ptmalloc_t::MAX_FIX_ALLOC_SIZE; ix++)
        {
                ClearFastAllocator(ix);
        }
}


void CPTMallocator::ClearFastAllocator(size_t size)
{
        if (!_fix_allocator[size])
        {
                SHM_ASSERT(!_ptmalloc->_fix_allocator_t[size]);
                return;
        }
        SHM_ASSERT(!_fix_allocator[size]->ObjCount());
        _fix_allocator[size]->ClearFreeList();
        SHM_ASSERT(!_fix_allocator[size]->TotalCount());
        delete _fix_allocator[size];
        _fix_allocator[size] = 0;
        _ptmalloc->_fix_allocator_t[size]->~fixallocator_t();
        FreeImpl(_ptmalloc->_fix_allocator_t[size]);
        _ptmalloc->_fix_allocator_t[size] = 0;
}



//是否按照理论调用各个构造函数还需要验证
CFixAllocator::CFixAllocator(CPTMallocator* alloc,fixallocator_t* fixallocator_data)
:_alloc(alloc), _fixallocator_data(fixallocator_data)
{
        _element_size = _fixallocator_data->_element_size;
}

CFixAllocator::CFixAllocator(CPTMallocator* alloc,fixallocator_t* fixallocator_data, int element_size)
:_alloc(alloc),_fixallocator_data(fixallocator_data),_element_size(element_size)
{
        _fixallocator_data->_element_size = element_size;
        _fixallocator_data->_count = 0;
        //AllocNewChunk();
}

void* CFixAllocator::operator new (size_t size)
{
        return malloc(size);
}

void CFixAllocator::operator delete (void* p)
{
        free(p);
}
};
