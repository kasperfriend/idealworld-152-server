#include "kmallocator.h"
#include "shmmgrimpl.h"
#include "ptmalloctor.h"
#include <new>
#include <malloc.h>
#include <stdio.h>
//test end
namespace pwrdshmm
{

CKernelMallocator::CKernelMallocator()
:_pkmalloc(0),_dynamic_mm(0),
_mm_key(0),_mm_root(0),_mm_size(0),_page_size(0),
_zone(0),_zone_end(0),_zone_num(0),_unmem_zone(0),
_page(0),_page_end(0),_page_num(0),
_static_addr_start(0),_static_addr_end(0),
_dynameic_start_page_id(0),_mutex(0),_pt_mutex(0),
_ptmalloc_used_lru(0),_ptmalloc_free_lru(0),
_ptmalloc(0),_ptmalloc_num(0),_ptmalloc_used_num(0),_ptmalloc_free_num(0),
_page_shift(0),_max_page_can_alloc(0),_page_size_mask(0)
{
}

CKernelMallocator& CKernelMallocator::GetInstance()
{
	static CKernelMallocator inst;

	return inst;
}


bool CKernelMallocator::Init(int create, int shm_key, unsigned long shm_root, unsigned long long shm_size, unsigned int zone_num, unsigned int page_size)
{
        if (!shm_root || shm_root & (kmalloc_t::KMALLOC_SIZE-1)
                || zone_num <=0 || zone_num > kmalloc_t::MAX_ZONE_NUM
                || page_size < kmalloc_t::MIN_PAGE_SIZE
                || page_size > kmalloc_t::MAX_PAGE_SIZE
                || sizeof(kmalloc_t) > kmalloc_t::KMALLOC_SIZE)
        {
                return false;
        }
        _pkmalloc = (kmalloc_t*)shm_root;

        if (create)
        {
                _pkmalloc->_mm_key = (shm_key%(1<<kmalloc_t::SHM_KEY_SHIFT)) << kmalloc_t::SHM_KEY_SHIFT;
                _pkmalloc->_mm_root = shm_root + kmalloc_t::KMALLOC_SIZE;
                _pkmalloc->_mm_size = shm_size - kmalloc_t::KMALLOC_SIZE;
                if (_pkmalloc->_mm_size <= kmalloc_t::MIN_MEM_ALLOC
                        || _pkmalloc->_mm_size > kmalloc_t::MAX_MEM_ALLOC)
                {
                        return false;
                }

                _pkmalloc->_zone_num = zone_num;
                unsigned int max_zone_page_num = 1 << (zone_num-1);
                if (_pkmalloc->_mm_size%max_zone_page_num)
                {
                        return false;
                }

                _pkmalloc->_page_size = page_size;
                _pkmalloc->_page_num =_pkmalloc->_mm_size/_pkmalloc->_page_size;
                if (_pkmalloc->_page_num > kmalloc_t::MAX_PAGE_NUM)
                {
                        return false;
                }
                _pkmalloc->_page_size_mask = (unsigned long long)page_size - 1;

                for (unsigned short ix = 0; ix < _pkmalloc->_zone_num; ix++)
                {
                        _pkmalloc->_zone[ix]._id = ix;
                        _pkmalloc->_zone[ix]._water = 0;
                        _pkmalloc->_zone[ix]._page_num = 1<<ix;
                        _pkmalloc->_zone[ix]._list_num = 0;
                        _pkmalloc->_zone[ix]._lru.init();
                }
                _pkmalloc->_unmem_zone._id = _pkmalloc->_zone_num;
                _pkmalloc->_unmem_zone._water = 0;
                _pkmalloc->_unmem_zone._page_num = 1<<(_pkmalloc->_zone_num-1);
                _pkmalloc->_unmem_zone._list_num = 0;
                _pkmalloc->_unmem_zone._lru.init();

                for (unsigned short ix = 0; ix < _pkmalloc->_page_num; ix++)
                {
                        _pkmalloc->_page[ix]._id = ix;
                        _pkmalloc->_page[ix]._flag = page_t::FLAG_INVAIL;
                        _pkmalloc->_page[ix]._page_num = 0;
                        _pkmalloc->_page[ix]._lru.init();
                        _pkmalloc->_page[ix]._address = 0;
                        _pkmalloc->_page[ix]._shm_address = 0;
                }

                if (create == kmalloc_t::CREATE_TYPE_STATIC)
                {
                        //pagebindaddr need the two var;
                        _pkmalloc->_dynameic_start_page_id = _pkmalloc->_page_num;
                        _pkmalloc->_dynamic_mm = 0;

                        page_t* page = _pkmalloc->_page;
                        zone_t* big_zone = &_pkmalloc->_zone[_pkmalloc->_zone_num-1];
	                unsigned short big_zone_num = (unsigned short)(_pkmalloc->_page_num/big_zone->_page_num);
                        unsigned long long addr = _pkmalloc->_mm_root;
                        unsigned int chunk_size = big_zone->_page_num*_pkmalloc->_page_size;
                        for (unsigned short ix = 0; ix < big_zone_num; ix++)
	                {
		                page->_page_num = big_zone->_page_num;
                                page->_address = addr;
		                PutPageToZoneBack(big_zone,page);
		                page += big_zone->_page_num;
                                addr += chunk_size;
	                }
                        SHM_ASSERT(page == _pkmalloc->_page + _pkmalloc->_page_num);
                }
                else
                {
                         //pagebindaddr need the two var;
                        page_t* page = _pkmalloc->_page;
                        zone_t* big_zone = &_pkmalloc->_zone[_pkmalloc->_zone_num-1];
	                unsigned short big_zone_num = (unsigned short)(kmalloc_t::MIN_MEM_ALLOC/_pkmalloc->_page_size/big_zone->_page_num);
                        unsigned short unmen_zone_num = (unsigned short)((_pkmalloc->_mm_size - kmalloc_t::MIN_MEM_ALLOC)/_pkmalloc->_page_size/big_zone->_page_num);
                        unsigned long long addr = _pkmalloc->_mm_root;
                        unsigned int chunk_size = big_zone->_page_num*_pkmalloc->_page_size;
                        for (unsigned int ix = 0; ix < big_zone_num; ix++)
	                {
		                page->_page_num = big_zone->_page_num;
                                page->_address = addr;
		                PutPageToZoneBack(big_zone,page);
		                page += page->_page_num;
                                addr += chunk_size;
	                }

                        _pkmalloc->_dynameic_start_page_id = page->_id;
                        _pkmalloc->_dynamic_mm = 1;

                        for (unsigned int ix = 0; ix < unmen_zone_num; ix++)
	                {
		                page->_page_num = big_zone->_page_num;
                                PutPageToUnMemZoneBack(&_pkmalloc->_unmem_zone,page);
		                page += page->_page_num;
	                }
                        SHM_ASSERT(page == _pkmalloc->_page + _pkmalloc->_page_num);
                }

                if (!_pkmalloc->_mutex.Init())
                {
                        return false;
                }


                if (!_pkmalloc->_pt_mutex.Init())
                {
                        return false;
                }

                _pkmalloc->_ptmalloc_used_lru.init();
                _pkmalloc->_ptmalloc_free_lru.init();
                _pkmalloc->_ptmalloc_num = 0;
                _pkmalloc->_ptmalloc_used_num = 0;
                _pkmalloc->_ptmalloc_free_num = 0;
        }

        if (create == kmalloc_t::CREATE_TYPE_USED && _pkmalloc->_dynamic_mm)
        {
                page_t* page = _pkmalloc->_page + _pkmalloc->_dynameic_start_page_id;
                unsigned short unmen_zone_num = (unsigned short)(
                        (_pkmalloc->_mm_size - kmalloc_t::MIN_MEM_ALLOC)/_pkmalloc->_page_size/_pkmalloc->_unmem_zone._page_num);
                unsigned long shmsize = _pkmalloc->_page_size * (_pkmalloc->_unmem_zone._page_num+1);
                for (unsigned int ix = 0; ix < unmen_zone_num; ix++)
                {
                        if (page->_flag != page_t::FLAG_UNMEM)
                        {
                                int shmkey = _pkmalloc->_mm_key | page->_id;
                                SHM_ASSERT(page->_address && page->_shm_address);
                                unsigned long long shm_addr = (unsigned long long )CShmMgrImpl::GetInstance().ShmReMatch(shmkey, (void*)page->_shm_address, shmsize);
                                if (!shm_addr)
                                {
                                        return false;
                                }
                                unsigned long long addr = (shm_addr + _pkmalloc->_page_size_mask) & ~(_pkmalloc->_page_size_mask);
                                if (shm_addr != page->_shm_address
                                    || *(unsigned short*)addr != page->_id)
                                {
                                        return false;
                                }
                        }
                        page = page + _pkmalloc->_unmem_zone._page_num;
                }
        }

        _mm_key = _pkmalloc->_mm_key;
        if (_mm_key != (unsigned int)shm_key<<kmalloc_t::SHM_KEY_SHIFT)
        {
                return false;
        }
        _mm_root = (char*)_pkmalloc->_mm_root;
        if ((unsigned long)_mm_root != shm_root + kmalloc_t::KMALLOC_SIZE)
        {
                return false;
        }
        _mm_size = _pkmalloc->_mm_size;
        if (_mm_size != shm_size - kmalloc_t::KMALLOC_SIZE)
        {
                return false;
        }

        _zone_num = _pkmalloc->_zone_num;
        if (_zone_num != zone_num)
        {
                return false;
        }

        _page_size = _pkmalloc->_page_size;
        if (_page_size != page_size)
        {
               return false;
        }

        _zone = _pkmalloc->_zone;
        _zone_end = _pkmalloc->_zone + _pkmalloc->_zone_num;
        _page = _pkmalloc->_page;
        _page_end = _pkmalloc->_page + _pkmalloc->_page_num;
        _page_num = _pkmalloc->_page_num;
        unsigned int page_num = _mm_size/page_size;
        if (_page_num != page_num)
        {
                return false;
        }
        _page_shift = -1;
        while (page_size)
        {
                page_size >>= 1;
                _page_shift++;
        }

        _unmem_zone = &_pkmalloc->_unmem_zone;
        _dynamic_mm = _pkmalloc->_dynamic_mm;

        if (_dynamic_mm)
        {
                _static_addr_start = _mm_root;
                _static_addr_end = _static_addr_start + kmalloc_t::MIN_MEM_ALLOC;
        }
        else
        {
                _static_addr_start = (char*)PageToPoint(_page) - kmalloc_t::DYNAMIAC_PAGE_HEAD_SIZE;
                _static_addr_end = (char*)PageToPoint(_page+_page_num) - kmalloc_t::DYNAMIAC_PAGE_HEAD_SIZE;
        }

        _dynameic_start_page_id = _pkmalloc->_dynameic_start_page_id;

        _max_page_can_alloc = (_zone+_zone_num-1)->_page_num << _page_shift;

        _page_size_mask = _pkmalloc->_page_size_mask;

        _ptmalloc_used_lru = &_pkmalloc->_ptmalloc_used_lru;
        _ptmalloc_free_lru = &_pkmalloc->_ptmalloc_free_lru;
        _ptmalloc = (unsigned long long*)_pkmalloc->_ptmalloc;
        _ptmalloc_num = &_pkmalloc->_ptmalloc_num;
        _ptmalloc_used_num = &_pkmalloc->_ptmalloc_used_num;
        _ptmalloc_free_num = &_pkmalloc->_ptmalloc_free_num;

        //SHM_ASSERT(_ptmalloc_used_lru->_next == _ptmalloc_used_lru);
        while (_ptmalloc_used_lru->_next != _ptmalloc_used_lru)
        {
                ptmalloc_t* p = list_head(_ptmalloc_used_lru,ptmalloc_t,_lru);
                SHM_ASSERT(p->_mutex.Trylock());
                SHM_ASSERT(p->_mutex.Unlock());
                PutPTMallocator(p);
        }

        if (*_ptmalloc_used_num + *_ptmalloc_free_num != *_ptmalloc_num)
        {
                return false;
        }

        _mutex = &_pkmalloc->_mutex;
        if (!_mutex->Trylock())
        {
                return false;
        }
        if (!_mutex->Unlock())
        {
                return false;
        }
        _pt_mutex = &_pkmalloc->_pt_mutex;
        if (!_pt_mutex->Trylock())
        {
                return false;
        }
        if (!_pt_mutex->Unlock())
        {
                return false;
        }


        for (int ix = 0; ix < kmalloc_t::MAX_THREAD_NUM; ix++)
        {
                _ptmallocator[ix] = 0;
        }

        unsigned short free_page,used_page,unmem_page;
        if (!TestMemoryPage(free_page, used_page, unmem_page))
        {
                return false;
        }

        char temp = _static_addr_end[-1];
        _static_addr_end[-1] = 0xff;
        _static_addr_end[-1] = temp;


        return true;
}

void CKernelMallocator::PageBindAddr(page_t* page, unsigned long long addr)
{
        if (_dynamic_mm && page->_id >= _dynameic_start_page_id)
	{
                page->_address = addr;
                *((unsigned short*)addr) = page->_id;
        }
        else
        {
                page->_address = addr;
        }
}

unsigned long long CKernelMallocator::GetPageAddr(page_t* page)
{
        if (_dynamic_mm && page->_id >= _dynameic_start_page_id)
        {
                return page->_address;
        }
        else
        {
                return (unsigned long long)(_mm_root + ((unsigned long)(page-_page) << _page_shift));
        }
}

void CKernelMallocator::PutPTMallocator(ptmalloc_t* p)
{
        SHM_ASSERT(*_ptmalloc_used_num && p->_used == 1);
        p->_lru.unlink();
        p->_used = 0;
        p->_class_addr = 0;
        _ptmalloc_free_lru->push_front(p->_lru);
        (*_ptmalloc_used_num)--;
        (*_ptmalloc_free_num)++;
}

ptmalloc_t* CKernelMallocator::GetPTMallocator()
{
        SHM_ASSERT(*_ptmalloc_free_num && _ptmalloc_free_lru->_next != _ptmalloc_free_lru);
        ptmalloc_t* ptmalloc = list_head(_ptmalloc_free_lru,ptmalloc_t,_lru);
        return GetPTMallocator(ptmalloc);
}

ptmalloc_t* CKernelMallocator::GetPTMallocator(ptmalloc_t* ptmalloc)
{
        SHM_ASSERT(*_ptmalloc_free_num && _ptmalloc_free_lru->_next != _ptmalloc_free_lru);
        SHM_ASSERT(ptmalloc->_used == 0 && ptmalloc->_class_addr == 0);
        ptmalloc->_lru.unlink();
        ptmalloc->_used = 1;
        _ptmalloc_used_lru->push_front(ptmalloc->_lru);
        (*_ptmalloc_used_num)++;
        (*_ptmalloc_free_num)--;
        return ptmalloc;
}




char *CKernelMallocator::PageToPoint(page_t* page)
{
	if (_dynamic_mm && page->_id >= _dynameic_start_page_id)
	{
                SHM_ASSERT(*(unsigned short *)page->_address == page->_id);
                return (char*)(page->_address + kmalloc_t::DYNAMIAC_PAGE_HEAD_SIZE);
	}
	else
	{
		return _mm_root + ((unsigned long)(page-_page) << _page_shift) + kmalloc_t::DYNAMIAC_PAGE_HEAD_SIZE;
	}

}

page_t* CKernelMallocator::PointToPage(char* point)
{
        point -= kmalloc_t::DYNAMIAC_PAGE_HEAD_SIZE;
	if (!_dynamic_mm || (point >= _static_addr_start && point < _static_addr_end))
	{
		return _page+((unsigned long)(point - _mm_root)>>_page_shift);
	}
	else
	{
		unsigned long long addr = (unsigned long long)(point);
		unsigned short page_id = *(unsigned short *)addr;
		SHM_ASSERT(page_id < _page_num && _page[page_id]._address == addr);
		return _page+page_id;
	}
}

page_t* CKernelMallocator::GetPage( unsigned short page_num )
{
	unsigned int zone_index = 0;
	page_num--;
	while(page_num)
	{
		page_num >>= 1;
		zone_index++;
	}

        if (_zone + zone_index >= _zone_end)
        {
                return 0;
        }

	for(zone_t* zone = _zone + zone_index; zone < _zone_end; zone++)
	{
		if (zone->_list_num)
		{
			return GetPageFromZone(zone);
		}
	}

        CollectMemory();
        for(zone_t* zone = _zone + zone_index; zone < _zone_end; zone++)
	{
		if (zone->_list_num)
		{
			return GetPageFromZone(zone);
		}
	}

        if (_dynamic_mm)
        {
                if (!_unmem_zone->_list_num)
                {
                     return 0;
                }
                page_t* page = list_head(&_unmem_zone->_lru,page_t,_lru);
                int shmkey = _mm_key | page->_id;
                unsigned long size = (page->_page_num+1) << _page_shift; //it's waste mm
                unsigned long long addr = (unsigned long long)CShmMgrImpl::GetInstance().ShmMatch(shmkey, size);
                if (!addr)
                {
                        return 0;
                }
                page = GetPageFromUnMemZone(_unmem_zone, page);
                page->_shm_address = addr;
                addr = (addr + _page_size_mask) & ~(_page_size_mask);
                PageBindAddr(page, addr);
                return page;
        }
	return 0;
}

void CKernelMallocator::ReturnMemory( page_t* page )
{
	SHM_ASSERT(page->_page_num);
	unsigned short num = page->_page_num;
        unsigned long long addr = page->_address;
	for(unsigned int ix = 0; num && ix < _zone_num; ix++)
	{
		if (num & 1)
		{
			page->_page_num = _zone[ix]._page_num;
                        PageBindAddr(page, addr);
			PutPage(_zone+ix, page);
			page += _zone[ix]._page_num;
                        addr = addr + (_zone[ix]._page_num<<_page_shift);
		}
		num >>= 1;
	}
        SHM_ASSERT(!num);
}

page_t* CKernelMallocator::GetMemory( unsigned short page_num )
{
        if (page_num > _max_page_can_alloc)
        {
                return 0;
        }
	page_t* page = GetPage(page_num);
	if (!page)
	{
		return 0;
	}
	page->_flag = page_t::FLAG_USED;
	int left_page_num = page->_page_num - page_num;
	page->_page_num = page_num;

	if( left_page_num == 0)
	{
		return page;
	}
        unsigned long long addr = GetPageAddr(page);
	page_t* left_page = page+page_num;
	left_page->_page_num = left_page_num;
        addr = addr + (page->_page_num<<_page_shift);
        PageBindAddr(left_page, addr);
	ReturnMemory(left_page);
	return page;
}

void CKernelMallocator::PutPage(zone_t* zone, page_t* page)
{
	SHM_ASSERT(page->_page_num == zone->_page_num);

	page->_page_num = zone->_page_num;
	//need check

        if (_dynamic_mm && zone == _zone_end - 1
                && page->_id >= _dynameic_start_page_id)
        {
                SHM_ASSERT(page->_address && page->_shm_address);
                SHM_ASSERT(page->_id == *(unsigned short*)page->_address);
                CShmMgrImpl::GetInstance().Shmdt((void *)page->_shm_address);
                page->_shm_address = 0;
                page->_address = 0;
                PutPageToUnMemZone(_unmem_zone,page);
                return;
        }

	unsigned short buddy_page_index = (unsigned short)(page->_id) ^ (1 << zone->_id);
	page_t* buddy_page = _page + buddy_page_index;

	if (buddy_page->_flag == page_t::FLAG_FREE
                && buddy_page->_page_num == page->_page_num
                && zone->_id < _zone_num-1)
	{
		GetPageFromZone(zone,buddy_page);
		if (zone->_list_num >= zone->_water)
		{
			page_t* big_page = page < buddy_page ? page : buddy_page;
			big_page->_page_num <<= 1;
			PutPage(zone+1, big_page);
		}
		else
		{
			PutPageToZoneBack(zone,page);
			PutPageToZoneBack(zone,buddy_page);
		}
	}
	else
	{
		PutPageToZoneBack(zone,page);
	}
}

void CKernelMallocator::PutMemory( page_t* page )
{
        if (!page)
        {
                return;
        }
        SHM_ASSERT(page->_flag == page_t::FLAG_USED);
	SHM_ASSERT(page->_page_num <= _zone[_zone_num-1]._page_num);
	page->_flag = page_t::FLAG_INVAIL;

	unsigned short zone_id = 0;
	while (page->_page_num > _zone[zone_id]._page_num)
	{
		zone_id++;
	}
	if (page->_page_num == _zone[zone_id]._page_num)
	{
		PutPage(_zone + zone_id, page);
		return;
	}

	page_t* buddy_page = page + page->_page_num;
	if (buddy_page < _page_end
		&& buddy_page->_flag == page_t::FLAG_FREE
		&& buddy_page->_page_num + page->_page_num == _zone[zone_id]._page_num
		&& buddy_page->_page_num != page->_page_num)
	{
		unsigned short buddy_zone_id = 0;
		for (; buddy_zone_id < _zone_num; buddy_zone_id++)
		{
			if (_zone[buddy_zone_id]._page_num == buddy_page->_page_num)
			{
				break;
			}
		}
		SHM_ASSERT(buddy_zone_id < _zone_num);
		buddy_page = GetPageFromZone(_zone+buddy_zone_id,buddy_page);
		SHM_ASSERT(buddy_page > page);
		page->_page_num += buddy_page->_page_num;
		PutPage(_zone + zone_id, page);
	}
	else
	{
		unsigned short left_page = page->_page_num;
                unsigned long long addr = GetPageAddr(page);
		for (short ix = (short)(zone_id-1); left_page && ix >= 0; ix--)
		{
			if (left_page >= _zone[ix]._page_num)
			{
				page->_page_num = _zone[ix]._page_num;
                                PageBindAddr(page, addr);
				PutPage(_zone + ix, page);
				page = page + _zone[ix]._page_num;
                                addr = addr + (_zone[ix]._page_num<<_page_shift);
                                left_page -= _zone[ix]._page_num;
			}
		}
		SHM_ASSERT(!left_page);
	}
}

void CKernelMallocator::CollectMemory()
{
        for (zone_t* zone = _zone; zone < _zone_end-1; zone++)
        {
                list_head_t* plist = &zone->_lru;
		while (plist->_pre != &zone->_lru)
		{
                        plist = plist->_pre;
                        page_t* page = list_entry(plist, page_t, _lru);
                        unsigned short buddy_page_index = (unsigned short)(page->_id) ^ (1 << zone->_id);
	                page_t* buddy_page = _page + buddy_page_index;
                        if (buddy_page->_flag == page_t::FLAG_FREE &&
                               buddy_page->_page_num == page->_page_num)
                        {
                                plist = plist->_next; //it may check the same list again, but it's ok;
                                page = GetPageFromZone(zone, page);
                                buddy_page = GetPageFromZone(zone, buddy_page);
                                page = page < buddy_page ? page : buddy_page;
                                page->_page_num <<= 1;
                                PutPage(zone+1, page);
                                continue;
                        }
		}
        }
}

void *CKernelMallocator::Malloc(unsigned long long size)
{
        SHM_MT(SHM_GUARD(_mutex, OBJ, SHM_ASSERT(0)));
        unsigned long long malloc_size = size;
        //if (_dynamic_mm)
        {
                malloc_size += kmalloc_t::DYNAMIAC_PAGE_HEAD_SIZE;
        }
        if (malloc_size < size)
        {
                return 0;
        }

        unsigned short page_num = (malloc_size + _page_size - 1) >> _page_shift;
        if (page_num > _max_page_can_alloc)
        {
                return 0;
        }

        unsigned short malloc_page_num = 1;
        while (page_num > malloc_page_num)
        {
                malloc_page_num <<= 1;
        }


        page_t* page =  GetMemory(malloc_page_num);
        if (!page)
        {
                return 0;
        }

        return PageToPoint(page);
}

void CKernelMallocator::Free(void* p)
{
        SHM_MT(SHM_GUARD(_mutex, OBJ, SHM_ASSERT(0)));
        page_t *page = PointToPage((char*)p);
        SHM_ASSERT(page);
        PutMemory(page);
}

void *CKernelMallocator::MallocPTMallocator()
{
        SHM_MT(SHM_GUARD(_pt_mutex, LOCKOBJ, SHM_ASSERT(0)));
        if ((*_ptmalloc_num) >= kmalloc_t::MAX_THREAD_NUM)
        {
                return 0;
        }

        for (unsigned int i = 0; i < *_ptmalloc_num; i++)
        {
                if (_ptmallocator[i] && !_ptmallocator[i]->IsPthreadOwn())
                {
                        _ptmallocator[i]->SetPthreadOwn();
                        return _ptmallocator[i];
                }
        }

        //avoid call the operator new which overload
        CPTMallocator* ptmallocator = (CPTMallocator*)malloc(sizeof(CPTMallocator));
        if (!ptmallocator)
        {
                return 0;
        }
        ptmallocator = new ((void*)ptmallocator) CPTMallocator;

        ptmalloc_t* p = 0;
        if (*_ptmalloc_free_num)
        {

                p = GetPTMallocator();
                SHM_ASSERT(p);
                if (!ptmallocator->Init(0, p))
                {
                        PutPTMallocator(p);
                        ptmallocator->~CPTMallocator();
                        free(ptmallocator);
                        SHM_ASSERT(0);
                }
        }
        else
        {
                if (!ptmallocator->Init(1,0))
                {
                        ptmallocator->~CPTMallocator();
                        free(ptmallocator);
                        SHM_ASSERT(0);
                }
                p =  ptmallocator->GetPTMalloc();
                p->_id = *_ptmalloc_num;
                _ptmalloc[(*_ptmalloc_num)] = (unsigned long long)p;
                (*_ptmalloc_num)++;
                p->_lru.init();
                p->_used = 1;
                _ptmalloc_used_lru->push_front(p->_lru);
                (*_ptmalloc_used_num)++;
        }
        SHM_ASSERT(!_ptmallocator[p->_id]);
        ptmallocator->SetPthreadOwn();
        _ptmallocator[p->_id] = ptmallocator;
        SHM_ASSERT(*_ptmalloc_free_num + *_ptmalloc_used_num == *_ptmalloc_num);
        return ptmallocator;
}


void CKernelMallocator::FreePTMallocator(void *point)
{
        if (!point)
        {
                return;
        }
        SHM_MT(SHM_GUARD(_pt_mutex, OBJ, SHM_ASSERT(0)));
        CPTMallocator* ptmallocator = (CPTMallocator*)point;
        ptmalloc_t* p =  ptmallocator->GetPTMalloc();
        SHM_ASSERT(_ptmallocator[p->_id] == ptmallocator
                && p->_class_addr == (unsigned long long)ptmallocator);

        ptmallocator->ClearPthreadOwn();
        _ptmallocator[p->_id] = 0;
        PutPTMallocator(p);
        ptmallocator->~CPTMallocator();
        free(point);
        SHM_ASSERT(*_ptmalloc_free_num + *_ptmalloc_used_num == *_ptmalloc_num);
}

CPTMallocator* CKernelMallocator::GetPTMallocatorWithID(int id)
{
        SHM_MT(SHM_GUARD(_pt_mutex, OBJ, SHM_ASSERT(0)));
        SHM_ASSERT(id >=0 && (unsigned int)id < *_ptmalloc_num);
        if (_ptmallocator[id])
        {
                SHM_ASSERT(_ptmallocator[id]->GetPTMalloc()->_class_addr == (unsigned long long)_ptmallocator[id]);
                return _ptmallocator[id];
        }
        CPTMallocator* ptmallocator = (CPTMallocator*)malloc(sizeof(CPTMallocator));
        if (!ptmallocator)
        {
                return 0;
        }
        ptmallocator = new((void*)ptmallocator)CPTMallocator;

        ptmalloc_t*p = (ptmalloc_t*)_ptmalloc[id];
        p = GetPTMallocator(p);
        if (!ptmallocator->Init(0, p))
        {
                PutPTMallocator(p);
                ptmallocator->~CPTMallocator();
                free(ptmallocator);
                SHM_ASSERT(0);
        }
        _ptmallocator[id] = ptmallocator;
        SHM_ASSERT(*_ptmalloc_free_num + *_ptmalloc_used_num == *_ptmalloc_num);
        return ptmallocator;
}


bool CKernelMallocator::TestMemoryPage()
{
        unsigned short used_page,free_page,unmem_page;
        return TestMemoryPage(free_page, used_page, unmem_page);
}

bool CKernelMallocator::TestMemoryPage(unsigned short& free_page, unsigned short& used_page, unsigned short& unmem_page)
{
        SHM_MT(SHM_GUARD(_mutex, OBJ, SHM_ASSERT(0)));
        SHM_ASSERT(*_ptmalloc_free_num + *_ptmalloc_used_num == *_ptmalloc_num);
	used_page = 0;
	free_page = 0;
        unmem_page = 0;
	for (unsigned short ix = 0; ix < kmalloc_t::MAX_PAGE_NUM; ix++)
	{
		_page_info[ix] = page_t::FLAG_INVAIL;
	}

	for (unsigned int ix = 0; ix < _zone_num; ix++)
	{
		list_head_t* plist = &_zone[ix]._lru;
		while (plist->_next != &_zone[ix]._lru)
		{
			plist = plist->_next;
			page_t* page = list_entry(plist, page_t, _lru);
                        char *point = PageToPoint(page);
                        page_t*temp_page = PointToPage(point);
                        if (page != temp_page)
                        {
                                SHM_ASSERT(0);
                                return false;
                        }
			if (page->_flag != page_t::FLAG_FREE)
			{
                                SHM_ASSERT(0);
				return false;
			}
			unsigned short begin_id =  page->_id;
			for (unsigned short iy = 0; iy < page->_page_num; iy++)
			{
                                if (iy > 0 && page[iy]._flag != page_t::FLAG_INVAIL)
                                {
                                        SHM_ASSERT(0);
                                        return false;
                                }
				if (_page_info[begin_id + iy] != page_t::FLAG_INVAIL)
				{
                                        SHM_ASSERT(0);
					return false;
				}
				_page_info[begin_id + iy] = page_t::FLAG_FREE;
			}
			free_page += page->_page_num;
		}
	}

        list_head_t* plist = &_unmem_zone->_lru;
        while (plist->_next != &_unmem_zone->_lru)
	{
	        plist = plist->_next;
		page_t* page = list_entry(plist, page_t, _lru);
		if (page->_flag != page_t::FLAG_UNMEM)
		{
                        SHM_ASSERT(0);
			return false;
		}
		unsigned short begin_id =  page->_id;
		for (unsigned short iy = 0; iy < page->_page_num; iy++)
		{
                        if (iy > 0 && page[iy]._flag != page_t::FLAG_INVAIL)
                        {
                                SHM_ASSERT(0);
                                return false;
                        }
			if (_page_info[begin_id + iy] != page_t::FLAG_INVAIL)
			{
                                SHM_ASSERT(0);
				return false;
			}
			_page_info[begin_id + iy] = page_t::FLAG_UNMEM;
		}
		unmem_page += page->_page_num;
	}

	for (unsigned short ix = 0; ix < _page_num; ix++)
	{
		page_t* page = _page + ix;
		if (page->_flag >= page_t::FLAG_USED)
		{
                        char *point = PageToPoint(page);
                        page_t*temp_page = PointToPage(point);
                        if (page != temp_page)
                        {
                                SHM_ASSERT(0);
                                return false;
                        }
			unsigned short begin_id =  page->_id;
			for (unsigned short iy = 0; iy < page->_page_num; iy++)
			{
                                if (iy > 0 && page[iy]._flag != page_t::FLAG_INVAIL)
                                {
                                        SHM_ASSERT(0);
                                        return false;
                                }
				if (_page_info[begin_id + iy] != page_t::FLAG_INVAIL)
				{
                                        SHM_ASSERT(0);
					return false;
				}
				_page_info[begin_id + iy] = page_t::FLAG_USED;
			}
			used_page += page->_page_num;
		}
	}

	for (unsigned short ix = 0; ix < _page_num; ix++)
	{
		if (_page_info[ix] == page_t::FLAG_INVAIL)
		{
                        SHM_ASSERT(0);
			return false;
		}
	}

        if (free_page + used_page + unmem_page != (unsigned short)_page_num)
        {
                SHM_ASSERT(0);
                return false;
        }

	return true;
}

void CKernelMallocator::PrintZoneInfo()
{
        SHM_MT(SHM_GUARD(_mutex, OBJ, SHM_ASSERT(0)));
	for (unsigned int ix = 0; ix < _zone_num; ix++)
	{
		PrintZoneInfo(_zone+ix);
	}

        PrintZoneInfo(_unmem_zone);
}

void CKernelMallocator::PrintZoneInfo(zone_t *zone)
{
	printf("zone:id = %d  pagenum = %d water = %d  listnum = %d\n",zone->_id, zone->_page_num,
				zone->_water, zone->_list_num);

	list_head_t* plist = &zone->_lru;
	while (plist->_next != &zone->_lru)
	{
		plist = plist->_next;
		page_t* page = list_entry(plist, page_t, _lru);
		printf("page id %d  pagenum %d flag %d addr 0x%llx shm_addr 0x%llx\n",page->_id, page->_page_num, page->_flag, page->_address, page->_shm_address);
	}
	printf("\n");
}

bool CKernelMallocator::TestAllMemoryPage()
{
        unsigned short used_page,free_page,unmem_page;
        return TestMemoryPage(free_page, used_page, unmem_page);
}
bool CKernelMallocator::TestAllMemoryPage(unsigned short& free_page, unsigned short& used_page, unsigned short& unmem_page)
{
        SHM_MT(SHM_GUARD(_mutex, OBJ, SHM_ASSERT(0)));
        SHM_ASSERT(*_ptmalloc_free_num + *_ptmalloc_used_num == *_ptmalloc_num);
	used_page = 0;
	free_page = 0;
        unmem_page = 0;
	for (unsigned short ix = 0; ix < kmalloc_t::MAX_PAGE_NUM; ix++)
	{
		_page_info[ix] = page_t::FLAG_INVAIL;
	}
	for (unsigned int ix = 0; ix < _zone_num; ix++)
	{
		list_head_t* plist = &_zone[ix]._lru;
		while (plist->_next != &_zone[ix]._lru)
		{
			plist = plist->_next;
			page_t* page = list_entry(plist, page_t, _lru);
                        char *point = PageToPoint(page);
                        page_t*temp_page = PointToPage(point);
                        if (page != temp_page)
                        {
                                SHM_ASSERT(0);
                                return false;
                        }
			if (page->_flag != page_t::FLAG_FREE)
			{
                                SHM_ASSERT(0);
				return false;
			}
			unsigned short begin_id =  page->_id;
			for (unsigned short iy = 0; iy < page->_page_num; iy++)
			{
                                if (iy > 0 && page[iy]._flag != page_t::FLAG_INVAIL)
                                {
                                        SHM_ASSERT(0);
                                        return false;
                                }
				if (_page_info[begin_id + iy] != page_t::FLAG_INVAIL)
				{
                                        SHM_ASSERT(0);
					return false;
				}
				_page_info[begin_id + iy] = page_t::FLAG_FREE;
			}
			free_page += page->_page_num;
		}
	}
        list_head_t* plist = &_unmem_zone->_lru;
        while (plist->_next != &_unmem_zone->_lru)
	{
	        plist = plist->_next;
		page_t* page = list_entry(plist, page_t, _lru);
		if (page->_flag != page_t::FLAG_UNMEM)
		{
                        SHM_ASSERT(0);
			return false;
		}
		unsigned short begin_id =  page->_id;
		for (unsigned short iy = 0; iy < page->_page_num; iy++)
		{
                        if (iy > 0 && page[iy]._flag != page_t::FLAG_INVAIL)
                        {
                                SHM_ASSERT(0);
                                return false;
                        }
			if (_page_info[begin_id + iy] != page_t::FLAG_INVAIL)
			{
                                SHM_ASSERT(0);
				return false;
			}
			_page_info[begin_id + iy] = page_t::FLAG_UNMEM;
		}
		unmem_page += page->_page_num;
	}
	for (unsigned short ix = 0; ix < _page_num; ix++)
	{
		page_t* page = _page + ix;
		if (page->_flag >= page_t::FLAG_USED)
		{
                        char *point = PageToPoint(page);
                        page_t*temp_page = PointToPage(point);
                        if (page != temp_page || !TestUsedPage(point))
                        {
                                SHM_ASSERT(0);
                                return false;
                        }
			unsigned short begin_id =  page->_id;
			for (unsigned short iy = 0; iy < page->_page_num; iy++)
			{
                                if (iy > 0 && page[iy]._flag != page_t::FLAG_INVAIL)
                                {
                                        SHM_ASSERT(0);
                                        return false;
                                }
				if (_page_info[begin_id + iy] != page_t::FLAG_INVAIL)
				{
                                        SHM_ASSERT(0);
					return false;
				}
				_page_info[begin_id + iy] = page_t::FLAG_USED;
			}
			used_page += page->_page_num;
		}
	}
	for (unsigned short ix = 0; ix < _page_num; ix++)
	{
		if (_page_info[ix] == page_t::FLAG_INVAIL)
		{
                        SHM_ASSERT(0);
			return false;
		}
	}
        if (free_page + used_page + unmem_page != (unsigned short)_page_num)
        {
                SHM_ASSERT(0);
                return false;
        }
	return true;
}
bool CKernelMallocator::TestUsedPage(char*point)
{
        ptheap_t* pheap = (ptheap_t*)point;
        chunk_t* pchunk = 0;
        if (pheap->_ptmalloc_t_addr == (unsigned long long)(point + ptmalloc_t::HEAP_HEAD_SIZE))
        {
                pchunk = (chunk_t*)(pheap->_ptmalloc_t_addr + ptmalloc_t::MAX_PTMALLOC_SIZE);
        }
        else
        {
                pchunk = (chunk_t*)(point + ptmalloc_t::HEAP_HEAD_SIZE);
                SHM_ASSERT(pchunk->_size & ptmalloc_t::CHUNK_MARK);
        }
        chunk_t* pnext_chunk = (chunk_t*)((char*)pchunk+(pchunk->_size&ptmalloc_t::CHUNK_SIZE_HIGH_MASK));
        while (!(pnext_chunk->_size & ptmalloc_t::CHUNK_MARK))
        {
                if (!(pnext_chunk->_size&ptmalloc_t::PRE_USED))
                {
                        SHM_ASSERT(pnext_chunk->_pre_size == (pchunk->_size&ptmalloc_t::CHUNK_SIZE_HIGH_MASK));
                }
                pchunk = pnext_chunk;
                pnext_chunk = (chunk_t*)((char*)pchunk+(pchunk->_size&ptmalloc_t::CHUNK_SIZE_HIGH_MASK));
        }
        if (!(pnext_chunk->_size&ptmalloc_t::PRE_USED))
        {
                SHM_ASSERT(pnext_chunk->_pre_size == (pchunk->_size&ptmalloc_t::CHUNK_SIZE_HIGH_MASK));
        }
	SHM_ASSERT(!(pnext_chunk->_size&ptmalloc_t::CHUNK_SIZE_HIGH_MASK));
        return true;
}
void run()
{

}



};


