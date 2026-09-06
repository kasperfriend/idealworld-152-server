/* NOTE (Windows): this file is named ASSERT.h and lives on a compiler
 * -I path, so on case-insensitive filesystems `#include <assert.h>`
 * resolves HERE instead of the CRT's assert.h.  Chain to the real one
 * so lowercase assert() keeps working; each copy has a unique guard
 * so the whole chain runs exactly once per translation unit. */
#ifdef __GNUC__
#include_next <assert.h>
#endif

#ifndef __CM_LIB_ASSERT_H_CSKILL__
#define __CM_LIB_ASSERT_H_CSKILL__

#ifdef __cplusplus
extern "C"
{
#endif
int * ASSERT_FAIL(const char * msg, const char *filename , int line);
#ifdef __cplusplus
}
#endif


#ifdef  NDEBUG
#define ASSERT(expr)           ( (void)(0))
#else
#define ASSERT(expr)	(void)((expr)?1:(*ASSERT_FAIL(#expr,__FILE__,__LINE__) = 0)) 
#endif

#endif

