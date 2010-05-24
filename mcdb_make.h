/* Copyright 2010  Glue Logic LLC
 * License: GPLv3
 */

#ifndef INCLUDED_MCDB_MAKE_H
#define INCLUDED_MCDB_MAKE_H

#include <sys/types.h> /* size_t   */

#include "mcdb_attribute.h"

#ifdef __cplusplus
extern "C" {
#endif

struct mcdb_hp;                        /* (private structure) */

struct mcdb_make {
  size_t pos;
  size_t offset;
  char * restrict map;
  size_t fsz;
  size_t msz;
  size_t pgalign;
  struct mcdb_hplist *head;
  int fd;
  void * (*fn_malloc)(size_t);         /* fn ptr to malloc() */
  void (*fn_free)(void *);             /* fn ptr to free() */
};

extern int
mcdb_make_start(struct mcdb_make * restrict, int,
                void * (*)(size_t), void (*)(void *))
  __attribute_nonnull__  __attribute_warn_unused_result__;
extern int
mcdb_make_add(struct mcdb_make * restrict,
              const char * restrict, size_t,
              const char * restrict, size_t)
  __attribute_nonnull__  __attribute_warn_unused_result__;
extern int
mcdb_make_finish(struct mcdb_make * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;
extern int
mcdb_make_destroy(struct mcdb_make * restrict)
  __attribute_nonnull__;

/* support for adding entries from input stream, instead of fully in memory */
extern int
mcdb_make_addbegin(struct mcdb_make * restrict, size_t, size_t)
  __attribute_nonnull__  __attribute_warn_unused_result__;
extern void
mcdb_make_addbuf_key(struct mcdb_make * restrict,const char * restrict,size_t)
  __attribute_nonnull__;
extern void
mcdb_make_addbuf_data(struct mcdb_make * restrict,const char * restrict,size_t)
  __attribute_nonnull__;
extern void
mcdb_make_addend(struct mcdb_make * restrict)
  __attribute_nonnull__;
extern void
mcdb_make_addrevert(struct mcdb_make * restrict)
  __attribute_nonnull__;

#ifdef __cplusplus
}
#endif

#endif
