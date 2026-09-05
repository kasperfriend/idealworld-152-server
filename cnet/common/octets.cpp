#include "octets.h"

namespace GNET
{

Octets::Rep Octets::Rep::null = { 0, 0, 1 };

void* Octets::Rep::operator new (size_t size, size_t extra) { return malloc(size + extra); }
void  Octets::Rep::operator delete (void *p) { free(p);  }
};
