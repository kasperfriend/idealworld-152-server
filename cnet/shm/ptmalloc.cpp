#include "ptmalloc.h"
#include "ptmalloctor.h"
#include "shminit.h"
#include <malloc.h>
namespace pwrdshmm
{
int start_shm_init();
void *CPTMalloc::Malloc(unsigned long long sz)
{
        static int init = start_shm_init();
        if (init != CShmInit::ST_NORMAL)
        {
        return CPTMallocator::Malloc(sz);
        }
        else
        {
                return malloc((size_t)sz);
        }
}

void CPTMalloc::Free(void *p)
{
        static int init = start_shm_init();
        if (init != CShmInit::ST_NORMAL)
        {
        CPTMallocator::Free(p);
        }
        else
        {
                free(p);
        }
}

unsigned long long CPTMalloc::MallocSize(void *p)
{
        static int init = start_shm_init();
        if (init != CShmInit::ST_NORMAL)
        {
        return CPTMallocator::MallocSize(p);
        }
        else
        {
                SHM_ASSERT(0);
                return 0;
        }
}
bool CPTMalloc::TestChunk()
{
        static int init = start_shm_init();
        if (init != CShmInit::ST_NORMAL)
        {
                return CPTMallocator::TestChunk();
        }
        else
        {
                SHM_ASSERT(0);
                return false;
        }
}
void *CPTMalloc::FastMalloc(size_t sz)
{
        static int init = start_shm_init();
        if (init != CShmInit::ST_NORMAL)
        {
                return CPTMallocator::FastMalloc(sz);
        }
        else
        {
                return malloc(sz);
        }
}
void CPTMalloc::FastFree(void *p, size_t sz)
{
        static int init = start_shm_init();
        if (init != CShmInit::ST_NORMAL)
        {
                return CPTMallocator::FastFree(p,sz);
        }
        else
        {
                return free(p);
        }
}
};
