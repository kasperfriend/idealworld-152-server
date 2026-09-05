#include "kmallocator.h"
#include "shmmgrimpl.h"
#include "ptmalloctor.h"
#include "kmalloc.h"
#include "ptmalloc.h"
//test
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <memory.h>
#include <sys/time.h>
#include <unistd.h>
#include <list>

namespace pwrdshmm
{
int init()
{
        int creat = 2;
        int shm_key = 103;
        unsigned long long shm_root = 0x50000000 - shmmm_t::SHMMM_SIZE - kmalloc_t::KMALLOC_SIZE;
        //unsigned long long shm_size = 0x400000 + shmmm_t::SHMMM_SIZE + kmalloc_t::KMALLOC_SIZE;
        //int page_num = 1024;
        unsigned long long shm_size = 0xb8000000 + shmmm_t::SHMMM_SIZE + kmalloc_t::KMALLOC_SIZE;
        unsigned long long first_shm_size = 0x40000000 + shmmm_t::SHMMM_SIZE + kmalloc_t::KMALLOC_SIZE;
        int page_num = 1024*1024;

        if (!CShmMgrImpl::GetInstance().Init(creat, shm_key, (char *)shm_root, first_shm_size))
        {
                printf("shmmgrimpl fail\n");
                return 0;
        }
	if (!CKernelMallocator::GetInstance().Init(creat, shm_key ,shm_root + shmmm_t::SHMMM_SIZE, shm_size - shmmm_t::SHMMM_SIZE, 8, page_num))
	{
                printf("kmalloc fail\n");
                return 0;
	}
        unsigned short used_page,free_page,unmem_page;
        if (!CKernelMallocator::GetInstance().TestMemoryPage(free_page,used_page,unmem_page))
	{
		printf("errrrrrr\n");
	}else
	{
		printf("init ok free = %d used = %d\n",free_page, used_page);
	}
        CKernelMallocator::GetInstance().PrintZoneInfo();
        return 0;
}
int start_shm_init()
{
	static int shminit = init();
        return shminit;
}

int BaseTest()
{
        //base test
         int max_page_size = (int)CKernelMallocator::GetInstance().GetMaxPageSize();
         int page_num = (int)CKernelMallocator::GetInstance().GetPageNum();
         //int page_size = (int)CKernelMallocator::GetInstance().GetPageSize();
        page_t** page = new page_t* [page_num];
	unsigned short used_page,free_page,unmem_page;

        for (int ix = 1; ix <= max_page_size; ix++)
        {
                int now_page_num = max_page_size/ix+1;
                for (int iy = 0; iy < now_page_num; iy++)
                {
                        page[iy] = CKernelMallocator::GetInstance().GetMemory(ix);
                        if (page[iy])
                        {
                                char* p = (char*)CKernelMallocator::GetInstance().PageToPoint(page[iy]);
                                page_t* ptemp_page = CKernelMallocator::GetInstance().PointToPage(p);
                                if (ptemp_page != page[iy])
                                {
                                        assert(0);
                                }
                                //memset(p,0xff, CKernelMallocator::GetInstance().GetPageSize(page[iy]));

                        }
                        CKernelMallocator::GetInstance().TestMemoryPage(free_page,used_page,unmem_page);
                }
                for (int iy = 0; iy < now_page_num; iy++)
                {
                        CKernelMallocator::GetInstance().PutMemory(page[iy]);
                        CKernelMallocator::GetInstance().TestMemoryPage(free_page,used_page,unmem_page);
                }
        }

        for (int ix = 1; ix <= max_page_size; ix++)
        {
                int now_page_num = max_page_size/ix+1;
                for (int iy = 0; iy < now_page_num; iy++)
                {
                        page[iy] = CKernelMallocator::GetInstance().GetMemory(ix);
                        if (page[iy])
                        {
                                char* p = (char*)CKernelMallocator::GetInstance().PageToPoint(page[iy]);
                                page_t* ptemp_page = CKernelMallocator::GetInstance().PointToPage(p);
                                if (ptemp_page != page[iy])
                                {
                                        assert(0);
                                }
                                //memset(p,0xff,CKernelMallocator::GetInstance().GetPageSize(page[iy]));

                        }
                        CKernelMallocator::GetInstance().TestMemoryPage(free_page,used_page,unmem_page);
                }
                for (int iy = now_page_num - 1; iy >= 0; iy--)
                {
                        CKernelMallocator::GetInstance().PutMemory(page[iy]);
                        CKernelMallocator::GetInstance().TestMemoryPage(free_page,used_page,unmem_page);
                }
        }

        for (int ix = max_page_size; ix >=1; ix--)
        {
                int now_page_num = max_page_size/ix+1;
                for (int iy = 0; iy < now_page_num; iy++)
                {
                        page[iy] = CKernelMallocator::GetInstance().GetMemory(ix);
                        if (page[iy])
                        {
                                char* p = (char*)CKernelMallocator::GetInstance().PageToPoint(page[iy]);
                                page_t* ptemp_page = CKernelMallocator::GetInstance().PointToPage(p);
                                if (ptemp_page != page[iy])
                                {
                                        assert(0);
                                }
                                //memset(p,0xff, CKernelMallocator::GetInstance().GetPageSize(page[iy]));

                        }
                        CKernelMallocator::GetInstance().TestMemoryPage(free_page,used_page,unmem_page);
                }
                for (int iy = 0; iy < now_page_num; iy++)
                {
                        CKernelMallocator::GetInstance().PutMemory(page[iy]);
                        CKernelMallocator::GetInstance().TestMemoryPage(free_page,used_page,unmem_page);
                }
        }

        for (int ix = max_page_size; ix >= 1; ix--)
        {
                int now_page_num = max_page_size/ix+1;
                for (int iy = 0; iy < now_page_num; iy++)
                {
                        page[iy] = CKernelMallocator::GetInstance().GetMemory(ix);
                        if (page[iy])
                        {
                                char* p = (char*)CKernelMallocator::GetInstance().PageToPoint(page[iy]);
                                page_t* ptemp_page = CKernelMallocator::GetInstance().PointToPage(p);
                                if (ptemp_page != page[iy])
                                {
                                        assert(0);
                                }
                                //memset(p,0xff, CKernelMallocator::GetInstance().GetPageSize(page[iy]));

                        }
                        CKernelMallocator::GetInstance().TestMemoryPage(free_page,used_page,unmem_page);
                }
                for (int iy = now_page_num - 1; iy >= 0; iy--)
                {
                        CKernelMallocator::GetInstance().PutMemory(page[iy]);
                        CKernelMallocator::GetInstance().TestMemoryPage(free_page,used_page,unmem_page);
                }
        }



        if (!CKernelMallocator::GetInstance().TestMemoryPage(free_page,used_page,unmem_page))
	{
                assert(0);
		printf("errrrrrr\n");
	}else
	{
		printf("free = %d used = %d\n",free_page, used_page);
	}
        CKernelMallocator::GetInstance().PrintZoneInfo();
        delete [] page ;
        printf("BaseTest ok \n");
        return 0;
}

int RandomTest()
{
        //random test
        int t = 10000000;
        int test_memory = 0;
        int malloc_count = 0;
        int free_count = 0;
        int out_mem_count = 0;

        int total_malloc_mem = 0;

        unsigned short used_page,free_page,unmem_page;
        srand(time(NULL));
        int max_page_size = CKernelMallocator::GetInstance().GetMaxPageSize();
        int page_num = CKernelMallocator::GetInstance().GetPageNum();

        page_t** page = new page_t* [page_num];
        int now_page = 0;

        for (int ix = 0 ; ix < page_num; ix++)
        {
                page[ix] = NULL;
        }
        while(t--)
        {

                bool malloc_mem = rand()%2;
                if (now_page == page_num-1 || total_malloc_mem >= (page_num-page_num/5))
                {
                        malloc_mem = false;

                }
                else if (now_page == 0 || total_malloc_mem <= page_num/4)
                {
                        malloc_mem = true;
                }

                if (malloc_mem)
                {

                        int malloc_page_size = (rand()%(max_page_size)) +1 ;

                        page[now_page] = CKernelMallocator::GetInstance().GetMemory(malloc_page_size);
                        if (page[now_page])
                        {
                                total_malloc_mem += page[now_page]->_page_num;
                                malloc_count++;
                                //printf("malloc pos %d page id %d page_size %d\n",now_page, page[now_page]->_id, page[now_page]->_page_num);


                                if (page[now_page])
                                {
                                        char* p = (char*)CKernelMallocator::GetInstance().PageToPoint(page[now_page]);
                                        page_t* ptemp_page = CKernelMallocator::GetInstance().PointToPage(p);
                                        if (ptemp_page != page[now_page])
                                        {
                                                assert(0);
                                        }
                                        //memset(p,0xff, CKernelMallocator::GetInstance().GetPageSize(page[now_page]));
                                        //memset(p,0,page[now_page]->_page_num*page_size);
                                        //unsigned char xx = rand()%256;
                                        //memset(p,xx,page[now_page]->_page_num*page_size);
                                }
                                now_page++;
                        }
                        else
                        {
                                out_mem_count++;
                                //printf("no mem malloc %d used %d free %d\n",malloc_page_size,total_malloc_mem,page_num-total_malloc_mem);
                                //CKernelMallocator::GetInstance().PrintZoneInfo();
                        }

                }
                else
                {
                        free_count++;
                        int free_pos = rand()%now_page;
                        total_malloc_mem -= page[free_pos]->_page_num;
                        //printf("free pos %d page id %d page_size %d\n",free_pos, page[free_pos]->_id, page[free_pos]->_page_num);
                        CKernelMallocator::GetInstance().PutMemory(page[free_pos]);
                        for (int ix = free_pos; ix < now_page; ix++)
                        {
                                page[ix] = page[ix+1];
                        }
                        now_page--;
                }

                if (test_memory++ == 1000)
                {
                        test_memory = 0;
                        if (!CKernelMallocator::GetInstance().TestMemoryPage(free_page,used_page,unmem_page))
	                {
                                assert(0);
		                printf("errrrrrr\n");
	                }else
	                {
		                //printf("free = %d used = %d\n",free_page, used_page);
	                }
                }
        }

        for ( int ix = 0; ix < now_page; ix++)
        {
               CKernelMallocator::GetInstance().PutMemory(page[ix]);
        }

        if (!CKernelMallocator::GetInstance().TestMemoryPage(free_page,used_page,unmem_page))
	{
                assert(0);
		printf("errrrrrr\n");
	}else
	{
		printf("free = %d used = %d\n",free_page, used_page);
	}
        delete [] page ;
        printf("malloc mem %d malloc %d free %d out_mem %d\n",
                total_malloc_mem, malloc_count, free_count, out_mem_count);
        printf("Random ok \n");
        return 0;
}

int CoverTest()
{
        //cover test
        int page_num = CKernelMallocator::GetInstance().GetPageNum();
        int max_page_size = CKernelMallocator::GetInstance().GetMaxPageSize();

        unsigned short free_page,used_page,unmem_page;

        page_t** page = new page_t* [page_num];

        //normall
        page[0] = CKernelMallocator::GetInstance().GetMemory(1);
        page[1] = CKernelMallocator::GetInstance().GetMemory(max_page_size/2+1);
        page[2] = CKernelMallocator::GetInstance().GetMemory(max_page_size+1);
        CKernelMallocator::GetInstance().PutMemory(page[1]);
        CKernelMallocator::GetInstance().PutMemory(page[0]);
        CKernelMallocator::GetInstance().PutMemory(page[2]);
        if (!CKernelMallocator::GetInstance().TestMemoryPage(free_page,used_page,unmem_page))
	{
                assert(0);
		printf("errrrrrr\n");
	}

        //CKernelMallocator::CollectMemory
        for (int ix = 0; ix < page_num; ix++)
        {
                page[ix] = CKernelMallocator::GetInstance().GetMemory(1);
        }
        for (int ix = 0; ix < page_num; ix++)
        {
               CKernelMallocator::GetInstance().PutMemory(page[ix]);
        }
        page[0]= CKernelMallocator::GetInstance().GetMemory(max_page_size);
        page[1]= CKernelMallocator::GetInstance().GetMemory(max_page_size);
        page[2]= CKernelMallocator::GetInstance().GetMemory(max_page_size);;
        page[3]= CKernelMallocator::GetInstance().GetMemory(max_page_size);
        CKernelMallocator::GetInstance().PrintZoneInfo();
        CKernelMallocator::GetInstance().PutMemory(page[0]);
        CKernelMallocator::GetInstance().PutMemory(page[1]);
        CKernelMallocator::GetInstance().PutMemory(page[2]);
        CKernelMallocator::GetInstance().PutMemory(page[3]);

        //dynimac get page put page
        for (int ix = 0; ix < page_num; ix++)
        {
               page[ix] = CKernelMallocator::GetInstance().GetMemory(max_page_size);
        }

        for (int ix = 0; ix < page_num; ix++)
        {
               CKernelMallocator::GetInstance().PutMemory(page[ix]);
        }

        if (!CKernelMallocator::GetInstance().TestMemoryPage(free_page,used_page,unmem_page))
        {
                       assert(0);
                       printf("errrrrrr\n");
        }

        //no memory
        for (int ix = 0; ix < page_num+1; ix++)
        {
                page[ix] = CKernelMallocator::GetInstance().GetMemory(1);
        }
        for (int ix = 0; ix < page_num+1; ix++)
        {
                CKernelMallocator::GetInstance().PutMemory(page[ix]);
        }

        if (!CKernelMallocator::GetInstance().TestMemoryPage(free_page,used_page,unmem_page))
        {
                assert(0);
                printf("errrrrrr\n");
        }

        delete [] page;
        printf("CoverTest ok\n");
        return 0;
}

int DynamicTest()
{
        //dynamic mm
        int page_num = CKernelMallocator::GetInstance().GetPageNum();
        int max_page_size = CKernelMallocator::GetInstance().GetMaxPageSize();
  //      unsigned short free_page,used_page;
        page_t** page = new page_t* [page_num];

        page[0]= CKernelMallocator::GetInstance().GetMemory(max_page_size);
        page[1]= CKernelMallocator::GetInstance().GetMemory(max_page_size);
        page[2]= CKernelMallocator::GetInstance().GetMemory(max_page_size);
        page[3]= CKernelMallocator::GetInstance().GetMemory(1);
        CKernelMallocator::GetInstance().PutMemory(page[3]);
        page[3]= CKernelMallocator::GetInstance().GetMemory(max_page_size-1);
        CKernelMallocator::GetInstance().PutMemory(page[3]);

        page[3]= CKernelMallocator::GetInstance().GetMemory(max_page_size-1);
        page[4]= CKernelMallocator::GetInstance().GetMemory(1);

        CKernelMallocator::GetInstance().PutMemory(page[3]);
        CKernelMallocator::GetInstance().PutMemory(page[4]);
        CKernelMallocator::GetInstance().PutMemory(page[2]);
        CKernelMallocator::GetInstance().PutMemory(page[1]);
        CKernelMallocator::GetInstance().PutMemory(page[0]);
        delete [] page;
        printf("DynamicTest ok\n");
        return 0;
}

/*pt test*/

int KMallocTest()
{
        BaseTest();
        RandomTest();
        CoverTest();
        DynamicTest();
        printf("kmalloc test ok\n");
        return 0;
}


inline unsigned long long  GetBinsSize(unsigned int index)
{
        if (index < 64)
        {
                return index<<4;
        }
        else if (index < 96)
        {
                return 1024 + ((index-64)<<6);
        }
        else if (index < 112)
        {
                return 3072 + ((index-96)<<9);
        }
        else if (index < 120)
        {
                if (index == 112) return 11264;
                return (index-110)<<12;
        }
        else if (index < 124)
        {
                if (index == 120) return 40960;
                return (index-119)<<15;
        }
        else if (index < 126)
        {
                if (index == 124) return 163840;
                return (index-124)<<18;
        }
        else
        {
                return 524288;
        }
}


int PTCoverTest()
{
        CPTMallocator* ppppp = CPTMallocator::GetInstance();
        CPTMallocator& aa = *ppppp;
        chunk_t** chunk= new chunk_t* [1000000];

        chunk[0] = aa.GetChunk(32);
        chunk[1] = aa.GetChunk(1008);
        chunk[2] = aa.GetChunk(1031000);
        aa.PutChunk(chunk[0]);
        aa.PutChunk(chunk[1]);
        chunk[0] = aa.GetChunk(1032);
        aa.PutChunk(chunk[0]);
        aa.PutChunk(chunk[2]);

        chunk[0] = aa.GetChunk(24);
        chunk[1] = aa.GetChunk(24);
        chunk[2] = aa.GetChunk(24);

        aa.PutChunk(chunk[0]);
        aa.PutChunk(chunk[2]);
        aa.PutChunk(chunk[1]);

        //PutChunkToSortBins
        chunk[0] = aa.GetChunk(1024);
        chunk[1] = aa.GetChunk(1024);
        chunk[2] = aa.GetChunk(1024);

        chunk[3] = aa.GetChunk(1008);
        chunk[4] = aa.GetChunk(1008);
        chunk[5] = aa.GetChunk(1008);

        chunk[6] = aa.GetChunk(1040);
        chunk[7] = aa.GetChunk(1040);
        chunk[8] = aa.GetChunk(1040);
        chunk[9] = aa.GetChunk(1040);

        chunk[10] = aa.GetChunk(1056);
        chunk[11] = aa.GetChunk(1056);
        chunk[12] = aa.GetChunk(1056);


        aa.PutChunkToSortBins(chunk[6]);
        aa.PutChunkToSortBins(chunk[7]);
        aa.PutChunkToSortBins(chunk[8]);
        aa.PutChunkToSortBins(chunk[0]);
        aa.PutChunkToSortBins(chunk[1]);
        aa.PutChunkToSortBins(chunk[10]);
        aa.PutChunkToSortBins(chunk[11]);
        aa.PutChunkToSortBins(chunk[3]);
        aa.PutChunkToSortBins(chunk[2]);
        aa.PutChunkToSortBins(chunk[4]);
        aa.PutChunkToSortBins(chunk[5]);
        aa.PutChunkToSortBins(chunk[9]);
        aa.PutChunkToSortBins(chunk[12]);


        //GetFromBins
        chunk[3] = aa.GetFromBins(chunk[3]);
        chunk[0] = aa.GetFromBins(chunk[0]);
        chunk[6] = aa.GetFromBins(chunk[6]);
        chunk[10] = aa.GetFromBins(chunk[10]);

        chunk[9] = aa.GetFromBins(chunk[9]);
        chunk[8] = aa.GetFromBins(chunk[8]);
        chunk[7] = aa.GetFromBins(chunk[7]);

        chunk[5] = aa.GetFromBins(chunk[5]);
        chunk[4] = aa.GetFromBins(chunk[4]);

        chunk[12] = aa.GetFromBins(chunk[12]);
        chunk[11] = aa.GetFromBins(chunk[11]);

        chunk[1] = aa.GetFromBins(chunk[1]);
        chunk[2] = aa.GetFromBins(chunk[2]);

        for (int ix = 0; ix < 13; ix++)
        {
                aa.PutChunkToSortBins(chunk[ix]);
        }

        //new chunk
        chunk[0] = aa.GetChunk(1024*1000);
        aa.PutChunk(chunk[0]);
        chunk[0] = aa.GetChunk(1024*1024);
        aa.PutChunk(chunk[0]);

        //GetBinsIndex
        for (unsigned long long ix = 32; ix < 524288; ix++)
        {
                unsigned int index = aa.GetBinsIndex(ix);
                unsigned long long min = GetBinsSize(index);
                unsigned long long max = GetBinsSize(index+1);
                SHM_ASSERT(ix>=min && ix < max);
        }

        //GetChunkFromSortBins
        chunk[0] = aa.GetChunkFromSortBins(1024);
        chunk[1] = aa.GetChunkFromSortBins(32);
        chunk[2] = aa.GetChunkFromSortBins(1040);
        chunk[3] = aa.GetChunkFromSortBins(32);
        chunk[4] = aa.GetChunkFromSortBins(1072);
        chunk[5] = aa.GetChunkFromSortBins(32);
        chunk[6] = aa.GetChunkFromSortBins(1072);
        chunk[7] = aa.GetChunkFromSortBins(32);
        chunk[8] = aa.GetChunkFromSortBins(1088);
        aa.PutChunkToSortBins(chunk[0]);
        aa.PutChunkToSortBins(chunk[2]);
        aa.PutChunkToSortBins(chunk[4]);
        aa.PutChunkToSortBins(chunk[6]);
        aa.PutChunkToSortBins(chunk[8]);


        chunk[0] = aa.GetChunkFromSortBins(1024);
        chunk[2] = aa.GetChunkFromSortBins(1072);
        chunk[4] = aa.GetChunkFromSortBins(1088);
        chunk[6] = aa.GetChunkFromSortBins(1056);
        chunk[8] = aa.GetChunkFromSortBins(1072);

        chunk[9] = aa.GetChunkFromSortBins(1056);
        chunk[10] = aa.GetChunkFromSortBins(1056);
        chunk[11] = aa.GetChunkFromSortBins(1056);
        chunk[12] = aa.GetChunkFromSortBins(1040);
        chunk[13] = aa.GetChunkFromSortBins(1040);
        chunk[14] = aa.GetChunkFromSortBins(1072);
        aa.PutChunkToSortBins(chunk[14]);
        chunk[14] = aa.GetChunkFromSortBins(1040);

        for (int ix = 0; ix < 15; ix++)
        {
                aa.PutChunkToSortBins(chunk[ix]);
        }


        for (unsigned int index = 2; index < 126; index++)
        {
                unsigned long long sz = GetBinsSize(index);
                chunk[index*2] = aa.GetChunkFromSortBins(sz);
                chunk[index*2+1] = aa.GetChunkFromSortBins(32);
        }

        for (unsigned int index = 2; index < 126; index++)
        {
                aa.PutChunkToSortBins(chunk[index*2]);//memlast
                //aa.PutChunk(chunk[index*2]);
        }

        for (unsigned int ix = 0; ix < 64; ix++)
        {
                chunk[300+ix] = aa.GetChunkFromSortBins(1024);
        }

        for (unsigned int ix = 0; ix < 64; ix++)
        {
                chunk[364+ix] = aa.GetChunkFromSortBins(3840);
        }

        for (unsigned int index = 2; index < 126; index++)
        {
                aa.PutChunk(chunk[index*2+1]);
        }

        for (unsigned int ix = 0; ix < 64; ix++)
        {
                aa.PutChunk(chunk[300+ix]);
        }

        for (unsigned int ix = 0; ix < 64; ix++)
        {
                aa.PutChunk(chunk[364+ix]);
        }



        //GetChunk
        for (unsigned int ix = 0; ix < 10; ix++)
        {
                chunk[1000+ix] = aa.GetChunk(24);
        }

        for (unsigned int ix = 0; ix < 10; ix++)
        {
                 aa.PutChunk(chunk[1000+ix]);
        }


        //collect memmory
        for (int ix = 0; ix < 1000000; ix++)
        {
                chunk[ix] = aa.GetChunk(24);
        }


        for (int ix = 0; ix < 1000000; ix++)
        {
                aa.PutChunk(chunk[ix]);
        }


        if (aa._ptmalloc->_has_fast_bins)
                aa.CleanFastBins();

        delete []chunk;
        printf("PTCoverTest ok\n");
        return 0;
}

int PTBaseTest()
{
        void ** point = new void* [10000];

        for (int ix = 0; ix < 10000; ix++)
        {
                point[ix] = CPTMallocator::Malloc(ix);
        }

        for (int ix = 0; ix < 10000; ix++)
        {
                CPTMallocator::Free(point[ix]);
        }

        for (int ix = 0; ix < 10000; ix++)
        {
                point[ix] = CPTMallocator::Malloc(10000-ix);
        }

        for (int ix = 0; ix < 10000; ix++)
        {
                CPTMallocator::Free(point[ix]);
        }

        for (int ix = 0; ix < 100; ix++)
        {
                for (int iy = 0; iy < 100; iy++)
                {
                        point[ix*100+iy] = CPTMallocator::Malloc(ix*ix+1024+iy);
                }
        }
        for (int ix = 0; ix < 10000; ix++)
        {
                CPTMallocator::Free(point[ix]);
        }

        for (int ix = 0; ix < 100; ix++)
        {
                for (int iy = 0; iy < 100; iy++)
                {
                        point[ix*100+iy] = CPTMallocator::Malloc(iy);
                }
        }
        for (int ix = 0; ix < 10000; ix++)
        {
                CPTMallocator::Free(point[ix]);
        }


        CPTMallocator* ptmalloc = CPTMallocator::GetInstance();

        if (ptmalloc->_ptmalloc->_has_fast_bins)
                ptmalloc->CleanFastBins();


        delete []point;
        printf("PTBaseTest ok\n");
        return 0;
}

int PTRandomOneThrTest()
{
        int t = 1000000;
        int max_point_num = 100000;
        void ** point = new void* [max_point_num];
        int now_point_num = 0;
        int test_memory = 0;
        unsigned long long tatol_malloc_size = 0;
        unsigned long long max_tatol_malloc_size = 0;
        unsigned long long max_mem_size = 0xa0000000;
        int malloc_count = 0;
        int free_count = 0;
        srand(time(NULL));
        while(t--)
        {
                bool malloc_mem = rand()%2;
                if (now_point_num >= max_point_num/100*99 || tatol_malloc_size >= max_mem_size/100*75)
                {
                        malloc_mem = false;
                }
                else if (tatol_malloc_size <= max_mem_size/100*50)
                {
                        malloc_mem = true;
                }
                if (!now_point_num)
                {
                        malloc_mem = true;
                }
                if (malloc_mem)
                {
                        int malloc_size = rand()%(1024*1024);
                        if (malloc_size == 0) malloc_size++;
                        point[now_point_num] = CPTMallocator::Malloc(malloc_size);
                        if (point[now_point_num])
                        {
                        unsigned long long memsz = CPTMallocator::MallocSize(point[now_point_num]);
                        memset(point[now_point_num],0xee,memsz);
                        tatol_malloc_size += memsz;
                        if (tatol_malloc_size > max_tatol_malloc_size)
                        {
                                max_tatol_malloc_size = tatol_malloc_size;
                        }
                        now_point_num++;
                        malloc_count++;
                        }
                }
                else
                {
                        int free = rand()%now_point_num;
                        tatol_malloc_size -= CPTMallocator::MallocSize(point[free]);
                        CPTMallocator::Free(point[free]);
                        for (int ix = free; ix < now_point_num; ix++)
                        {
                                point[ix] = point[ix+1];
                        }
                        now_point_num--;
                        free_count++;
                }
                if (test_memory++ == 1000)
                {
                        unsigned short free_page,used_page,unmem_page;
                        test_memory = 0;
                        if (!CKernelMallocator::GetInstance().TestAllMemoryPage(free_page,used_page,unmem_page))
	                {
                                assert(0);
		                printf("errrrrrr\n");
	                }else
	                {
	                }
                        if (!CPTMallocator::TestChunk())
	                {
                                assert(0);
		                printf("errrrrrr\n");
	                }
                }
        }
        unsigned short free_page,used_page,unmem_page;
        if (!CKernelMallocator::GetInstance().TestMemoryPage(free_page,used_page,unmem_page))
	{
                assert(0);
                printf("errrrrrr\n");
	}else
        {
	}
        printf("free = %d used = %d unmem = %d\n",free_page, used_page, unmem_page);
        printf("malloc_mm = %lld max_tatol_malloc_size = %lld\n",tatol_malloc_size,max_tatol_malloc_size);
        for (int ix = 0; ix < now_point_num; ix++)
        {
                CPTMallocator::Free(point[ix]);
        }
        CPTMallocator* ptmalloc = CPTMallocator::GetInstance();
        if (ptmalloc->_ptmalloc->_has_fast_bins)
                ptmalloc->CleanFastBins();
        if (!CKernelMallocator::GetInstance().TestMemoryPage(free_page,used_page,unmem_page))
	{
                assert(0);
                printf("errrrrrr\n");
	}else
        {
	}
        if (!ptmalloc->TestChunk())
        {
                assert(0);
        }
        printf("after free\nfree = %d used = %d unmem = %d malloc_count = %d free_count = %d\n",
                free_page, used_page, unmem_page, malloc_count, free_count);
        delete []point;
        printf("PTRandomOneThrTest ok\n");
        return 0;
}

int PTRandomTest()
{
        int t = 100000;
        int max_point_num = 100000;
        void ** point = new void* [max_point_num];
        int now_point_num = 0;
        int test_memory = 0;
        unsigned long long tatol_malloc_size = 0;
        unsigned long long max_tatol_malloc_size = 0;
        unsigned long long max_mem_size = 0xa0000000;
        int malloc_count = 0;
        int free_count = 0;
        srand(time(NULL));

        while(t--)
        {
                bool malloc_mem = rand()%2;
                if (now_point_num >= max_point_num/100*99 || tatol_malloc_size >= max_mem_size/100*75)
                {
                        malloc_mem = false;
                }
                else if (tatol_malloc_size <= max_mem_size/100*50)
                {
                        malloc_mem = true;
                }

                if (!now_point_num)
                {
                        malloc_mem = true;
                }

                if (malloc_mem)
                {
                        //int malloc_size = rand()%(1024*1024*2);
                        int malloc_size = rand()%(1024*1024);
                        if (malloc_size == 0) malloc_size++;
                        point[now_point_num] = CPTMallocator::Malloc(malloc_size);

                        //assert(point[now_point_num]);
                        if (point[now_point_num])
                        {
                        unsigned long long memsz = CPTMallocator::MallocSize(point[now_point_num]);
                        memset(point[now_point_num],0xee,memsz);
                        tatol_malloc_size += memsz;
                        if (tatol_malloc_size > max_tatol_malloc_size)
                        {
                                max_tatol_malloc_size = tatol_malloc_size;
                        }
                        now_point_num++;
                        malloc_count++;
                        }
                }
                else
                {
                        int free = rand()%now_point_num;
                        tatol_malloc_size -= CPTMallocator::MallocSize(point[free]);
                        CPTMallocator::Free(point[free]);
                        for (int ix = free; ix < now_point_num; ix++)
                        {
                                point[ix] = point[ix+1];
                        }
                        now_point_num--;
                        free_count++;
                }


                if (test_memory++ == 1000)
                {
                        unsigned short free_page,used_page,unmem_page;
                        test_memory = 0;
                        if (!CKernelMallocator::GetInstance().TestMemoryPage(free_page,used_page,unmem_page))
	                {
                                assert(0);
		                printf("errrrrrr\n");
	                }else
	                {
		                //printf("free = %d used = %d\n",free_page, used_page);
	                }
                        if (!CPTMallocator::TestChunk())
	                {
                                assert(0);
		                printf("errrrrrr\n");
	                }
                }
        }

        unsigned short free_page,used_page,unmem_page;

        if (!CKernelMallocator::GetInstance().TestMemoryPage(free_page,used_page,unmem_page))
	{
                assert(0);
                printf("errrrrrr\n");
	}else
        {
                //printf("free = %d used = %d\n",free_page, used_page);
	}

        printf("free = %d used = %d unmem = %d\n",free_page, used_page, unmem_page);
        printf("malloc_mm = %lld max_tatol_malloc_size = %lld\n",tatol_malloc_size,max_tatol_malloc_size);

        for (int ix = 0; ix < now_point_num; ix++)
        {
                CPTMallocator::Free(point[ix]);
        }

        CPTMallocator* ptmalloc = CPTMallocator::GetInstance();
        if (ptmalloc->_ptmalloc->_has_fast_bins)
                ptmalloc->CleanFastBins();

        if (!CKernelMallocator::GetInstance().TestMemoryPage(free_page,used_page,unmem_page))
	{
                assert(0);
                printf("errrrrrr\n");
	}else
        {
                //printf("free = %d used = %d\n",free_page, used_page);
	}
        if (!ptmalloc->TestChunk())
        {
                assert(0);
	}

        printf("after free\nfree = %d used = %d unmem = %d malloc_count = %d free_count = %d\n",
                free_page, used_page, unmem_page, malloc_count, free_count);

        delete []point;
        printf("PTRandomTest ok\n");
        return 0;
}

static void *g_p[100000];
static CPthreadMutex g_lock[100000];

void *thr_fn1(void*)
{
        srand(time(NULL));
        int t = 100000;
        while(t--)
        {
                int pos = rand()%100000;
                int size = rand()%(1024)+1;
                SHM_GUARD(&g_lock[pos], OBJ, SHM_ASSERT(0));
                if (g_p[pos])
                {
                        CPTMallocator::Free(g_p[pos]);
                }
                g_p[pos] = CPTMallocator::Malloc(size);
                if (g_p[pos])
                {
                        memset(g_p[pos],0xff,CPTMallocator::MallocSize(g_p[pos]));
                }
        }

        printf("thr_fn1 %lu ok\n", pthread_self());
        return (void*)0;
}

void *thr_fn2(void*)
{
        srand(time(NULL));
        int t = 1000000;
        while(t--)
        {
                int pos = rand()%100000;
                SHM_GUARD(&g_lock[pos], OBJ, SHM_ASSERT(0));
                if (g_p[pos])
                {
                        memset(g_p[pos],0xff,CPTMallocator::MallocSize(g_p[pos]));
                        CPTMallocator::Free(g_p[pos]);
                        g_p[pos] = 0;
                }
        }

        printf("thr_fn2 %lu ok\n", pthread_self());
        return (void*)0;
}



void ThreadTest()
{
        for( int ix = 0; ix <100000; ix++)
        {
                g_p[ix] = 0;
                SHM_ASSERT(g_lock[ix].Init());
        }
        pthread_t ntid[20];
        for (int ix = 0; ix < 8; ix++)
        {
                pthread_create(&ntid[ix],NULL,thr_fn1,NULL);
        }
        for (int ix = 0; ix < 8; ix++)
        {
                pthread_create(&ntid[ix+8],NULL,thr_fn2,NULL);
        }

        for (int ix = 0; ix < 16; ix++)
        {
                pthread_join(ntid[ix],NULL);
        }
        printf("ThreadTest ok \n");
}


void SpeedTest()
{
        void*p[1000000];
        struct timeval tv1,tv2;
        gettimeofday(&tv1,NULL);
        int size = 32;
        int t= 100;
        //int alter = 100000;
        int alter = 1000;
        while(t--)
        {
                for (int ix = 0; ix < alter; ix++)
                {
                        p[ix] = CPTMallocator::Malloc(size);
                        //assert(p[ix]);
                        if (p[ix])
                                memset(p[ix],0xff,size);
                }

                for (int ix = 0; ix < alter; ix++)
                {
                        if (p[ix])
                                CPTMallocator::Free(p[ix]);

                }

                for (int ix = 0; ix < alter; ix++)
                {
                        p[ix] = CPTMallocator::Malloc(size);
                        assert(p[ix]);
                        memset(p[ix],0xff,size);
                        CPTMallocator::Free(p[ix]);
                }

        }
        gettimeofday(&tv2,NULL);
        //printf("tv1 %d %d\n",(unsigned int)tv1.tv_sec, (unsigned int)tv1.tv_usec);
        //printf("tv2 %d %d\n",(unsigned int)tv2.tv_sec, (unsigned int)tv2.tv_usec);
        unsigned long long diff = (tv2.tv_sec-tv1.tv_sec)*1000000+tv2.tv_usec-tv1.tv_usec;
        //printf("tv %lld\n",diff);
        gettimeofday(&tv1,NULL);
        t = 100;
        while(t--)
        {
                for (int ix = 0; ix < alter; ix++)
                {
                        p[ix] = malloc(size);
                        memset(p[ix],0xff,size);
                }

                for (int ix = 0; ix < alter; ix++)
                {
                        free(p[ix]);
                }
                for (int ix = 0; ix < alter; ix++)
                {
                        p[ix] = malloc(size);
                        memset(p[ix],0xff,size);
                        free(p[ix]);
                }
        }
        gettimeofday(&tv2,NULL);
        //printf("tv1 %d %d\n",(unsigned int)tv1.tv_sec, (unsigned int)tv1.tv_usec);
        //printf("tv2 %d %d\n",(unsigned int)tv2.tv_sec, (unsigned int)tv2.tv_usec);
        unsigned long long diff2 = (tv2.tv_sec-tv1.tv_sec)*1000000+tv2.tv_usec-tv1.tv_usec;
       // printf("tv %lld\n",diff2);

        printf("%lld %lld %lf\n",diff,diff2,(double)diff/diff2);
}

};
using namespace pwrdshmm;

void *thr_fn(void*)
{

        CPTMallocator*aa = CPTMallocator::GetInstance();
        printf("%lu start %p\n", pthread_self(), aa);

        PTCoverTest();
        PTBaseTest();
        PTRandomTest();
        SpeedTest();
        return ((void*)0);
}

void EachSpeedTest(int size,int test_cast, int alter)
{
        void*p[alter];
        struct timeval tv1,tv2;
        gettimeofday(&tv1,NULL);
        int t = test_cast;
        while(t--)
        {
                for (int ix = 0; ix < alter; ix++)
                {
                        p[ix] = CPTMalloc::Malloc(size);
                        assert(p[ix]);
                        memset(p[ix],0xff,size);
                }

                for (int ix = 0; ix < alter; ix++)
                {
                        CPTMalloc::Free(p[ix]);
                }

                for (int ix = 0; ix < alter; ix++)
                {
                        p[ix] = CPTMalloc::Malloc(size);
                        assert(p[ix]);
                        memset(p[ix],0xff,size);
                        CPTMalloc::Free(p[ix]);
                }

        }
        gettimeofday(&tv2,NULL);
        unsigned long long diff = (tv2.tv_sec-tv1.tv_sec)*1000000+tv2.tv_usec-tv1.tv_usec;

        gettimeofday(&tv1,NULL);
        t = test_cast;
        while(t--)
        {
                for (int ix = 0; ix < alter; ix++)
                {
                        p[ix] = malloc(size);
                        memset(p[ix],0xff,size);
                }

                for (int ix = 0; ix < alter; ix++)
                {
                        free(p[ix]);
                }
                for (int ix = 0; ix < alter; ix++)
                {
                        p[ix] = malloc(size);
                        memset(p[ix],0xff,size);
                        free(p[ix]);
                }
        }
        gettimeofday(&tv2,NULL);
        unsigned long long diff2 = (tv2.tv_sec-tv1.tv_sec)*1000000+tv2.tv_usec-tv1.tv_usec;
        printf("size = %d %lld %lld %lf\n", size,diff,diff2, (double)diff/diff2);
}



void *SpeedTest2(void*)
{
        EachSpeedTest(16,10,100000);
        EachSpeedTest(32,10,100000);
        EachSpeedTest(100,10,100000);
	/*
        EachSpeedTest(200,1,100000);
        EachSpeedTest(500,1,100000);
        EachSpeedTest(1000,1,10000);
        EachSpeedTest(2000,1,10000);
        EachSpeedTest(8000,1,10000);
        EachSpeedTest(1000*1000,10,100);
        EachSpeedTest(2000*1000,10,100);
	*/
        return ((void*)0);
}

int g_long_size = 100000;
static void* g_long_p[100000];
static CPthreadMutex g_long_lock[100000];
static bool stop = false;

static CPthreadMutex g_stat_lock;
static  long long g_stat_max_mem_size = 0;
static  long long g_stat_mem_size = 0;
static  long long g_stat_malloc_count = 0;
static  long long g_stat_free_count = 0;
static  long long g_stat_out_mem_count = 0;
//static  long long g_total_size = 0x200000000ull;




void *long_thr_fn1(void*arg)
{
        unsigned long long max_malloc_size = (unsigned long long)arg;
        srand(time(NULL));
         long long max_mem_size = 0;
         long long total_mem_size = 0;
         long long total_malloc_count = 0;
         long long total_free_count = 0;
         long long total_out_mem_count = 0;
         long long mem_size = 0;
         long long malloc_count = 0;
         long long free_count = 0;
         long long out_mem_count = 0;

        int fail = 0;
        int t = 0;
        while(!stop)
        {
                int pos = rand()%g_long_size;
                int size = rand()%(max_malloc_size)+1;
                {
                        SHM_GUARD(&g_long_lock[pos], OBJ, SHM_ASSERT(0));
                        if (g_long_p[pos])
                        {
                                mem_size -= CPTMalloc::MallocSize(g_long_p[pos]);
                                total_mem_size -= CPTMalloc::MallocSize(g_long_p[pos]);
                                CPTMalloc::Free(g_long_p[pos]);
                                g_long_p[pos] = 0;
                                free_count++;
                                total_free_count++;
                        }
                        g_long_p[pos] = CPTMalloc::Malloc(size);
                        malloc_count++;
                        total_malloc_count++;
                        if (g_long_p[pos])
                        {
                                memset(g_long_p[pos],0xff,CPTMalloc::MallocSize(g_long_p[pos]));
                                mem_size += CPTMalloc::MallocSize(g_long_p[pos]);
                                total_mem_size += CPTMalloc::MallocSize(g_long_p[pos]);
                        }
                        else
                        {
                                fail++;
                                out_mem_count++;
                                total_out_mem_count++;
                        }
                }
                if (fail >= 1)
                {
                        fail = 0;
                        sleep(10);
                }

                if (total_mem_size > max_mem_size)
                {
                        max_mem_size = total_mem_size;
                }

                if (t++ >= 100)
                {
                        t = 0;
                        {
                                SHM_GUARD(&g_stat_lock, OBJ, SHM_ASSERT(0));
                                g_stat_mem_size += mem_size;
                                if (g_stat_mem_size > g_stat_max_mem_size)
                                {
                                        g_stat_max_mem_size = g_stat_mem_size;
                                }
                                g_stat_malloc_count += malloc_count;

                                g_stat_free_count += free_count;

                                g_stat_out_mem_count += out_mem_count;

                        }
                        mem_size = 0;
                        malloc_count = 0;
                        free_count = 0;
                        out_mem_count = 0;
                        if (!CPTMalloc::TestChunk())
                        {
                                SHM_ASSERT(0);
                        }
                }
        }

        {
                SHM_GUARD(&g_stat_lock, OBJ, SHM_ASSERT(0));
                g_stat_mem_size += mem_size;
                if (g_stat_mem_size > g_stat_max_mem_size)
                {
                        g_stat_max_mem_size = g_stat_mem_size;
                }
                g_stat_malloc_count += malloc_count;

                g_stat_free_count += free_count;

                g_stat_out_mem_count += out_mem_count;

        }
        mem_size = 0;
        malloc_count = 0;
        free_count = 0;
        out_mem_count = 0;

        printf("thr_fn1 %lu ok\n", pthread_self());
        return (void*)0;
}


void *long_thr_fn2(void*arg)
{
        unsigned long long start_pos = (unsigned long long)arg;
        srand(time(NULL));
        long long mem_size = 0;
        unsigned long long malloc_count = 0;
        unsigned long long free_count = 0;
        unsigned long long out_mem_count = 0;
        int t = 0;
        while(!stop)
        {
                for (int ix = (int)start_pos ; ix < g_long_size; ix+=2)
                {
                        if (g_long_p[ix])
                        {
                               SHM_GUARD(&g_long_lock[ix], OBJ, SHM_ASSERT(0));
                               if (g_long_p[ix])
                               {
                                        mem_size -= CPTMalloc::MallocSize(g_long_p[ix]);
                                        CPTMalloc::Free(g_long_p[ix]);
                                        g_long_p[ix] = 0;
                                        free_count++;
                               }
                        }
                }

                if (t++ >= 100)
                {
                        t = 0;
                        {
                                SHM_GUARD(&g_stat_lock, OBJ, SHM_ASSERT(0));
                                g_stat_mem_size += mem_size;
                                if (g_stat_mem_size > g_stat_max_mem_size)
                                {
                                        g_stat_max_mem_size = g_stat_mem_size;
                                }
                                g_stat_malloc_count += malloc_count;

                                g_stat_free_count += free_count;

                                g_stat_out_mem_count += out_mem_count;

                        }
                        mem_size = 0;
                        malloc_count = 0;
                        free_count = 0;
                        out_mem_count = 0;
                }
                sleep(1);
        }

        {
                SHM_GUARD(&g_stat_lock, OBJ, SHM_ASSERT(0));
                g_stat_mem_size += mem_size;
                if (g_stat_mem_size > g_stat_max_mem_size)
                {
                        g_stat_max_mem_size = g_stat_mem_size;
                }
                g_stat_malloc_count += malloc_count;

                g_stat_free_count += free_count;

                g_stat_out_mem_count += out_mem_count;

        }


        printf("thr_fn2 %lu ok\n", pthread_self());
        return (void*)0;
}




int LongTimeTest()
{
        SHM_ASSERT(g_stat_lock.Init());
        for( int ix = 0; ix < g_long_size; ix++)
        {
                g_long_p[ix] = 0;
                SHM_ASSERT(g_long_lock[ix].Init());
        }


        pthread_t ntid[18];
        pthread_create(&ntid[0],NULL,long_thr_fn1,(void*)80);
        pthread_create(&ntid[1],NULL,long_thr_fn1,(void*)1024);
        pthread_create(&ntid[2],NULL,long_thr_fn1,(void*)1024);
        pthread_create(&ntid[3],NULL,long_thr_fn1,(void*)1024);
        pthread_create(&ntid[4],NULL,long_thr_fn1,(void*)8000);
        pthread_create(&ntid[5],NULL,long_thr_fn1,(void*)(8*1024));
        pthread_create(&ntid[6],NULL,long_thr_fn1,(void*)1024000);
        pthread_create(&ntid[7],NULL,long_thr_fn1,(void*)1024000);
        pthread_create(&ntid[8],NULL,long_thr_fn1,(void*)(1024*1024));
        pthread_create(&ntid[9],NULL,long_thr_fn1,(void*)(8*1024*1000));
        pthread_create(&ntid[10],NULL,long_thr_fn2,(void*)0);
        pthread_create(&ntid[11],NULL,long_thr_fn2,(void*)1);
        pthread_create(&ntid[12],NULL,long_thr_fn2,(void*)2);

        long long max_mem_size = 0;
        long long mem_size = 0;
        long long malloc_count = 0;
        long long free_count = 0;
        long long out_count = 0;
        long long pre_malloc_count = 0;
        long long pre_free_count = 0;
        long long pre_out_count = 0;

        unsigned short free_page;
        unsigned short used_page;
        unsigned short unmem_page;
        unsigned short min_free_page = 10000;
        unsigned short max_free_page = 0;
        unsigned short min_used_page = 10000;
        unsigned short max_uses_page = 0;
        unsigned short min_unmem_page = 10000;
        unsigned short max_unmem_page = 0;

        int t = 0;
        int abc = 0;
        int display = 1;
        while(!stop)
        {
                {
                        SHM_GUARD(&g_stat_lock, OBJ, SHM_ASSERT(0));
                        max_mem_size = g_stat_max_mem_size;
                        mem_size = g_stat_mem_size;
                        malloc_count = g_stat_malloc_count;
                        free_count = g_stat_free_count;
                        out_count = g_stat_out_mem_count;
                }
                printf("%lu: mem = %lld  max_mem = 0x%llx  mem_count = 0x%llx  free_count = 0x%llx  out_count = 0x%llx %lld %lld %lld\n",
                        pthread_self(),mem_size,max_mem_size,malloc_count,free_count,out_count,malloc_count-pre_malloc_count,free_count-pre_free_count,out_count-pre_out_count);
                pre_malloc_count = malloc_count;
                pre_free_count = free_count;
                pre_out_count = out_count;
                if (t++>=display)
                {
                        t = 0;
                        if (!CKMalloc::GetInstance().TestMemoryPage(free_page, used_page, unmem_page))
                        {
                                SHM_ASSERT(0);
                        }
                        else
                        {
                                printf("mem = %d  free = %d  unmem = %d \n",used_page,free_page,unmem_page);
                                printf("mem = %d-%d  free = %d-%d  unmem = %d-%d \n",
                                        min_used_page,max_uses_page,
                                        min_free_page,max_free_page,
                                        min_unmem_page,max_unmem_page);
                        }
                        if (!CPTMalloc::TestChunk())
                        {
                                SHM_ASSERT(0);
                        }
                }

                if (abc >= 360)
                {
                        if (free_page > max_free_page)
                        {
                                max_free_page = free_page;
                        }
                        if (free_page < min_free_page)
                        {
                                min_free_page = free_page;
                        }
                        if (used_page > max_uses_page)
                        {
                                max_uses_page = used_page;
                        }
                        if (used_page < min_used_page)
                        {
                                min_used_page = used_page;
                        }
                        if (unmem_page > max_unmem_page)
                        {
                                max_unmem_page = unmem_page;
                        }
                        if (unmem_page < min_unmem_page)
                        {
                                min_unmem_page = unmem_page;
                        }
                }
                else
                {
                        abc++;
                }

                sleep(10);

        }
        for (int ix = 0; ix < 13; ix++)
        {
               pthread_join(ntid[ix],NULL);
        }
        for( int ix = 0; ix < g_long_size; ix++)
        {
                if (g_long_p[ix])
                {
                       g_stat_mem_size -= CPTMalloc::MallocSize(g_long_p[ix]);
                       CPTMalloc::Free(g_long_p[ix]);
                       g_long_p[ix] = 0;
                       g_stat_free_count++;
                }
        }

        printf("%lu: mem = %lld  max_mem = 0x%llx  mem_count = 0x%llx  free_count = 0x%llx  out_count = 0x%llx\n",
                        pthread_self(),g_stat_mem_size,g_stat_max_mem_size,g_stat_malloc_count,g_stat_free_count,g_stat_out_mem_count);

        if (!CKMalloc::GetInstance().TestMemoryPage(free_page, used_page, unmem_page))
        {
                SHM_ASSERT(0);
        }
        else
        {
                printf("mem = %d  free = %d  unmem = %d \n",used_page,free_page,unmem_page);
                printf("mem = %d-%d  free = %d-%d  unmem = %d-%d \n",
                        min_used_page,max_uses_page,
                        min_free_page,max_free_page,
                        min_unmem_page,max_unmem_page);
        }

        if (!CPTMalloc::TestChunk())
        {
                SHM_ASSERT(0);
        }
        return 0;
}

void EachFastSpeedTest(int size,int test_cast, int alter)
{
        void*p[alter];
        struct timeval tv1,tv2;
        gettimeofday(&tv1,NULL);
        int t = test_cast;
        while(t--)
        {
                for (int ix = 0; ix < alter; ix++)
                {
                        p[ix] = CPTMalloc::FastMalloc(size);
                        assert(p[ix]);
                        memset(p[ix],0xff,size);
                }
                for (int ix = 0; ix < alter; ix++)
                {
                        CPTMalloc::FastFree(p[ix],size);
                }
                for (int ix = 0; ix < alter; ix++)
                {
                        p[ix] = CPTMalloc::FastMalloc(size);
                        assert(p[ix]);
                        memset(p[ix],0xff,size);
                        CPTMalloc::FastFree(p[ix],size);
                }
        }
        gettimeofday(&tv2,NULL);
        unsigned long long diff = (tv2.tv_sec-tv1.tv_sec)*1000000+tv2.tv_usec-tv1.tv_usec;
        gettimeofday(&tv1,NULL);
        t = test_cast;
        while(t--)
        {
                for (int ix = 0; ix < alter; ix++)
                {
                        p[ix] = malloc(size);
                        memset(p[ix],0xff,size);
                }
                for (int ix = 0; ix < alter; ix++)
                {
                        free(p[ix]);
                }
                for (int ix = 0; ix < alter; ix++)
                {
                        p[ix] = malloc(size);
                        memset(p[ix],0xff,size);
                        free(p[ix]);
                }
        }
        gettimeofday(&tv2,NULL);
        unsigned long long diff2 = (tv2.tv_sec-tv1.tv_sec)*1000000+tv2.tv_usec-tv1.tv_usec;
        printf("size = %d %lld %lld %lf\n", size,diff,diff2, (double)diff/diff2);
}
void *FastSpeedTest(void*)
{
        EachFastSpeedTest(0,10,10000);
        EachFastSpeedTest(1,10,10000);
        EachFastSpeedTest(2,10,10000);
        EachFastSpeedTest(3,10,10000);
        EachFastSpeedTest(4,10,10000);
        EachFastSpeedTest(16,10,10000);
        EachFastSpeedTest(32,10,10000);
        EachFastSpeedTest(100,10,10000);
        EachFastSpeedTest(200,10,10000);
        EachFastSpeedTest(500,10,10000);
        EachFastSpeedTest(1000,10,10000);
        EachFastSpeedTest(2000,10,10000);
        EachFastSpeedTest(8000,10,10000);
        EachFastSpeedTest(1000*1000,10,100);
        EachFastSpeedTest(2000*1000,10,100);
        CPTMallocator::GetInstance()->ClearFastAllocator();
        return (void*)0;
}
int FastBaseTest()
{
        void ** point = new void* [10000];
        for (int ix = 0; ix < 10000; ix++)
        {
                point[ix] = CPTMalloc::FastMalloc(ix);
        }
        for (int ix = 0; ix < 10000; ix++)
        {
                CPTMalloc::FastFree(point[ix],ix);
        }
        for (int ix = 0; ix < 10000; ix++)
        {
                point[ix] = CPTMalloc::FastMalloc(10000-ix);
        }
        for (int ix = 0; ix < 10000; ix++)
        {
                CPTMalloc::FastFree(point[ix],10000-ix);
        }
        for (int ix = 0; ix < 100; ix++)
        {
                for (int iy = 0; iy < 100; iy++)
                {
                        point[ix*100+iy] = CPTMalloc::FastMalloc(ix*ix+1024+iy);
                }
        }
        for (int ix = 0; ix < 100; ix++)
        {
                for (int iy = 0; iy < 100; iy++)
                {
                        CPTMalloc::FastFree(point[ix*100+iy],ix*ix+1024+iy);
                }
        }
        for (int ix = 0; ix < 100; ix++)
        {
                for (int iy = 0; iy < 100; iy++)
                {
                        point[ix*100+iy] = CPTMalloc::FastMalloc(iy);
                }
        }
        for (int ix = 0; ix < 100; ix++)
        {
                for (int iy = 0; iy < 100; iy++)
                {
                         CPTMalloc::FastFree(point[ix*100+iy],iy);
                }
        }
        for (int ix = 0; ix < 100; ix++)
        {
                for (int iy = 0; iy < 100; iy++)
                {
                        point[ix*100+iy] = CPTMalloc::FastMalloc(ix);
                }
        }
        for (int ix = 0; ix < 100; ix++)
        {
                for (int iy = 0; iy < 100; iy++)
                {
                         CPTMalloc::FastFree(point[ix*100+iy],ix);
                }
        }
        for (int ix = 0; ix < 100; ix++)
        {
                for (int iy = 0; iy < 100; iy++)
                {
                        point[ix*100+iy] = CPTMalloc::FastMalloc(iy);
                }
        }
        for (int ix = 0; ix < 100; ix++)
        {
                for (int iy = 0; iy < 100; iy++)
                {
                         CPTMalloc::FastFree(point[ix*100+iy],iy);
                }
        }
        for (int ix = 0; ix < 10000; ix++)
        {
                point[ix] = CPTMalloc::FastMalloc(0);
        }
        for (int ix = 0; ix < 10000; ix++)
        {
                CPTMalloc::FastFree(point[ix],0);
        }
        for (int ix = 0; ix < 10000; ix++)
        {
                point[ix] = CPTMalloc::FastMalloc(1);
        }
        for (int ix = 0; ix < 10000; ix++)
        {
                CPTMalloc::FastFree(point[ix],1);
        }
        for (int ix = 0; ix < 10000; ix++)
        {
                point[ix] = CPTMalloc::FastMalloc(2);
        }
        for (int ix = 0; ix < 10000; ix++)
        {
                CPTMalloc::FastFree(point[ix],2);
        }
        for (int ix = 0; ix < 10000; ix++)
        {
                point[ix] = CPTMalloc::FastMalloc(3);
        }
        for (int ix = 0; ix < 10000; ix++)
        {
                CPTMalloc::FastFree(point[ix],3);
        }
        for (int ix = 0; ix < 10000; ix++)
        {
                point[ix] = CPTMalloc::FastMalloc(16);
        }
        for (int ix = 0; ix < 10000; ix++)
        {
                CPTMalloc::FastFree(point[ix],16);
        }
        for (int ix = 0; ix < 10000; ix++)
        {
                point[ix] = CPTMalloc::FastMalloc(1000);
        }
        for (int ix = 0; ix < 10000; ix++)
        {
                CPTMalloc::FastFree(point[ix],1000);
        }
        for (int ix = 0; ix < 10000; ix++)
        {
                point[ix] = CPTMalloc::FastMalloc(1023);
        }
        for (int ix = 0; ix < 10000; ix++)
        {
                CPTMalloc::FastFree(point[ix],1023);
        }
        for (int ix = 0; ix < 10000; ix++)
        {
                point[ix] = CPTMalloc::FastMalloc(1024);
        }
        for (int ix = 0; ix < 10000; ix++)
        {
                CPTMalloc::FastFree(point[ix],1024);
        }
        for (int ix = 0; ix < 10000; ix++)
        {
                point[ix] = CPTMalloc::FastMalloc(2000);
        }
        for (int ix = 0; ix < 10000; ix++)
        {
                CPTMalloc::FastFree(point[ix],2000);
        }
        for (int ix = 0; ix < 100; ix++)
        {
                point[ix] = CPTMalloc::FastMalloc(1000000);
        }
        for (int ix = 0; ix < 100; ix++)
        {
                CPTMalloc::FastFree(point[ix],1000000);
        }
        for (int ix = 0; ix < 100; ix++)
        {
                point[ix] = CPTMalloc::FastMalloc(2000000);
        }
        for (int ix = 0; ix < 100; ix++)
        {
                CPTMalloc::FastFree(point[ix],2000000);
        }
        for (int ix = 0; ix < 2000; ix++)
        {
                point[ix] = CPTMalloc::FastMalloc(1);
        }
        for (int ix = 0; ix < 2000; ix++)
        {
                point[2000+ix] = CPTMalloc::FastMalloc(2);
        }
        for (int ix = 0; ix < 2000; ix++)
        {
                point[4000+ix] = CPTMalloc::FastMalloc(1);
        }
        for (int ix = 0; ix < 2000; ix++)
        {
                point[6000+ix] = CPTMalloc::FastMalloc(13);
        }
        for (int ix = 0; ix < 2000; ix++)
        {
                 CPTMalloc::FastFree(point[ix],1);
        }
        for (int ix = 0; ix < 2000; ix++)
        {
                 CPTMalloc::FastFree(point[2000+ix],2);
        }
        for (int ix = 0; ix < 2000; ix++)
        {
                 CPTMalloc::FastFree(point[4000+ix],1);
        }
        for (int ix = 0; ix < 2000; ix++)
        {
                 CPTMalloc::FastFree(point[6000+ix],13);
        }
        delete []point;
        printf("PTFastBaseTest ok\n");
        return 0;
}
int FastCoverTest()
{
        void ** point = new void* [200000];
        int t = 0;
        for (int ix = 0; ix < 100; ix++)
        {
                for (int iy = 0; iy < 1024; iy++)
                {
                        point[t] = CPTMalloc::FastMalloc(4);
                        memset(point[t++],0,4);
                }
        }
        for (int ix = 0; ix < t; ix++)
        {
                if (point[ix])
                {
                        CPTMalloc::FastFree(point[ix],4);
                }
        }
        t = 0;
        for (int ix = 0; ix < 100; ix++)
        {
                for (int iy = 0; iy < 1024; iy++)
                point[t++] = CPTMalloc::FastMalloc(4);
        }
        for (; t >= 0; t--)
        {
                if (point[t])
                {
                        CPTMalloc::FastFree(point[t],4);
                }
        }
        delete []point;
        printf("PTFastBaseTest ok\n");
        return 0;
}
int FastRandomTest()
{
        int t = 100000;
        int max_point_num = 100000;
        void ** point = new void* [max_point_num];
        for (int ix = 0; ix < max_point_num; ix++)
        {
                point[ix] = 0;
        }
        int now_point_num = 0;
        int test_memory = 0;
        unsigned long long tatol_malloc_size = 0;
        unsigned long long max_tatol_malloc_size = 0;
        unsigned long long max_mem_size = 20000000;
        int malloc_count = 0;
        int free_count = 0;
        srand(time(NULL));
        while(t--)
        {
                bool malloc_mem = rand()%2;
                if (now_point_num >= max_point_num/100*99 || tatol_malloc_size >= max_mem_size/100*75)
                {
                        malloc_mem = false;
                }
                else if (tatol_malloc_size <= max_mem_size/100*50)
                {
                        malloc_mem = true;
                }
                if (!now_point_num)
                {
                        malloc_mem = true;
                }
                if (malloc_mem)
                {
                        int malloc_size = rand()%(1024);
                        point[now_point_num] = CPTMalloc::FastMalloc(malloc_size);
                        assert(point[now_point_num]);
                        if (point[now_point_num])
                        {
                                unsigned long long memsz = malloc_size;
                                memset(point[now_point_num],0xee,memsz);
                                *(unsigned short *)(point[now_point_num]) = (unsigned short)memsz;
                                tatol_malloc_size += memsz;
                                if (tatol_malloc_size > max_tatol_malloc_size)
                                {
                                        max_tatol_malloc_size = tatol_malloc_size;
                                }
                                now_point_num++;
                                malloc_count++;
                        }
                }
                else
                {
                        int free = rand()%now_point_num;
                        unsigned short  malloc_size = *(unsigned short *)point[free];
                        tatol_malloc_size -= malloc_size;
                        CPTMalloc::FastFree(point[free],malloc_size);
                        for (int ix = free; ix < now_point_num; ix++)
                        {
                                point[ix] = point[ix+1];
                        }
                        now_point_num--;
                        free_count++;
                }
                if (test_memory++ == 1000)
                {
                        unsigned short free_page,used_page,unmem_page;
                        test_memory = 0;
                        if (!CKernelMallocator::GetInstance().TestMemoryPage(free_page,used_page,unmem_page))
	                {
                                assert(0);
		                printf("errrrrrr\n");
	                }else
	                {
	                }
                        if (!CPTMallocator::TestChunk())
	                {
                                assert(0);
		                printf("errrrrrr\n");
	                }
                }
        }
        unsigned short free_page,used_page,unmem_page;
        CPTMallocator* ptmalloc = CPTMallocator::GetInstance();
        if (!CKernelMallocator::GetInstance().TestMemoryPage(free_page,used_page,unmem_page))
	{
                assert(0);
                printf("errrrrrr\n");
	}else
        {
	}
        if (!ptmalloc->TestChunk())
        {
                assert(0);
        }
        for (int ix = 0; ix < max_point_num; ix++)
        {
                if (point[ix])
                {
                        unsigned short  malloc_size = *(unsigned short *)point[ix];
                        CPTMalloc::FastFree(point[ix],malloc_size);
                }
                point[ix] = 0;
        }
        printf("free\nfree = %d used = %d unmem = %d malloc_count = %d free_count = %d\n",
                free_page, used_page, unmem_page, malloc_count, free_count);
        delete []point;
        printf("FastRandomTest ok\n");
        return 0;
}
void* FastTest(void*)
{
        FastBaseTest();
        FastCoverTest();
        FastRandomTest();
        CPTMallocator::GetInstance()->ClearFastAllocator();
        return (void*)0;
}
void *fast_long_thr_fn1(void*arg)
{
        unsigned long long max_malloc_size = (unsigned long long)arg;
        srand(time(NULL));
         long long max_mem_size = 0;
         long long total_mem_size = 0;
         long long total_malloc_count = 0;
         long long total_free_count = 0;
         long long total_out_mem_count = 0;
         long long mem_size = 0;
         long long malloc_count = 0;
         long long free_count = 0;
         long long out_mem_count = 0;
        int fail = 0;
        int t = 0;
        while(!stop)
        {
                int pos = rand()%g_long_size;
                int size = rand()%(max_malloc_size)+1;
                if (size < 4)
                {
                        size = 4;
                }
                size = 16;
                {
                        SHM_GUARD(&g_long_lock[pos], OBJ, SHM_ASSERT(0));
                        if (g_long_p[pos])
                        {
                                unsigned int fast_malloc_size = *(unsigned int*)g_long_p[pos];
                                mem_size -= fast_malloc_size;
                                total_mem_size -= fast_malloc_size;
                                CPTMalloc::FastFree(g_long_p[pos],fast_malloc_size);
                                g_long_p[pos] = 0;
                                free_count++;
                                total_free_count++;
                        }
                        g_long_p[pos] = CPTMalloc::FastMalloc(size);
                        malloc_count++;
                        total_malloc_count++;
                        if (g_long_p[pos])
                        {
                                memset(g_long_p[pos],0xff,size);
                                *(unsigned int*)g_long_p[pos] = size;
                                mem_size += size;
                                total_mem_size += size;
                        }
                        else
                        {
                                fail++;
                                out_mem_count++;
                                total_out_mem_count++;
                        }
                }
                if (fail >= 1)
                {
                        fail = 0;
                        sleep(10);
                }
                if (total_mem_size > max_mem_size)
                {
                        max_mem_size = total_mem_size;
                }
                if (t++ >= 100)
                {
                        t = 0;
                        {
                                SHM_GUARD(&g_stat_lock, OBJ, SHM_ASSERT(0));
                                g_stat_mem_size += mem_size;
                                if (g_stat_mem_size > g_stat_max_mem_size)
                                {
                                        g_stat_max_mem_size = g_stat_mem_size;
                                }
                                g_stat_malloc_count += malloc_count;
                                g_stat_free_count += free_count;
                                g_stat_out_mem_count += out_mem_count;
                        }
                        mem_size = 0;
                        malloc_count = 0;
                        free_count = 0;
                        out_mem_count = 0;
                        if (!CPTMalloc::TestChunk())
                        {
                                SHM_ASSERT(0);
                        }
                }
        }
        {
                SHM_GUARD(&g_stat_lock, OBJ, SHM_ASSERT(0));
                g_stat_mem_size += mem_size;
                if (g_stat_mem_size > g_stat_max_mem_size)
                {
                        g_stat_max_mem_size = g_stat_mem_size;
                }
                g_stat_malloc_count += malloc_count;
                g_stat_free_count += free_count;
                g_stat_out_mem_count += out_mem_count;
        }
        mem_size = 0;
        malloc_count = 0;
        free_count = 0;
        out_mem_count = 0;
        printf("thr_fn1 %lu ok\n", pthread_self());
        return (void*)0;
}
void *fast_long_thr_fn2(void*arg)
{
        unsigned long long start_pos = (unsigned long long)arg;
        srand(time(NULL));
        long long mem_size = 0;
        unsigned long long malloc_count = 0;
        unsigned long long free_count = 0;
        unsigned long long out_mem_count = 0;
        int t = 0;
        while(!stop)
        {
                for (int ix = (int)start_pos ; ix < g_long_size; ix+=6)
                {
                        if (g_long_p[ix])
                        {
                               SHM_GUARD(&g_long_lock[ix], OBJ, SHM_ASSERT(0));
                               if (g_long_p[ix])
                               {
                                        unsigned int fast_malloc_size = *(unsigned int *)g_long_p[ix];
                                        mem_size -= fast_malloc_size;
                                        CPTMalloc::FastFree(g_long_p[ix],fast_malloc_size);
                                        g_long_p[ix] = 0;
                                        free_count++;
                               }
                        }
                }
                if (t++ >= 100)
                {
                        t = 0;
                        {
                                SHM_GUARD(&g_stat_lock, OBJ, SHM_ASSERT(0));
                                g_stat_mem_size += mem_size;
                                if (g_stat_mem_size > g_stat_max_mem_size)
                                {
                                        g_stat_max_mem_size = g_stat_mem_size;
                                }
                                g_stat_malloc_count += malloc_count;
                                g_stat_free_count += free_count;
                                g_stat_out_mem_count += out_mem_count;
                        }
                        mem_size = 0;
                        malloc_count = 0;
                        free_count = 0;
                        out_mem_count = 0;
                }
                sleep(10);
        }
        {
                SHM_GUARD(&g_stat_lock, OBJ, SHM_ASSERT(0));
                g_stat_mem_size += mem_size;
                if (g_stat_mem_size > g_stat_max_mem_size)
                {
                        g_stat_max_mem_size = g_stat_mem_size;
                }
                g_stat_malloc_count += malloc_count;
                g_stat_free_count += free_count;
                g_stat_out_mem_count += out_mem_count;
        }
        printf("thr_fn2 %lu ok\n", pthread_self());
        return (void*)0;
}
int FastLongTimeTest()
{
        SHM_ASSERT(g_stat_lock.Init());
        for( int ix = 0; ix < g_long_size; ix++)
        {
                g_long_p[ix] = 0;
                SHM_ASSERT(g_long_lock[ix].Init());
        }
        pthread_t ntid[20];
        pthread_create(&ntid[0],NULL,fast_long_thr_fn1,(void*)32);
        pthread_create(&ntid[1],NULL,fast_long_thr_fn1,(void*)128);
        pthread_create(&ntid[2],NULL,fast_long_thr_fn1,(void*)1024);
        pthread_create(&ntid[3],NULL,fast_long_thr_fn1,(void*)1024);
        pthread_create(&ntid[4],NULL,fast_long_thr_fn1,(void*)1024);
        pthread_create(&ntid[5],NULL,fast_long_thr_fn1,(void*)1024);
        pthread_create(&ntid[6],NULL,fast_long_thr_fn1,(void*)1024);
        pthread_create(&ntid[7],NULL,fast_long_thr_fn1,(void*)4000);
        pthread_create(&ntid[8],NULL,fast_long_thr_fn1,(void*)8000);
        pthread_create(&ntid[9],NULL,fast_long_thr_fn1,(void*)50000);
        pthread_create(&ntid[10],NULL,fast_long_thr_fn1,(void*)1024000);
        pthread_create(&ntid[11],NULL,fast_long_thr_fn1,(void*)(1024*1024));
        pthread_create(&ntid[12],NULL,fast_long_thr_fn1,(void*)(8*1024*1000));
        pthread_create(&ntid[13],NULL,fast_long_thr_fn2,(void*)0);
        pthread_create(&ntid[14],NULL,fast_long_thr_fn2,(void*)1);
        pthread_create(&ntid[15],NULL,fast_long_thr_fn2,(void*)2);
        pthread_create(&ntid[16],NULL,fast_long_thr_fn2,(void*)3);
        pthread_create(&ntid[17],NULL,fast_long_thr_fn2,(void*)4);
        pthread_create(&ntid[18],NULL,fast_long_thr_fn2,(void*)5);
        long long max_mem_size = 0;
        long long mem_size = 0;
        long long malloc_count = 0;
        long long free_count = 0;
        long long out_count = 0;
        long long pre_malloc_count = 0;
        long long pre_free_count = 0;
        long long pre_out_count = 0;
        unsigned short free_page;
        unsigned short used_page;
        unsigned short unmem_page;
        unsigned short min_free_page = 10000;
        unsigned short max_free_page = 0;
        unsigned short min_used_page = 10000;
        unsigned short max_uses_page = 0;
        unsigned short min_unmem_page = 10000;
        unsigned short max_unmem_page = 0;
        int t = 0;
        int abc = 0;
        int display = 1;
        while(!stop)
        {
                {
                        SHM_GUARD(&g_stat_lock, OBJ, SHM_ASSERT(0));
                        max_mem_size = g_stat_max_mem_size;
                        mem_size = g_stat_mem_size;
                        malloc_count = g_stat_malloc_count;
                        free_count = g_stat_free_count;
                        out_count = g_stat_out_mem_count;
                }
                printf("%lu: mem = %lld  max_mem = 0x%llx  mem_count = 0x%llx  free_count = 0x%llx  out_count = 0x%llx %lld %lld %lld\n",
                        pthread_self(),mem_size,max_mem_size,malloc_count,free_count,out_count,malloc_count-pre_malloc_count,free_count-pre_free_count,out_count-pre_out_count);
                pre_malloc_count = malloc_count;
                pre_free_count = free_count;
                pre_out_count = out_count;
                if (t++>=display)
                {
                        t = 0;
                        if (!CKMalloc::GetInstance().TestMemoryPage(free_page, used_page, unmem_page))
                        {
                                SHM_ASSERT(0);
                        }
                        else
                        {
                                printf("mem = %d  free = %d  unmem = %d \n",used_page,free_page,unmem_page);
                                printf("mem = %d-%d  free = %d-%d  unmem = %d-%d \n",
                                        min_used_page,max_uses_page,
                                        min_free_page,max_free_page,
                                        min_unmem_page,max_unmem_page);
                        }
                        if (!CPTMalloc::TestChunk())
                        {
                                SHM_ASSERT(0);
                        }
                }
                if (abc >= 360)
                {
                        if (free_page > max_free_page)
                        {
                                max_free_page = free_page;
                        }
                        if (free_page < min_free_page)
                        {
                                min_free_page = free_page;
                        }
                        if (used_page > max_uses_page)
                        {
                                max_uses_page = used_page;
                        }
                        if (used_page < min_used_page)
                        {
                                min_used_page = used_page;
                        }
                        if (unmem_page > max_unmem_page)
                        {
                                max_unmem_page = unmem_page;
                        }
                        if (unmem_page < min_unmem_page)
                        {
                                min_unmem_page = unmem_page;
                        }
                }
                else
                {
                        abc++;
                }
                sleep(10);
        }
        for (int ix = 0; ix < 19; ix++)
        {
               pthread_join(ntid[ix],NULL);
        }
        for( int ix = 0; ix < g_long_size; ix++)
        {
                if (g_long_p[ix])
                {
                       unsigned int fast_malloc_size = *(unsigned int*)g_long_p[ix];
                       CPTMalloc::FastFree(g_long_p[ix],fast_malloc_size);
                       g_long_p[ix] = 0;
                       g_stat_free_count++;
                }
        }
        printf("%lu: mem = %lld  max_mem = 0x%llx  mem_count = 0x%llx  free_count = 0x%llx  out_count = 0x%llx\n",
                        pthread_self(),g_stat_mem_size,g_stat_max_mem_size,g_stat_malloc_count,g_stat_free_count,g_stat_out_mem_count);
        if (!CKMalloc::GetInstance().TestMemoryPage(free_page, used_page, unmem_page))
        {
                SHM_ASSERT(0);
        }
        else
        {
                printf("mem = %d  free = %d  unmem = %d \n",used_page,free_page,unmem_page);
                printf("mem = %d-%d  free = %d-%d  unmem = %d-%d \n",
                        min_used_page,max_uses_page,
                        min_free_page,max_free_page,
                        min_unmem_page,max_unmem_page);
        }
        if (!CPTMalloc::TestChunk())
        {
                SHM_ASSERT(0);
        }
        return 0;
}
int main()
{
	start_shm_init();
        KMallocTest();
 //       LongTimeTest();

        pthread_t ntid[10];
        for (int ix = 0; ix < 10; ix++)
                pthread_create(&ntid[ix],NULL,SpeedTest2,NULL);

        for (int ix = 0; ix < 10; ix++)
                pthread_join(ntid[ix],NULL);

        for (int ix = 0; ix < 10; ix++)
                pthread_create(&ntid[ix],NULL,thr_fn,NULL);
/*
        for (int ix = 0; ix < 10; ix++)
                pthread_join(ntid[ix],NULL);
        for (int ix = 0; ix < 10; ix++)
                pthread_create(&ntid[ix],NULL,FastSpeedTest,NULL);

        for (int ix = 0; ix < 10; ix++)
                pthread_join(ntid[ix],NULL);
        ThreadTest();
        for (int ix = 0; ix < 5; ix++)
                pthread_create(&ntid[ix],NULL,FastTest,NULL);
	*/
        for (int ix = 0; ix < 5; ix++)
                pthread_join(ntid[ix],NULL);

        ThreadTest();
        PTRandomOneThrTest();
        pthread_exit(NULL);
        return 0;
}




