/*
 * mcdb_make - create mcdb
 *
 * Copyright (c) 2010, Glue Logic LLC. All rights reserved. code()gluelogic.com
 *
 *  This file is part of mcdb.
 *
 *  mcdb is free software: you can redistribute it and/or modify it under
 *  the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 2.1 of the License, or
 *  (at your option) any later version.
 *
 *  mcdb is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with mcdb.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * mcdb is originally based upon the Public Domain cdb-0.75 by Dan Bernstein
 */

#ifndef INCLUDED_MCDB_MAKE_H
#define INCLUDED_MCDB_MAKE_H

#include <sys/types.h> /* size_t   */

#include "code_attributes.h"

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


/*
 * Note: mcdb *_make_* routines are not thread-safe
 * (no need for thread-safety; mcdb is typically created from a single stream)
 */


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
