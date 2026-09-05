#ifndef    _KMALLOC_H_
#define    _KMALLOC_H_
namespace pwrdshmm
{
class CPTMallocator;
class CKMalloc
{
public:
        static CKMalloc& GetInstance();
        bool Init(int creat, int shm_key, unsigned long shm_root, unsigned long long shm_size, unsigned int zone_num, unsigned int page_size);
        void *Malloc(unsigned long long size);
        void Free(void* p);
        bool TestMemoryPage(unsigned short& free_page, unsigned short& used_page, unsigned short& unmem_page);
        void* MallocPTMallocator();
        void FreePTMallocator(void*p);
        CPTMallocator* GetPTMallocator(int id);
private:
        CKMalloc(){}
        CKMalloc(const CKMalloc&);
        CKMalloc& operator= (const CKMalloc&);

};
};
#endif
