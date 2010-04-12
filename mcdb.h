/* mmap constant database (mcdb)
 *
 * Copyright 2010  Glue Logic LLC
 * License: GPLv3
 * Originally based upon the Public Domain 'cdb-0.75' by Dan Bernstein
 *
 * - updated to C99
 * - optimized for mmap access to constant db
 * - redesigned for use in threaded programs
 * - convenience routines to check for updated constant db and to refresh mmap
 *
 * Advantages over external database
 * - performance: better; avoids context switch to external database process
 * Advantages over specialized hash map
 * - generic, reusable
 * - maintained (created and verified) externally to process (less overhead)
 * - shared across processes (though shared-memory could be used for hash map)
 * - read-only (though memory pages could also be marked read-only for hash map)
 * Disadvantages to specialized hash map
 * - performance: slightly lower than specialized hash map
 */

#ifndef MCDB_H
#define MCDB_H

#include <stdbool.h>  /* bool     */
#include <stdint.h>   /* uint32_t */
#include <unistd.h>   /* size_t   */
#include <sys/time.h> /* time_t   */

#ifndef _POSIX_MAPPED_FILES
#error "mcdb requires mmap support"
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

extern uint32_t
mcdb_hash(const void *, size_t);

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

/* inline mcdb_read() but skip bounds checks
 * caller must ensure buf size >= m->dlen
 */
#define mcdb_read_value(buf,m) memcpy((buf), (m)->map->ptr+(m)->dpos, (m)->dlen)

extern struct mcdb_mmap *
mcdb_mmap_create(const char *,const char *,void * (*)(size_t),void (*)(void *));
extern bool
mcdb_mmap_init(struct mcdb_mmap * restrict, int);
extern bool
mcdb_mmap_refresh(struct mcdb_mmap * restrict);
extern bool
mcdb_mmap_reopen(struct mcdb_mmap * restrict);
extern void
mcdb_mmap_unmap(struct mcdb_mmap * restrict);
extern void
mcdb_mmap_free(struct mcdb_mmap * restrict);
extern void
mcdb_mmap_destroy(struct mcdb_mmap * restrict);


#ifdef _THREAD_SAFE

extern bool
mcdb_register_access(struct mcdb_mmap **, bool);

#define mcdb_refresh_thread(mcdb) \
  ((mcdb)->map->next == NULL || mcdb_register_access(&((mcdb)->map), true))

#endif


#endif
