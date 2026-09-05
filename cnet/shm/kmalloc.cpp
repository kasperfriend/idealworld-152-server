#include "kmalloc.h"
#include "kmallocator.h"
namespace pwrdshmm
{

CKMalloc& CKMalloc::GetInstance()
{
        static CKMalloc inst;
        return inst;
}

void *CKMalloc::Malloc(unsigned long long size)
{
        return CKernelMallocator::GetInstance().Malloc(size);
}

void CKMalloc::Free(void* p)
{
        CKernelMallocator::GetInstance().Free(p);
}

bool CKMalloc::Init(int creat, int shm_key, unsigned long shm_root, unsigned long long shm_size, unsigned int zone_num, unsigned int page_size)
{
        return CKernelMallocator::GetInstance().Init(creat,shm_key,shm_root,shm_size,zone_num,page_size);
}

bool CKMalloc::TestMemoryPage(unsigned short& free_page, unsigned short& used_page, unsigned short& unmem_page)
{
        return CKernelMallocator::GetInstance().TestMemoryPage(free_page, used_page, unmem_page);
}

void* CKMalloc::MallocPTMallocator()
{
        return CKernelMallocator::GetInstance().MallocPTMallocator();
}

void CKMalloc::FreePTMallocator(void*p)
{
        CKernelMallocator::GetInstance().FreePTMallocator(p);
}


CPTMallocator* CKMalloc::GetPTMallocator(int id)
{
        return CKernelMallocator::GetInstance().GetPTMallocatorWithID(id);
}







};

