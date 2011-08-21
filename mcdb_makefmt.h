#ifndef INCLUDED_MCDB_MAKEFMT_H
#define INCLUDED_MCDB_MAKEFMT_H

#include <sys/types.h>  /* size_t */

#include "code_attributes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Note: ensure output file is open() O_RDWR if calling mcdb_makefmt_fdintofd()
 * or else mmap() may fail.
 * Note: caller of mcdb_makefmt_fdintofd() should choose whether or not to then
 * call fsync() or fdatasync().  See notes in mcdb_make.c:mcdb_mmap_commit() */

int
mcdb_makefmt_fdintofd (int, char * restrict, size_t,
                       int, void * (*)(size_t), void (*)(void *))
  __attribute_nonnull__  __attribute_warn_unused_result__;

int
mcdb_makefmt_fdintofile (const int, char * restrict, size_t,
                         const char * restrict,
                         void * (*)(size_t), void (*)(void *))
  __attribute_nonnull__  __attribute_warn_unused_result__;
int
mcdb_makefmt_fileintofile (const char * restrict, const char * restrict,
                           void * (*)(size_t), void (*)(void *))
  __attribute_nonnull__  __attribute_warn_unused_result__;

#ifdef __cplusplus
}
#endif

#endif
