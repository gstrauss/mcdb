#ifndef INCLUDED_MCDB_ERROR_H
#define INCLUDED_MCDB_ERROR_H

#include "code_attributes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* MCDB_ERROR_* enum error values are expected to be < 0 */
enum {
  MCDB_ERROR_READFORMAT = -1,
  MCDB_ERROR_READ       = -2,
  MCDB_ERROR_WRITE      = -3,
  MCDB_ERROR_MALLOC     = -4,
  MCDB_ERROR_USAGE      = -5
};

extern const char *mcdb_usage;

extern int
mcdb_error (int, const char * restrict)
  __attribute_nonnull__  __attribute_cold__  __attribute_warn_unused_result__;

#ifdef __cplusplus
}
#endif

#endif
