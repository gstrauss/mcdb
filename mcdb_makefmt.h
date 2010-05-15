#ifndef MCDB_MAKEFMT_H
#define MCDB_MAKEFMT_H

#include <sys/types.h>  /* size_t */

#include "mcdb_attribute.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Note: ensure output file is open() O_RDWR if calling mcdb_makefmt_fdintofd()
 * or else mmap() may fail */

int
mcdb_makefmt_fdintofd (int, char * restrict, size_t,
                       int, void * (*)(size_t), void (*)(void *))
  __attribute_nonnull__;

int
mcdb_makefmt_fdintofile (const int, char * restrict, size_t,
                         const char * restrict,
                         void * (*)(size_t), void (*)(void *))
  __attribute_nonnull__;
int
mcdb_makefmt_fileintofile (const char * restrict, const char * restrict,
                           void * (*)(size_t), void (*)(void *))
  __attribute_nonnull__;

#ifdef __cplusplus
}
#endif

#endif
