#ifndef    _PTMALLOC_H_
#define    _PTMALLOC_H_
#include <cstddef>
namespace pwrdshmm
{
class CPTMalloc
{
public:
        static void *Malloc(unsigned long long sz);
        static void Free(void *p);
        static unsigned long long MallocSize(void *p);

        static void *FastMalloc(size_t sz);
        static void FastFree(void *p, size_t sz);
        static bool TestChunk();
private:
        CPTMalloc();
        CPTMalloc(const CPTMalloc&);
        CPTMalloc& operator= (const CPTMalloc&);
};

};

#endif

