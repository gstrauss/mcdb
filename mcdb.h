/* mmap constant database (mcdb)
 *
 * Copyright 2010  Glue Logic LLC
 * License: GPLv3
 * Originally based upon the Public Domain 'cdb-0.75' by Dan Bernstein
 *
 * - updated to C99 and POSIX.1-2001 (not available/portable when djb wrote cdb)
 * - optimized for mmap access to constant db (and avoid double buffering)
 * - redesigned for use in threaded programs
 * - convenience routines to check for updated constant db and to refresh mmap
 * - 64-bit safe (needed to be able to create up to 4 GB database, max cdb size)
 *
 * Advantages over external database
 * - performance: better; avoids context switch to external database process
 * Advantages over specialized hash map
 * - generic, reusable
 * - maintained (created and verified) externally from process (less overhead)
 * - shared across processes (though shared-memory could be used for hash map)
 * - read-only (though memory pages could also be marked read-only for hash map)
 * Disadvantages to specialized hash map
 * - performance: slightly lower than specialized hash map
 * Disadvantages to djb cdb
 * - mmap requires address space be available into which to mmap the const db
 *   (i.e. large const db might fail to mmap into 32-bit process)
 * - mmap page alignment requirements and use of address space limits const db
 *   max size when created by 32-bit process.  Sizes approaching 4 GB may fail.
 * - arbitrary limit of each key or data set to (INT_MAX - 8 bytes; almost 2 GB)
 *   (djb cdb doc states there is limit besides cdb fitting into 4 GB)
 *   (writev() on some platforms in 32-bit exe might also have 2 GB limit)
 * - djb cdb tools work on input stream; mcdbctl operates on file
 *   The ability to work on an input stream forced certain design choices that
 *   might not be relevant to working on an mmap'd file, but mcdb preserves the
 *   same format for compatibility.
 *
 * Incompatibilities with djb cdb
 * - padding added at the end of key,value data to 8-byte align hash tables
 *   (incompatible with djb cdbdump)
 * - packing of integral lengths into char strings is done big-endian for
 *   performance in packing/unpacking integer data in 4-byte (or better)
 *   aligned addresses.  (incompatible with all djb cdb* tools and cdb's)
 *   (djb cdb documents all 32-bit quantities stored in little-endian form)
 */

#ifndef MCDB_H
#define MCDB_H

#include <stdbool.h>  /* bool     */
#include <stdint.h>   /* uint32_t */
#include <unistd.h>   /* size_t   */
#include <sys/time.h> /* time_t   */

#if !defined(_POSIX_MAPPED_FILES) || !(_POSIX_MAPPED_FILES-0) \
 || !defined(_POSIX_SYNCHRONIZED_IO) || !(_POSIX_SYNCHRONIZED_IO-0)
#error "mcdb requires mmap() and msync() support"
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct mcdb_mmap {
  unsigned char *ptr;         /* mmap pointer */
  uint32_t size;              /* mmap size */
  time_t mtime;               /* mmap file mtime */
  /* #ifdef _THREAD_SAFE */
  struct mcdb_mmap * volatile next;    /* updated (new) mcdb_mmap */
  void * (*fn_malloc)(size_t);         /* fn ptr to malloc() */
  void (*fn_free)(void *);             /* fn ptr to free() */
  volatile uint32_t refcnt;            /* registered access reference count */
  /* #endif */
  int dfd;                    /* fd open to dir in which mmap file resides */
  char fname[64];             /* basename of mmap file, relative to dir fd */
};

struct mcdb {
  struct mcdb_mmap *map;
  uint32_t loop;   /* number of hash slots searched under this key */
  uint32_t khash;  /* initialized if loop is nonzero */
  uint32_t kpos;   /* initialized if loop is nonzero */
  uint32_t hpos;   /* initialized if loop is nonzero */
  uint32_t hslots; /* initialized if loop is nonzero */
  uint32_t dpos;   /* initialized if cdb_findnext() returns 1 */
  uint32_t dlen;   /* initialized if cdb_findnext() returns 1 */
};

#define MCDB_HASH_INIT 5381

extern uint32_t
mcdb_hash(uint32_t, const void *, size_t);

#define mcdb_find(m,key,klen) \
  (mcdb_findstart((m),(key),(klen)) && mcdb_findnext((m),(key),(klen)))

extern bool
mcdb_findstart(struct mcdb * restrict, const char * restrict, size_t);
extern bool
mcdb_findnext(struct mcdb * restrict, const char * restrict, size_t);

extern void *
mcdb_read(struct mcdb * restrict, uint32_t, uint32_t, void * restrict);

#define mcdb_datapos(m)      ((m)->dpos)
#define mcdb_datalen(m)      ((m)->dlen)
#define mcdb_dataptr(m)      ((m)->map->ptr+(m)->dpos)

extern struct mcdb_mmap *
mcdb_mmap_create(const char *,const char *,void * (*)(size_t),void (*)(void *));
extern bool
mcdb_mmap_init(struct mcdb_mmap * restrict, int);
extern bool
mcdb_mmap_refresh(struct mcdb_mmap * restrict);
extern void
mcdb_mmap_free(struct mcdb_mmap * restrict);
extern void
mcdb_mmap_destroy(struct mcdb_mmap * restrict);


#ifdef _THREAD_SAFE

extern bool
mcdb_register_access(struct mcdb_mmap **, bool);

#define mcdb_thread_refresh(mcdb) \
  ((mcdb)->map->next == NULL || mcdb_register_access(&((mcdb)->map), true))

#endif


#define MCDB_SLOTS 256                      /* must be power-of-2 */
#define MCDB_SLOT_MASK (MCDB_SLOTS-1)       /* bitmask */
#define MCDB_HEADER_SZ (MCDB_SLOTS<<3)      /* MCDB_SLOTS * 8  (256*8 = 2048) */
#define MCDB_HEADER_MASK (MCDB_HEADER_SZ-1) /* bitmask */
#define MCDB_MMAP_SZ (1<<19)     /* 512KB; must be larger than MCDB_HEADER_SZ */


#ifdef __cplusplus
}
#endif

#endif
