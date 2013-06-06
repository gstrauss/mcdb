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

#ifndef _XOPEN_SOURCE /* POSIX_MADV_RANDOM */
#define _XOPEN_SOURCE 600
#endif
#ifndef _ATFILE_SOURCE /* fstatat(), openat() */
#define _ATFILE_SOURCE
#endif
#ifndef _GNU_SOURCE /* enable O_CLOEXEC on GNU systems */
#define _GNU_SOURCE 1
#endif
/* large file support needed for stat(),fstat() input file > 2 GB */
#define PLASMA_FEATURE_ENABLE_LARGEFILE

/* Note: mcdb client does not support files > 4 GB when compiled 32-bit
 * since the implementation mmap()s the entire file into the address space,
 * meaning only up to 4 GB (minus memory used by program and other libraries)
 * is supported in 32-bit.  djb's original implementation using lseek() and
 * read() would support > 4 GB with the > 4 GB support added in mcdb, but
 * mcdb uses mmap() for performance instead of lseek() and read().  In current
 * mcdb implementation, client must compile and run 64-bit for > 4 GB mcdb. */

#include "mcdb.h"
#include "nointr.h"
#include "uint32.h"
#include "plasma/plasma_attr.h"
#include "plasma/plasma_membar.h"
#include "plasma/plasma_stdtypes.h"  /* SIZE_MAX */

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <limits.h>
#include <string.h>

#ifdef _THREAD_SAFE
#include "plasma/plasma_spin.h" /* plasma_spin_lock_t, plasma_spin_lock_*() */
static plasma_spin_lock_t mcdb_global_spinlock = PLASMA_SPIN_LOCK_INITIALIZER;
#else
#define plasma_spin_lock_acquire(spin) (void)0
#define plasma_spin_lock_release(spin) (void)0
#endif

#ifndef O_CLOEXEC /* O_CLOEXEC available since Linux 2.6.23 */
#define O_CLOEXEC 0
#endif

/*(posix_madvise, defines not provided in Solaris 10, even w/ __EXTENSIONS__)*/
#if (defined(__sun) || defined(__hpux)) && !defined(POSIX_MADV_NORMAL)
extern int madvise(caddr_t, size_t, int);
#define posix_madvise(addr,len,advice)  madvise((caddr_t)(addr),(len),(advice))
#define POSIX_MADV_NORMAL      0
#define POSIX_MADV_RANDOM      1
#define POSIX_MADV_SEQUENTIAL  2
#define POSIX_MADV_WILLNEED    3
#define POSIX_MADV_DONTNEED    4
#endif

/* Note: tagc of 0 ('\0') is reserved to indicate no tag */

bool
mcdb_findtagstart(struct mcdb * const restrict m,
                  const char * const restrict key, const size_t klen,
                  const unsigned char tagc)
{
    const unsigned char * restrict ptr;
    uint32_t khash;
    if (m->map->hash_fn == uint32_hash_djb) {
        const uint32_t khash_init = /*init hash value; hash tagc if tagc not 0*/
          (tagc != 0)
            ? uint32_hash_djb_uchar(UINT32_HASH_DJB_INIT, tagc)
            : UINT32_HASH_DJB_INIT;
        khash = uint32_hash_djb(khash_init, key, klen);
    }
    else {
        const uint32_t khash_init = /*init hash value; hash tagc if tagc not 0*/
          (tagc != 0)
            ? m->map->hash_fn(m->map->hash_init, (const char *)&tagc, 1u)
            : m->map->hash_init;
        khash = m->map->hash_fn(khash_init, key, klen);
    }

    /* (hash function should not change on refresh,
     *  else move mcdb_thread_refresh_self() before khash calculation)*/
    (void) mcdb_thread_refresh_self(m);
    /* (ignore rc; continue with previous map in case of failure) */

    /* (size of data in lvl1 hash table element is 16-bytes (shift 4 bits)) */
    ptr = m->map->ptr + ((khash & MCDB_SLOT_MASK) << 4);
    m->hpos  = uint64_strunpack_bigendian_aligned_macro(ptr);
    m->hslots= uint32_strunpack_bigendian_aligned_macro(ptr+8);
    m->loop  = 0;
    if (__builtin_expect((!m->hslots), 0))
        return false;
    /* (size of data in lvl2 hash table element is 16-bytes (shift 4 bits)) */
    m->kpos  = m->hpos
             +(((uintptr_t)((khash>>MCDB_SLOT_BITS) % m->hslots)) << m->map->b);
    ptr = m->map->ptr + m->kpos;
    __builtin_prefetch(ptr,0,_MM_HINT_T1);   /*prefetch for mcdb_findtagnext()*/
    __builtin_prefetch(ptr+64,0,_MM_HINT_T1);/*prefetch for mcdb_findtagnext()*/
    uint32_strpack_bigendian_aligned_macro(&m->khash, khash);/*store bigendian*/
    return true;
}

bool
mcdb_findtagnext(struct mcdb * const restrict m,
                 const char * const restrict key, const size_t klen,
                 const unsigned char tagc)
{
    const unsigned char * ptr;
    const unsigned char * const restrict mptr = m->map->ptr;
    const uintptr_t hslots_end= m->hpos + (((uintptr_t)m->hslots) << m->map->b);
    uintptr_t vpos;
    uint32_t khash;

    if (m->map->b == 3) {
        while (m->loop < m->hslots) {
            ptr = mptr + m->kpos;
            m->kpos += 8;
            if (__builtin_expect((m->kpos == hslots_end), 0))
                m->kpos = m->hpos;
            khash= *(uint32_t *)ptr; /* m->khash stored bigendian */
            vpos = uint32_strunpack_bigendian_aligned_macro(ptr+4);
            ptr  = mptr + vpos;
            __builtin_prefetch((char *)ptr, 0, _MM_HINT_T2);
            if (__builtin_expect((!vpos), 0))
                break;
            ++m->loop;
            if (khash == m->khash) {
                ptr = mptr + vpos + 8;
                m->klen = uint32_strunpack_bigendian_macro(ptr-8);
                m->dlen = uint32_strunpack_bigendian_macro(ptr-4);
                m->dpos = vpos + 8 + m->klen;
                if (m->klen == klen+(tagc!=0)
                    && (tagc == 0 || tagc == *ptr++) && memcmp(key,ptr,klen)==0)
                    return true;
            }
        }
    }
    else {
        while (m->loop < m->hslots) {
            ptr = mptr + m->kpos;
            m->kpos += 16;
            if (__builtin_expect((m->kpos == hslots_end), 0))
                m->kpos = m->hpos;
            khash   = *(uint32_t *)ptr; /* m->khash stored bigendian */
            m->klen = uint32_strunpack_bigendian_aligned_macro(ptr+4);
            vpos    = uint64_strunpack_bigendian_aligned_macro(ptr+8);
            __builtin_prefetch((char *)mptr+vpos+4, 0, _MM_HINT_T2);
            if (__builtin_expect((!vpos), 0))
                break;
            ++m->loop;
            if (khash == m->khash && m->klen == klen+(tagc!=0)) {
                m->dpos = vpos + 8 + m->klen;
                ptr = mptr + vpos + 8;
                m->dlen = uint32_strunpack_bigendian_macro(ptr-4);
                if ((tagc == 0 || tagc == *ptr++) && memcmp(key,ptr,klen) == 0)
                    return true;
            }
        }
    }
    return (m->loop = false);
}

/* read value from mmap const db into buffer and return pointer to buffer
 * (return NULL if position (offset) or length to read will be out-of-bounds)
 * Note: caller must terminate with '\0' if desired, i.e. buf[len] = '\0';
 */
void *
mcdb_read(const struct mcdb * const restrict m, const uintptr_t pos,
          const uint32_t len, void * const restrict buf)
{
    const uintptr_t mapsz = m->map->size;
    return (pos <= mapsz && mapsz - pos >= len) /* bounds check */
      ? memcpy(buf, m->map->ptr + pos, len)
      : NULL;
}

uint32_t
mcdb_numrecs(struct mcdb * const restrict m)
{
    struct mcdb_mmap * const restrict map = m->map;
    if (map->n == ~0) {
        const unsigned char * const restrict ptr = map->ptr;
        uint32_t u = 0;
        for (unsigned int i = 8; i < MCDB_HEADER_SZ; i += 16)
            u += uint32_strunpack_bigendian_aligned_macro(ptr+i);
        map->n = u >> 1;  /* (hslots / 2) */
    }
    return map->n; /* mcdb_make currently limits n to INT_MAX (~2 billion) */
}

bool
mcdb_validate_slots(struct mcdb * const restrict m)
{
    const unsigned char * const restrict ptr = m->map->ptr;
    uint32_t u = 0;
    const uint32_t bits = m->map->b;
    uint64_t hpos;
    uint64_t hpos_next;
    uint32_t hslots;
    uint32_t numrecs = 0;
    if (MCDB_HEADER_SZ > m->map->size)
        return false;
    hpos_next  = uint64_strunpack_bigendian_aligned_macro(ptr);
    do {
        hpos = uint64_strunpack_bigendian_aligned_macro(ptr+u);
        numrecs += (hslots = uint32_strunpack_bigendian_aligned_macro(ptr+u+8));
        if (/* __builtin_expect( (*(uint32_t *)(ptr+u+12)) == 0, 1) && */
            __builtin_expect( (hpos == hpos_next), 1)) /*(skip padding == 0)*/
            hpos_next += ((uintptr_t)hslots << bits);
        else
            return false;
    } while ((u += 16) < MCDB_HEADER_SZ);
    m->map->n = numrecs >> 1;  /* (hslots / 2) */
    return (hpos_next == m->map->size);
}

bool
mcdb_iter(struct mcdb_iter * const restrict iter)
{
    if (iter->ptr < iter->eod) {
        iter->klen = uint32_strunpack_bigendian_macro(iter->ptr);
        iter->dlen = uint32_strunpack_bigendian_macro(iter->ptr+4);
        iter->ptr += 8 + iter->klen + iter->dlen;
        if (iter->klen != ~0) {  /* (klen == ~0 padding at end of data) */
            /* klen <= INT_MAX-8 (see mcdb_make.c), so no need to also check
             *   (iter->ptr >= iter->eod-(MCDB_PAD_MASK-7))
             * (using original iter->ptr value before update above) */
            __builtin_prefetch(iter->ptr, 0, _MM_HINT_T0);
            return true;
        }
        iter->ptr = iter->eod;   /* (reached end of data; return false below) */
    }
    return false;
}

void
mcdb_iter_init(struct mcdb_iter * const restrict iter,
               struct mcdb * const restrict m)
{
    /* About eod: Last data record must begin before end-of-data (iter->eod)
     * End-of-data is beginning of open hash tables minus up to MCDB_PAD_MASK
     * MCDB_PAD_MASK is 1-filled (~0), so iter->klen == ~0 is in padding
     * MCDB_PAD_MASK might be larger than min record size, so a valid
     *   record might reside in MCDB_PAD_MASK bytes before open hash tables
     * Minimum rec size is 8 bytes for klen, dlen; 7 or fewer bytes are padding
     */
    unsigned char * const ptr = m->map->ptr;
    iter->ptr  = ptr + MCDB_HEADER_SZ;
    iter->eod  = ptr + uint64_strunpack_bigendian_aligned_macro(ptr) - 7;
    __builtin_prefetch(iter->ptr,0,_MM_HINT_T0); /*(non-faulting ld if 0 recs)*/
    iter->klen = 0;
    iter->dlen = 0;
    iter->map  = m->map;
    /* Note: callers that intend to iterate through entire mcdb might call
     * posix_madvise() on the mcdb as long as mcdb fits into physical memory,
     * e.g. posix_madvise(iter->map, (size_t)(iter->eod - iter->map),
     *                    POSIX_MADV_SEQUENTIAL | POSIX_MADV_WILLNEED);
     *      (if iter->ptr instead of iter->map, round down for page alignment)
     */
}


/* Note: __attribute_noinline__ is used to mark less frequent code paths
 * to prevent inlining of seldoms used paths, hopefully improving instruction
 * cache hits.
 */


static void  inline
mcdb_mmap_unmap(struct mcdb_mmap * const restrict map)
  __attribute_nonnull__;

static void  inline
mcdb_mmap_unmap(struct mcdb_mmap * const restrict map)
{
    if (map->ptr)
        munmap(map->ptr, map->size);
    map->ptr  = NULL;
    map->size = 0;    /* map->size initialization required for mcdb_read() */
}

__attribute_noinline__
bool
mcdb_mmap_init(struct mcdb_mmap * const restrict map, int fd)
{
    struct stat st;
    void * restrict x;

    mcdb_mmap_unmap(map);

    if (fstat(fd, &st) != 0) return false;
  #if !defined(_LP64) && !defined(__LP64__)
    if (st.st_size > (off_t)SIZE_MAX) return (errno = EFBIG, false);
  #endif
    x = mmap(0, (size_t)st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (x == MAP_FAILED) return false;
    __builtin_prefetch((char *)x, 0, _MM_HINT_T0); /*(touch page w/ mcdb hdr)*/
  #if 0 /* disable; does not appear to improve performance */
    /*(peformance hit when hitting an uncached mcdb on my 32-bit Pentium-M)*/
    if (st.st_size > 4194304) /*(skip syscall overhead if < 4 MB (arbitrary))*/
        posix_madvise(((char *)x), st.st_size, POSIX_MADV_RANDOM);
	/*(addr (x) must be aligned on _SC_PAGESIZE for madvise portability)*/
  #endif
    map->ptr   = (unsigned char *)x;
    map->size  = (uintptr_t)st.st_size;
    map->b     = st.st_size < UINT_MAX || *(uint32_t *)x == 0 ? 3u : 4u;
    map->n     = ~0;
    map->mtime = st.st_mtime;
    map->next  = NULL;
    map->refcnt= 0;
    map->hash_init = UINT32_HASH_DJB_INIT;
    map->hash_fn   = uint32_hash_djb;
    return true;
}

__attribute_noinline__
void
mcdb_mmap_prefault(const struct mcdb_mmap * const restrict map)
{
    /* improves performance on uncached mcdb (if about to make *many* queries)
     * by asking operating system to prefault pages into memory from disk
     * (call only if mcdb fits into filesystem cache in physical memory) */
    posix_madvise(((char *)map->ptr), map->size,
                  POSIX_MADV_WILLNEED | POSIX_MADV_RANDOM);
}

__attribute_noinline__
void
mcdb_mmap_free(struct mcdb_mmap * const restrict map)
{
    if (map == NULL) return;
    mcdb_mmap_unmap(map);
    if (map->fn_free) {
        if (map->fname != NULL && map->fname != map->fnamebuf) {
            map->fn_free(map->fname);
            map->fname = NULL;
        }
        if (!map->allocated)
            map->fn_free(map);
    }
}

__attribute_noinline__
void
mcdb_mmap_destroy(struct mcdb_mmap * const restrict map)
{
    if (map == NULL) return;
    if (map->dfd != -1) {
        (void) nointr_close(map->dfd);
        map->dfd = -1;
    }
    mcdb_mmap_free(map);
}

__attribute_noinline__
bool
mcdb_mmap_reopen(struct mcdb_mmap * const restrict map)
{
    int fd;
    bool rc;

    const int oflags = O_RDONLY | O_NONBLOCK | O_CLOEXEC;
  #ifdef AT_FDCWD
    if (map->dfd != -1) {
        if ((fd = nointr_openat(map->dfd, map->fname, oflags, 0)) == -1)
            return false;
    }
    else
  #endif
    if ((fd = nointr_open(map->fname, oflags, 0)) == -1)
        return false;

    rc = mcdb_mmap_init(map, fd);

    (void) nointr_close(fd); /* close fd once it has been mmap'ed */

    return rc;
}

bool
mcdb_mmap_refresh_check(const struct mcdb_mmap * const restrict map)
{
    struct stat st;
    plasma_membar_ld_datadep(); /*(thread ld order dep b/w map and map->ptr)*/
    return (map->ptr == NULL
            || ( (
                  #ifdef AT_FDCWD
                    map->dfd != -1
                      ? fstatat(map->dfd, map->fname, &st, 0) == 0
                      :
                  #endif
                        stat(map->fname, &st) == 0 )
                && map->mtime != st.st_mtime ) );
}

/*
 * Example use of mcdb_mmap_create() and mcdb_mmap_destroy():
 *   mcdb_mmap_create()
 *   repeat:
 *     (optional) mcdb_mmap_refresh())
 *     (lookups)
 *   mcdb_mmap_destroy();
 *
 * Example use in threaded program where multiple threads access mcdb_mmap:
 *   maintenance thread: mcdb_mmap_create(...)
 *
 *   maintenance thread: mcdb_mmap_refresh_threadsafe(&map) (periodic/triggered)
 *   querying threads:   mcdb_thread_register(m)
 *   querying threads:   mcdb_find(m,key,klen)         (repeat for many lookups)
 *   querying threads:   mcdb_thread_unregister(m)
 *
 *   maintenance thread: mcdb_mmap_destroy(map)
 *
 * Querying threads can register once and then may do many lookups at will.
 * (no need to register, query, unregister each time)
 * Each query (mcdb_findtagstart()) checks for updated mmap by calling
 * mcdb_thread_refresh_self().  If a thread might go for a long period of time
 * without querying, and mcdb_mmap has been updated, then thread can optionally
 * (periodically) call mcdb_thread_refresh_self() to release the outdated mmap
 * before the thread gets around (some time in the future) to its next query.
 *
 * Note: using map->dfd means that if a directory replaces existing directory,
 * mcdb_mmap will not notice.  Pass dname == NULL to skip using map->dfd.
 *
 * Note: use return value from mcdb_mmap_create().  If an error occurs during
 * mcdb_mmap_create(), fn_free(map) is called, whether or not map or NULL was
 * passed as first argument to mcdb_mmap_create().  If non-NULL map is passed
 * in to mcdb_mmap_create(), then it is -not- free'd by mcdb_mmap_destroy(),
 * though internal allocations and resources are free'd.  If NULL map is passed
 * in, then it is free'd with mcdb_mmap_destroy() since mcdb allocated the map.
 */
__attribute_noinline__
struct mcdb_mmap *
mcdb_mmap_create(struct mcdb_mmap * restrict map,
                 const char * const dname  __attribute_unused__,
                 const char * const fname,
                 void * (*fn_malloc)(size_t), void (*fn_free)(void *))
{
    char *fbuf;
    size_t flen;
    const int allocated = (map != NULL);

    if (fn_malloc == NULL)
        return NULL;
    if (map == NULL && (map = fn_malloc(sizeof(struct mcdb_mmap))) == NULL)
        return NULL;
    /* initialize */
    memset(map, '\0', sizeof(struct mcdb_mmap));
    map->fn_malloc = fn_malloc;
    map->fn_free   = fn_free;
    map->allocated = allocated;
    map->dfd       = -1;
    flen           = strlen(fname);

  #if defined(__linux__) || defined(__sun)
    if (dname != NULL) {
        /* caller must have open STDIN, STDOUT, STDERR */
        map->dfd = nointr_open(dname, O_RDONLY|O_CLOEXEC, 0);
        if (map->dfd > STDERR_FILENO) {
            if (O_CLOEXEC == 0)
                (void) fcntl(map->dfd, F_SETFD, FD_CLOEXEC);
        }
        else {
            mcdb_mmap_destroy_h(map);
            return NULL;
        }
    }
  #else
    if (dname != NULL) {
        const size_t dlen = strlen(dname);
        if (sizeof(map->fnamebuf) >= dlen+flen+2)
            fbuf = map->fnamebuf;
        else if ((fbuf = fn_malloc(dlen+flen+2)) == NULL) {
            mcdb_mmap_destroy_h(map);
            return NULL;
        }
        memcpy(fbuf, dname, dlen);
        fbuf[dlen] = '/';
        memcpy(fbuf+dlen+1, fname, flen+1);
    }
    else
  #endif
    {
        if (sizeof(map->fnamebuf) > flen)
            fbuf = map->fnamebuf;
        else if ((fbuf = fn_malloc(flen+1)) == NULL) {
            mcdb_mmap_destroy_h(map);
            return NULL;
        }
        memcpy(fbuf, fname, flen+1);
    }
    map->fname = fbuf;

    if (mcdb_mmap_reopen(map)) {
        ++map->refcnt;
        return map;
    }
    else {
        mcdb_mmap_destroy_h(map);
        return NULL;
    }
}

__attribute_noinline__
struct mcdb_mmap *
mcdb_mmap_thread_registration(struct mcdb_mmap ** const restrict mapptr,
                              const int flags)
{
    struct mcdb_mmap *map;
    struct mcdb_mmap *next = NULL;
    struct mcdb_mmap *unmap = NULL;
    const bool register_use_incr = ((flags & MCDB_REGISTER_USE_INCR) != 0);
    #define register_use_decr (!register_use_incr)

    /* use file-scoped global spinlock to protect scan of multiple mcdb_mmap's
     * (care has been taken to avoid blocking operations while holding lock) */

    if (__builtin_expect( (!(flags & MCDB_REGISTER_ALREADY_LOCKED)), 1))
        (void) plasma_spin_lock_acquire(&mcdb_global_spinlock);
    plasma_membar_ccfence();

    map = *mapptr;

    if (__builtin_expect( (map == NULL), 0)
        || (__builtin_expect( (map->ptr == NULL), 0) && register_use_incr)) {
        plasma_spin_lock_release(&mcdb_global_spinlock);
        /* succeed if unregister; fail if register */ /*(NULL is failure)*/
        return (struct mcdb_mmap *)(uintptr_t)(!register_use_incr);
        /* If registering, possibly detected race condition in which another
         * thread released final reference and mcdb was munmap()'d while current
         * thread waited for lock.  It is now invalid to attempt to register use
         * of a resource that has been released.  Caller can detect and reopen*/
    }

    if (register_use_incr) {
        if ((next = map->next) == NULL)
            ++map->refcnt;
        else {
            while (next->next != NULL)
                next = next->next;
            ++next->refcnt;
            *mapptr = next;
        }
    }

    if ((register_use_decr || next != NULL) && --map->refcnt == 0) {
        while ((next = map->next) != NULL && (next->refcnt & ~0x40000000) == 0){
            map->next = next->next;
            next->next = unmap;
            (unmap = next)->fname = NULL;   /* do not free(next->fname) yet */
        }
        if (next != NULL)
            next->refcnt &= ~0x40000000;    /* unmark; now oldest map in chain*/
        map->next = unmap;
        (unmap = map)->fname = NULL;        /* do not free(map->fname) yet */
        if (register_use_decr)
            *mapptr = NULL;
        /*(map will be free'd but map value still not NULL for return val)*/
    }
    #undef register_use_decr

    plasma_spin_lock_release(&mcdb_global_spinlock);

    /* release unused maps after releasing lock to minimize time holding lock */
    next = unmap;
    while (next != NULL) {
        next = (unmap = next)->next;
        mcdb_mmap_free(unmap);
    }

    /* return map on which incr refcnt to avoid race after unlocking */
    return map; /*(for decr refcnt, non-NULL is success, even if map free'd)*/
}

/* theaded programs (while multiple threads are using same struct mcdb_mmap)
 * must be registered with current *mapptr before calling this routine, or else
 * there is a race condition where map might be removed out from under us. */
__attribute_noinline__
bool
mcdb_mmap_reopen_threadsafe(struct mcdb_mmap ** const restrict mapptr)
{
    struct mcdb_mmap * const map = *mapptr;
    struct mcdb_mmap *next;
    bool rc;

    /* use high bit of refcnt to guard that one thread attempts reopen
     * (must lock since others modify refcnt while holding same spinlock) */
    (void) plasma_spin_lock_acquire(&mcdb_global_spinlock);
    if (map->next != NULL)
        return NULL !=                      /* registration releases spinlock */
          mcdb_mmap_thread_registration_h(mapptr, MCDB_REGISTER_USE_INCR
                                                 |MCDB_REGISTER_ALREADY_LOCKED);
    if ((rc = (map->refcnt < 0x80000000u))) /*(0!=(map->refcnt & 0x80000000u))*/
        map->refcnt |= 0x80000000u;
    plasma_spin_lock_release(&mcdb_global_spinlock);
    if (!rc)
        return true; /*other threads return, even though mcdb not reopened yet*/

    if (__builtin_expect( (map->fn_malloc == NULL), 0)
        || (next = map->fn_malloc(sizeof(struct mcdb_mmap))) == NULL)
        return false; /*(misconfigured mcdb_mmap or map->fn_malloc failed)*/

    memcpy(next, map, sizeof(struct mcdb_mmap));
    next->ptr = NULL; /*(skip munmap() in mcdb_mmap_reopen())*/
    if (map->fname == map->fnamebuf)
        next->fname = next->fnamebuf;
    rc = mcdb_mmap_reopen(next);
    if (__builtin_expect((!rc), 0)) {
        map->fn_free(next);
        return false;
    }
    next->hash_init = map->hash_init;
    next->hash_fn   = map->hash_fn;
    next->refcnt   |= 0x40000000u;    /* flag to indicate not oldest in chain */
    plasma_membar_StoreStore();
    map->next       = next;
    (void) plasma_spin_lock_acquire(&mcdb_global_spinlock);
    map->refcnt    &= ~0x80000000u;         /* registration releases spinlock */
    return NULL !=
      mcdb_mmap_thread_registration_h(mapptr, MCDB_REGISTER_USE_INCR
                                             |MCDB_REGISTER_ALREADY_LOCKED);
}


/* alias symbols with hidden visibility for use in DSO linking static mcdb.o
 * (Reference: "How to Write Shared Libraries", by Ulrich Drepper)
 * (optimization)
 * The aliases below are not a complete set of mcdb symbols,
 * but instead are the most common used in libnss_mcdb.so.2 */
#if (__GNUC_PREREQ(4,0) || __has_attribute(alias)) \
 && !(defined(__APPLE__) && defined(__MACH__)) /* not supported on Darwin */
HIDDEN extern __typeof (mcdb_findtagstart)
                        mcdb_findtagstart_h
  __attribute__((alias ("mcdb_findtagstart")));
HIDDEN extern __typeof (mcdb_findtagnext)
                        mcdb_findtagnext_h
  __attribute__((alias ("mcdb_findtagnext")));
HIDDEN extern __typeof (mcdb_iter)
                        mcdb_iter_h
  __attribute__((alias ("mcdb_iter")));
HIDDEN extern __typeof (mcdb_iter_init)
                        mcdb_iter_init_h
  __attribute__((alias ("mcdb_iter_init")));
HIDDEN extern __typeof (mcdb_mmap_create)
                        mcdb_mmap_create_h
  __attribute__((alias ("mcdb_mmap_create")));
HIDDEN extern __typeof (mcdb_mmap_destroy)
                        mcdb_mmap_destroy_h
  __attribute__((alias ("mcdb_mmap_destroy")));
HIDDEN extern __typeof (mcdb_mmap_refresh_check)
                        mcdb_mmap_refresh_check_h
  __attribute__((alias ("mcdb_mmap_refresh_check")));
HIDDEN extern __typeof (mcdb_mmap_thread_registration)
                        mcdb_mmap_thread_registration_h
  __attribute__((alias ("mcdb_mmap_thread_registration")));
HIDDEN extern __typeof (mcdb_mmap_reopen_threadsafe)
                        mcdb_mmap_reopen_threadsafe_h
  __attribute__((alias ("mcdb_mmap_reopen_threadsafe")));
#endif
