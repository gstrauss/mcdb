/* TODO: portability to non-GCC compilers and earlier versions of GCC */

#ifdef __linux
#include <features.h>
#endif

#ifndef __attribute_noinline__
#define __attribute_noinline__  __attribute__((noinline))
#endif

#ifndef __attribute_nonnull__
#define __attribute_nonnull__  __attribute__((nonnull))
#endif

#ifndef __attribute_malloc__
#define __attribute_malloc__  __attribute__((malloc))
#endif

#ifndef __attribute_cold__
#define __attribute_cold__  __attribute__((cold))
#endif
