/*
 * mcdb - fast, reliable, simple code to create and read constant databases
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

#ifndef INCLUDED_MCDB_H
#define INCLUDED_MCDB_H

#include <stdbool.h>  /* bool     */
#include <stdint.h>   /* uint32_t, uintptr_t */
#include <unistd.h>   /* size_t   */
#include <sys/time.h> /* time_t   */

#if !defined(_POSIX_MAPPED_FILES) || !(_POSIX_MAPPED_FILES-0) \
 || !defined(_POSIX_SYNCHRONIZED_IO) || !(_POSIX_SYNCHRONIZED_IO-0)
#error "mcdb requires mmap() and msync() support"
#endif

#include "code_attributes.h"

#ifdef __cplusplus
extern "C" {
#endif

struct mcdb_mmap {
  unsigned char *ptr;         /* mmap pointer */
  uint32_t b;                 /* hash table stride bits: (data < 4GB) ? 3 : 4 */
  uint32_t n;                 /* num records in mcdb */
  uintptr_t size;             /* mmap size */
  time_t mtime;               /* mmap file mtime */
  struct mcdb_mmap * volatile next;    /* updated (new) mcdb_mmap */
  void * (*fn_malloc)(size_t);         /* fn ptr to malloc() */
  void (*fn_free)(void *);             /* fn ptr to free() */
  volatile uint32_t refcnt;            /* registered access reference count */
  int dfd;                    /* fd open to dir in which mmap file resides */
  char *fname;                /* basename of mmap file, relative to dir fd */
  char fnamebuf[64];          /* buffer in which to store short fname */
};

struct mcdb {
  struct mcdb_mmap *map;
  uint32_t loop;   /* num of hash slots searched under this key */
  uint32_t hslots; /* num of hash slots; init if mcdb_findtagstart() ret true */
  uintptr_t kpos;  /* initialized if mcdb_findtagstart() returns true */
  uintptr_t hpos;  /* initialized if mcdb_findtagstart() returns true */
  uintptr_t dpos;  /* initialized if mcdb_findtagnext() returns true */
  uint32_t dlen;   /* initialized if mcdb_findtagnext() returns true */
  uint32_t klen;   /* initialized if mcdb_findtagnext() returns true */
  uint32_t khash;  /* initialized by call to mcdb_findtagstart() */
};

extern bool
mcdb_findtagstart(struct mcdb * restrict, const char * restrict, size_t,
                  unsigned char)/* note: must be 0 or cast to (unsigned char) */
  __attribute_nonnull__  __attribute_warn_unused_result__  __attribute_hot__;
extern bool
mcdb_findtagnext(struct mcdb * restrict, const char * restrict, size_t,
                 unsigned char) /* note: must be 0 or cast to (unsigned char) */
  __attribute_nonnull__  __attribute_warn_unused_result__  __attribute_hot__;

#define mcdb_findstart(m,key,klen) mcdb_findtagstart((m),(key),(klen),0)
#define mcdb_findnext(m,key,klen)  mcdb_findtagnext((m),(key),(klen),0)
#define mcdb_find(m,key,klen) \
  (__builtin_expect((mcdb_findstart((m),(key),(klen))), 1) \
                  && mcdb_findnext((m),(key),(klen)))

extern void *
mcdb_read(const struct mcdb * restrict, uintptr_t, uint32_t, void * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;
extern uint32_t
mcdb_numrecs(struct mcdb * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;
extern bool
mcdb_validate_slots(struct mcdb * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;

/* (macros valid only after mcdb_find() or mcdb_find*next() returns true) */
#define mcdb_datapos(m)      ((m)->dpos)
#define mcdb_datalen(m)      ((m)->dlen)
#define mcdb_dataptr(m)      ((m)->map->ptr+(m)->dpos)
#define mcdb_keyptr(m)       ((m)->map->ptr+(m)->dpos-(m)->klen)
#define mcdb_keylen(m)       ((m)->klen)

struct mcdb_iter {
  unsigned char *ptr;
  unsigned char *eod;
  uint32_t klen;
  uint32_t dlen;
  struct mcdb_mmap *map;
};

/* (macros valid only after mcdb_iter() returns true) */
#define mcdb_iter_datapos(iter) ((iter)->ptr-(iter)->dlen-(iter)->map->ptr)
#define mcdb_iter_datalen(iter) ((iter)->dlen)
#define mcdb_iter_dataptr(iter) ((iter)->ptr-(iter)->dlen)
#define mcdb_iter_keylen(iter)  ((iter)->klen)
#define mcdb_iter_keyptr(iter)  ((iter)->ptr-(iter)->dlen-(iter)->klen)

extern bool
mcdb_iter(struct mcdb_iter * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;
extern void
mcdb_iter_init(struct mcdb_iter * restrict, struct mcdb * restrict)
  __attribute_nonnull__;

extern struct mcdb_mmap *  __attribute_malloc__
mcdb_mmap_create(struct mcdb_mmap * restrict,
                 const char *,const char *,void * (*)(size_t),void (*)(void *))
  __attribute_nonnull_x__((3,4,5))  __attribute_warn_unused_result__;
extern void
mcdb_mmap_destroy(struct mcdb_mmap * restrict)
  ;
/* check if constant db has been updated and refresh mmap
 * (for use with mcdb mmaps held open for any period of time)
 * (i.e. for any use other than mcdb_mmap_create(), query, mcdb_mmap_destroy())
 * caller may call mcdb_mmap_refresh() before mcdb_find() or mcdb_findstart(),
 * or at other scheduled intervals, or not at all, depending on program need.
 * Note: threaded programs should use thread-safe mcdb_thread_refresh()
 * (which passes a ptr to the map ptr and might update value of that ptr ptr) */
#define mcdb_mmap_refresh(map) \
  (__builtin_expect(!mcdb_mmap_refresh_check(map), true) \
   || __builtin_expect(mcdb_mmap_reopen(map), true))
#define mcdb_mmap_refresh_threadsafe(mapptr) \
  (__builtin_expect(!mcdb_mmap_refresh_check(*(mapptr)), true) \
   || __builtin_expect(mcdb_mmap_reopen_threadsafe(mapptr), true))

extern bool
mcdb_mmap_init(struct mcdb_mmap * restrict, int)
  __attribute_nonnull__  __attribute_warn_unused_result__;
extern void
mcdb_mmap_prefault(const struct mcdb_mmap * restrict)
  __attribute_nonnull__;
extern void
mcdb_mmap_free(struct mcdb_mmap * restrict)
  ;
extern bool
mcdb_mmap_reopen(struct mcdb_mmap * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;
extern bool
mcdb_mmap_refresh_check(const struct mcdb_mmap * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;


enum mcdb_flags {
  MCDB_REGISTER_USE_DECR = 0,
  MCDB_REGISTER_USE_INCR = 1,
  MCDB_REGISTER_MUNMAP_SKIP = 2,
  MCDB_REGISTER_MUTEX_LOCK_HOLD = 4,
  MCDB_REGISTER_MUTEX_UNLOCK_HOLD = 8
};

extern bool
mcdb_mmap_thread_registration(struct mcdb_mmap ** restrict,
                              enum mcdb_flags)
  __attribute_nonnull__;
extern bool
mcdb_mmap_reopen_threadsafe(struct mcdb_mmap ** restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;


#define mcdb_thread_register(mcdb) \
  mcdb_mmap_thread_registration(&(mcdb)->map, MCDB_REGISTER_USE_INCR)
#define mcdb_thread_unregister(mcdb) \
  mcdb_mmap_thread_registration(&(mcdb)->map, MCDB_REGISTER_USE_DECR)
#define mcdb_thread_refresh(mcdb) \
  mcdb_mmap_refresh_threadsafe(&(mcdb)->map)
#define mcdb_thread_refresh_self(mcdb) \
  (__builtin_expect((mcdb)->map->next == NULL, true) \
   || __builtin_expect(mcdb_thread_register(mcdb), true))


#define MCDB_SLOT_BITS 8                  /* 2^8 = 256 */
#define MCDB_SLOTS (1u<<MCDB_SLOT_BITS)   /* must be power-of-2 */
#define MCDB_SLOT_MASK (MCDB_SLOTS-1)     /* bitmask */
#define MCDB_HEADER_SZ (MCDB_SLOTS<<4)    /* MCDB_SLOTS * 16  (256*16=4096) */
#define MCDB_MMAP_SZ (1<<19)              /* 512KB; must be >  MCDB_HEADER_SZ */
#define MCDB_BLOCK_SZ (1<<22)             /*   4MB; must be >= MCDB_MMAP_SZ */

#define MCDB_PAD_ALIGN 16
#define MCDB_PAD_MASK (MCDB_PAD_ALIGN-1)


/* alias symbols with hidden visibility for use in DSO linking static mcdb.o
 * (Reference: "How to Write Shared Libraries", by Ulrich Drepper)
 * (optimization)
 * The aliases below are not a complete set of mcdb symbols,
 * but instead are the most common used in libnss_mcdb.so.2 */
#if defined(__GNUC__) && (__GNUC__ >= 4)
HIDDEN extern __typeof (mcdb_findtagstart)
                        mcdb_findtagstart_h;
HIDDEN extern __typeof (mcdb_findtagnext)
                        mcdb_findtagnext_h;
HIDDEN extern __typeof (mcdb_iter)
                        mcdb_iter_h;
HIDDEN extern __typeof (mcdb_iter_init)
                        mcdb_iter_init_h;
HIDDEN extern __typeof (mcdb_mmap_create)
                        mcdb_mmap_create_h;
HIDDEN extern __typeof (mcdb_mmap_destroy)
                        mcdb_mmap_destroy_h;
HIDDEN extern __typeof (mcdb_mmap_refresh_check)
                        mcdb_mmap_refresh_check_h;
HIDDEN extern __typeof (mcdb_mmap_thread_registration)
                        mcdb_mmap_thread_registration_h;
HIDDEN extern __typeof (mcdb_mmap_reopen_threadsafe)
                        mcdb_mmap_reopen_threadsafe_h;
#else
#define mcdb_findtagstart_h              mcdb_findtagstart
#define mcdb_findtagnext_h               mcdb_findtagnext
#define mcdb_iter_h                      mcdb_iter
#define mcdb_iter_init_h                 mcdb_iter_init
#define mcdb_mmap_create_h               mcdb_mmap_create
#define mcdb_mmap_destroy_h              mcdb_mmap_destroy
#define mcdb_mmap_refresh_check_h        mcdb_mmap_refresh_check
#define mcdb_mmap_thread_registration_h  mcdb_mmap_thread_registration 
#define mcdb_mmap_reopen_threadsafe_h    mcdb_mmap_reopen_threadsafe
#endif


#ifdef __cplusplus
}
#endif

#endif
