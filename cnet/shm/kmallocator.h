#ifndef    _KMALLOCATOR_H_
#define    _KMALLOCATOR_H_
#include "shmcom.h"
#include "lock.h"
namespace pwrdshmm
{
/******************************************** struct ********************************************/
#pragma pack(1)



typedef struct zone_t
{
	u16 _id;
	u16 _water;
	u16 _page_num;
	u16 _list_num;
	list_head_t _lru;
}zone_t;

typedef struct page_t
{
	enum{
		FLAG_INVAIL = 0,
                FLAG_UNMEM = 1,
		FLAG_FREE = 2,
		FLAG_USED = 4,
	};
	list_head_t _lru;
        u16 _id;
        u16 _flag;
	u16 _page_num;
	u16 _reserve;
	u64 _address;
        u64 _shm_address;
	page_t():_lru(),_flag(FLAG_INVAIL),_page_num(0),_reserve(0),_address(0){}
}page_t;

typedef struct kmalloc_t
{

        enum
        {
                CREATE_TYPE_USED = 0,
                CREATE_TYPE_STATIC = 1,
                CREATE_TYPE_DYNAMIC = 2,
        };

	enum
	{
	        SHM_KEY_SHIFT = 16,
		KMALLOC_SIZE = 4*1024*1024,
		DYNAMIAC_PAGE_HEAD_SIZE = 16,

                MAX_ZONE_NUM = 20,
		MAX_PAGE_NUM = 32*1024,
                MAX_THREAD_NUM = 20,

		MIN_PAGE_SIZE = 128*1024,
		//MIN_PAGE_SIZE = 1024,//for test

                MIN_MEM_ALLOC = 1024*1024*1024,
                //MIN_MEM_ALLOC = 1024*1024, //for test


	};
        static const unsigned long long MAX_PAGE_SIZE = 4*1024*1024*1024ull;
        static const unsigned long long MAX_MEM_ALLOC = 8*1024*1024*1024ull;
        u8  _dynamic_mm;
        u16 _dynameic_start_page_id;
        u16 _ver;
	u16 _used;
	u32 _free_page_address;
	u32 _zone_num;
	u32 _page_num;
        u32 _page_size;
        u32 _mm_key;
	u64 _mm_root;
        u64 _mm_size;
        u64 _page_size_mask;
        CPthreadMutex _mutex;
        CPthreadMutex _pt_mutex;
        page_t _page[MAX_PAGE_NUM];
        zone_t _zone[MAX_ZONE_NUM];
        zone_t _unmem_zone;
        list_head_t _ptmalloc_used_lru;
        list_head_t _ptmalloc_free_lru;
        u64 _ptmalloc[MAX_THREAD_NUM];
        u32 _ptmalloc_num;
        u32 _ptmalloc_used_num;
        u32 _ptmalloc_free_num;
}kmalloc_t;

#pragma pack()
/******************************************** end ********************************************/

/********************************************  ********************************************/
struct ptmalloc_t;
class CPTMallocator;
class CKernelMallocator
{
public:
	static CKernelMallocator& GetInstance();
	bool Init(int creat, int shm_key, unsigned long shm_root, unsigned long long shm_size, unsigned int zone_num, unsigned int page_size);

        void *Malloc(unsigned long long size);//need lock
        void Free(void* p);//need lock

        void *MallocPTMallocator();
        void FreePTMallocator(void*p);
        CPTMallocator* GetPTMallocatorWithID(int id);

	page_t* GetMemory(unsigned short page_num);
	void PutMemory(page_t* page);
        page_t* PointToPage(char* point);
        char *PageToPoint(page_t* page);


        inline unsigned short GetPageID(char *point)
        {
                return *(unsigned short *)((unsigned long)point);
        }

        inline unsigned int GetPageSize(page_t* page)
        {
                SHM_ASSERT(page->_flag != page_t::FLAG_INVAIL);
                /*
                return _dynamic_mm ? (page->_page_num << _page_shift) - kmalloc_t::DYNAMIAC_PAGE_HEAD_SIZE :
                                       page->_page_num << _page_shift;
                */
                return (page->_page_num << _page_shift) - kmalloc_t::DYNAMIAC_PAGE_HEAD_SIZE;
        }

        inline unsigned int GetPageNum()
        {
                return _page_num;
        }

        inline unsigned int GetMaxPageSize()
        {
                return 1 << (_zone_num-1);
        }

public:
	//helper fun
	bool TestMemoryPage(unsigned short& free_page, unsigned short& used_page, unsigned short& unmem_page);
        bool TestMemoryPage();
        bool TestAllMemoryPage(unsigned short& free_page, unsigned short& used_page, unsigned short& unmem_page);//this fun only used for gdb debug
        bool TestAllMemoryPage(); ////this fun only used for gdb debug
        bool TestUsedPage(char*point);
        //test
	void PrintZoneInfo();
        //end



private:
	CKernelMallocator();
	CKernelMallocator(const CKernelMallocator &rhs);
	CKernelMallocator & operator= (const CKernelMallocator &rhs);
private:
	inline unsigned int GetZoneIndex(unsigned int page_num)
	{
		for (unsigned int ix = 0; ix < _zone_num; ix++)
		{
			if (page_num <= _zone[ix]._page_num)
			{
				return ix;
			}
		}
		SHM_ASSERT(0);
		return 0;
	}

	inline page_t* GetPageFromZone(zone_t* zone)
	{
		SHM_ASSERT(zone->_list_num);
		zone->_list_num--;
		page_t* page = list_head(&zone->_lru,page_t,_lru);
		page->_lru.unlink();
                SHM_ASSERT(page->_flag == page_t::FLAG_FREE);
                page->_flag = page_t::FLAG_INVAIL;
		SHM_ASSERT(zone->_page_num == page->_page_num);
                SHM_ASSERT(!(page->_id & (page->_page_num-1)));
		return page;
	}

	inline page_t* GetPageFromZone(zone_t* zone, page_t* page)
	{
		SHM_ASSERT(zone->_list_num);
		zone->_list_num--;
		page->_lru.unlink();
                SHM_ASSERT(page->_flag == page_t::FLAG_FREE);
                page->_flag = page_t::FLAG_INVAIL;
		SHM_ASSERT(zone->_page_num == page->_page_num);
                SHM_ASSERT(!(page->_id & (page->_page_num-1)));
		return page;
	}

	inline void PutPageToZone( zone_t* zone, page_t* page )
	{
		SHM_ASSERT(zone->_page_num == page->_page_num);
                SHM_ASSERT(!(page->_id & (page->_page_num-1)));
		zone->_list_num++;
                page->_flag = page_t::FLAG_FREE;
                page->_lru.init();
		zone->_lru.push_front(page->_lru);
	}

	inline void PutPageToZoneBack( zone_t* zone, page_t* page )
	{
		SHM_ASSERT(zone->_page_num == page->_page_num);
                SHM_ASSERT(!(page->_id & (page->_page_num-1)));
		zone->_list_num++;
                page->_flag = page_t::FLAG_FREE;
                page->_lru.init();
		zone->_lru.push_back(page->_lru);
	}


        inline page_t* GetPageFromUnMemZone(zone_t* zone)
	{
		SHM_ASSERT(zone->_list_num);
		zone->_list_num--;
		page_t* page = list_head(&zone->_lru,page_t,_lru);
		page->_lru.unlink();
                SHM_ASSERT(page->_flag == page_t::FLAG_UNMEM);
                page->_flag = page_t::FLAG_INVAIL;
		SHM_ASSERT(zone->_page_num == page->_page_num);
                SHM_ASSERT(!(page->_id & (page->_page_num-1)));
		return page;
	}

	inline page_t* GetPageFromUnMemZone(zone_t* zone, page_t* page)
	{
		SHM_ASSERT(zone->_list_num);
		zone->_list_num--;
		page->_lru.unlink();
                SHM_ASSERT(page->_flag == page_t::FLAG_UNMEM);
                page->_flag = page_t::FLAG_INVAIL;
		SHM_ASSERT(zone->_page_num == page->_page_num);
                SHM_ASSERT(!(page->_id & (page->_page_num-1)));
		return page;
	}

	inline void PutPageToUnMemZone( zone_t* zone, page_t* page )
	{
		SHM_ASSERT(zone->_page_num == page->_page_num);
                SHM_ASSERT(!(page->_id & (page->_page_num-1)));
		zone->_list_num++;
                page->_flag = page_t::FLAG_UNMEM;
                page->_lru.init();
		zone->_lru.push_front(page->_lru);
	}

	inline void PutPageToUnMemZoneBack( zone_t* zone, page_t* page )
	{
		SHM_ASSERT(zone->_page_num == page->_page_num);
                SHM_ASSERT(!(page->_id & (page->_page_num-1)));
		zone->_list_num++;
                page->_flag = page_t::FLAG_UNMEM;
                page->_lru.init();
		zone->_lru.push_back(page->_lru);
	}


private:
	void PutPage(zone_t*zone,page_t* page);
	page_t* GetPage(unsigned short page_num);
	void ReturnMemory(page_t* page);
        void CollectMemory();
        void PageBindAddr(page_t* page, unsigned long long addr);
        unsigned long long GetPageAddr(page_t* page);
        void PutPTMallocator(ptmalloc_t* p);
        ptmalloc_t* GetPTMallocator();
        ptmalloc_t* GetPTMallocator(ptmalloc_t* ptmalloc);

        void PrintZoneInfo(zone_t *zone);
private:

	kmalloc_t* _pkmalloc;
        bool _dynamic_mm;
        unsigned int _mm_key;
        char* _mm_root;
        unsigned long long _mm_size;
	unsigned int _page_size;
	struct zone_t* _zone;
        struct zone_t* _zone_end;
        unsigned int _zone_num;
        struct zone_t* _unmem_zone;
	struct page_t* _page;
        struct page_t* _page_end;
        unsigned int _page_num;

        char* _static_addr_start;
        char* _static_addr_end;
        unsigned short _dynameic_start_page_id;
        CPthreadMutex* _mutex;
        CPthreadMutex* _pt_mutex;

        list_head_t* _ptmalloc_used_lru;
        list_head_t* _ptmalloc_free_lru;
        unsigned long long* _ptmalloc;
        unsigned int* _ptmalloc_num;
        unsigned int* _ptmalloc_used_num;
        unsigned int* _ptmalloc_free_num;

	//the left is not in shm
	unsigned int _page_shift;
        unsigned int _max_page_can_alloc;
        unsigned long long _page_size_mask;
	unsigned char _page_info[kmalloc_t::MAX_PAGE_NUM];
        CPTMallocator* _ptmallocator[kmalloc_t::MAX_THREAD_NUM];
};

void run();
};
#endif
