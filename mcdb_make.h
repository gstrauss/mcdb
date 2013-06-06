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

#include "plasma/plasma_feature.h"
#include "plasma/plasma_attr.h"
#include "plasma/plasma_stdtypes.h"  /* size_t, uint32_t, uintptr_t */
#include "mcdb.h"  /* MCDB_SLOTS */

#ifdef __cplusplus
extern "C" {
#endif

struct mcdb_hp { uintptr_t p; uint32_t h; uint32_t l; }; /*(private structure)*/
struct mcdb_hplist;                                      /*(private structure)*/

struct mcdb_make {
  size_t pos;
  size_t offset;
  char * restrict map;
  uint32_t hash_init;         /* hash init value */
  uint32_t hash_pad;          /* (padding)*/
  uint32_t (*hash_fn)(uint32_t, const void * restrict, size_t); /* hash func */
  size_t fsz;
  size_t osz;
  size_t msz;
  size_t pgalign;
  struct mcdb_hp hp;
  void * (*fn_malloc)(size_t);         /* fn ptr to malloc() */
  void (*fn_free)(void *);             /* fn ptr to free() */
  const char *fname;
  char *fntmp; /*(compiler warning for const char * restrict passed to free())*/
  int fd;
  mode_t st_mode;
  uint32_t count[MCDB_SLOTS];
  struct mcdb_hplist *head[MCDB_SLOTS];
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
  __attribute_nonnull__  __attribute_nothrow__;
extern void
mcdb_make_addbuf_data(struct mcdb_make * restrict,const char * restrict,size_t)
  __attribute_nonnull__  __attribute_nothrow__;
extern void
mcdb_make_addend(struct mcdb_make * restrict)
  __attribute_nonnull__  __attribute_nothrow__;
extern void
mcdb_make_addrevert(struct mcdb_make * restrict)
  __attribute_nonnull__  __attribute_nothrow__;


/* alias symbols with hidden visibility for use in DSO linking static mcdb.o
 * (Reference: "How to Write Shared Libraries", by Ulrich Drepper)
 * (optimization)
 * The aliases below are not a complete set of mcdb_make symbols */
#if (__GNUC_PREREQ(4,0) || __has_attribute(alias)) \
 && !(defined(__APPLE__) && defined(__MACH__)) /* not supported on Darwin */
HIDDEN extern __typeof (mcdb_make_add)
                        mcdb_make_add_h;
HIDDEN extern __typeof (mcdb_make_addbegin)
                        mcdb_make_addbegin_h;
HIDDEN extern __typeof (mcdb_make_addbuf_data)
                        mcdb_make_addbuf_data_h;
HIDDEN extern __typeof (mcdb_make_addbuf_key)
                        mcdb_make_addbuf_key_h;
HIDDEN extern __typeof (mcdb_make_addend)
                        mcdb_make_addend_h;
#else
#define mcdb_make_add_h                  mcdb_make_add
#define mcdb_make_addbegin_h             mcdb_make_addbegin
#define mcdb_make_addbuf_data_h          mcdb_make_addbuf_data
#define mcdb_make_addbuf_key_h           mcdb_make_addbuf_key
#define mcdb_make_addend_h               mcdb_make_addend
#endif


#ifdef __cplusplus
}
#endif

#endif
