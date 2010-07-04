/* License: GPLv3 */

#ifndef _XOPEN_SOURCE /* POSIX_MADV_RANDOM */
#define _XOPEN_SOURCE 600
#endif
#ifndef _ATFILE_SOURCE /* fstatat(), openat() */
#define _ATFILE_SOURCE
#endif
#ifndef _GNU_SOURCE /* enable O_CLOEXEC on GNU systems */
#define _GNU_SOURCE 1
#endif

#include "mcdb.h"
#include "nointr.h"
#include "uint32.h"
#include "code_attributes.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <limits.h>
#include <string.h>

#ifdef _THREAD_SAFE
#include <pthread.h>       /* pthread_mutex_t, pthread_mutex_{lock,unlock}() */
static pthread_mutex_t mcdb_global_mutex = PTHREAD_MUTEX_INITIALIZER;
#else
#define pthread_mutex_lock(mutexp) 0
#define pthread_mutex_unlock(mutexp) (void)0
#endif

#ifndef O_CLOEXEC /* O_CLOEXEC available since Linux 2.6.23 */
#define O_CLOEXEC 0
#endif


/* Note: tagc of 0 ('\0') is reserved to indicate no tag */

bool
mcdb_findtagstart(struct mcdb * const restrict m,
                  const char * const restrict key, const size_t klen,
                  const unsigned char tagc)
{
    const uint32_t khash_init = /* init hash value; hash tagc if tagc not 0 */
      (tagc != 0)
        ? uint32_hash_djb_uchar(UINT32_HASH_DJB_INIT, tagc)
        : UINT32_HASH_DJB_INIT;
    const uint32_t khash = uint32_hash_djb(khash_init, key, klen);
    const unsigned char * restrict ptr;

    (void) mcdb_thread_refresh(m);
    /* (ignore rc; continue with previous map in case of failure) */

    ptr = m->map->ptr + ((khash << 3) & MCDB_HEADER_MASK) + 4;
    m->hslots = uint32_strunpack_bigendian_aligned_macro(ptr);
    if (!m->hslots)
        return false;
    m->hpos  = uint32_strunpack_bigendian_aligned_macro(ptr-4);
    m->kpos  = m->hpos + (((khash >> 8) % m->hslots) << 3);
    m->khash = khash;
    m->loop  = 0;
    return true;
}

bool
mcdb_findtagnext(struct mcdb * const restrict m,
                 const char * const restrict key, const size_t klen,
                 const unsigned char tagc)
{
    const unsigned char * ptr;
    const unsigned char * const restrict mptr = m->map->ptr;
    uint32_t vpos, khash, len;

    while (m->loop < m->hslots) {
        ptr = mptr + m->kpos + 4;
        vpos = uint32_strunpack_bigendian_aligned_macro(ptr);
        if (!vpos)
            return false;
        khash = uint32_strunpack_bigendian_aligned_macro(ptr-4);
        m->kpos += 8;
        if (m->kpos == m->hpos + (m->hslots << 3))
            m->kpos = m->hpos;
        m->loop += 1;
        if (khash == m->khash) {
            ptr = mptr + vpos;
            len = uint32_strunpack_bigendian_macro(ptr);
            if (tagc != 0
                ? len == klen+1 && tagc == ptr[8] && memcmp(key,ptr+9,klen) == 0
                : len == klen && memcmp(key,ptr+8,klen) == 0) {
                m->dlen = uint32_strunpack_bigendian_macro(ptr+4);
                m->dpos = vpos + 8 + len;
                return true;
            }
        }
    }
    return false;
}

/* read value from mmap const db into buffer and return pointer to buffer
 * (return NULL if position (offset) or length to read will be out-of-bounds)
 * Note: caller must terminate with '\0' if desired, i.e. buf[len] = '\0';
 */
void *
mcdb_read(struct mcdb * const restrict m, const uint32_t pos,
          const uint32_t len, void * const restrict buf)
{
    const uint32_t mapsz = m->map->size;
    return (pos <= mapsz && mapsz - pos >= len) /* bounds check */
      ? memcpy(buf, m->map->ptr + pos, len)
      : NULL;
}



/* Note: __attribute_noinline__ is used to mark less frequent code paths
 * to prevent inlining of seldoms used paths, hopefully improving instruction
 * cache hits.
 */


bool  __attribute_noinline__
mcdb_register_access(struct mcdb_mmap * volatile * const restrict mcdb_mmap,
                     const enum mcdb_register_flags flags)
{
    struct mcdb_mmap *map;
    struct mcdb_mmap *next;
    const bool add = ((flags & MCDB_REGISTER_USE) != 0);

    if (!(flags & MCDB_REGISTER_MUTEX_UNLOCK_HOLD)
        && pthread_mutex_lock(&mcdb_global_mutex) != 0)
        return false;

    map = *mcdb_mmap;  /*(ought to be preceded by StoreLoad memory barrier)*/
    next = map->next;

    if (add && next != NULL) {
        while (next->next != NULL)
            next = next->next;
        ++next->refcnt;
        *mcdb_mmap = next;
    }

    if (map != NULL && !(add && next == NULL) && --map->refcnt == 0) {
        /*(not efficient (n!) if lots of maps updated between accesses)*/
        while (map->next != NULL && map->next->refcnt == 0) {
            struct mcdb_mmap * prev = map;
            next = map->next;
            while (next->next != NULL && next->next->refcnt == 0) {
                prev = next;
                next = next->next;
            }
            mcdb_mmap_free(next);
            prev->next = NULL;
        }
        if (!(flags & MCDB_REGISTER_MUNMAP_SKIP)) {
            mcdb_mmap_free(map);
            if (!add)
                *mcdb_mmap = NULL;
        }
    }

    if (!(flags & MCDB_REGISTER_MUTEX_LOCK_HOLD))
        pthread_mutex_unlock(&mcdb_global_mutex);

    return true;
}

static bool  __attribute_noinline__
mcdb_mmap_reopen(struct mcdb_mmap * const restrict map)
  __attribute_nonnull__  __attribute_warn_unused_result__;

static bool  __attribute_noinline__
mcdb_mmap_reopen(struct mcdb_mmap * const restrict map)
{
    int fd;
    bool rc;

    const int oflags = O_RDONLY | O_NONBLOCK | O_CLOEXEC;
  #if defined(__linux) || defined(__sun)
    if ((fd = nointr_openat(map->dfd, map->fname, oflags, 0)) == -1)
  #else
    if ((fd = nointr_open(map->fname, oflags, 0)) == -1)
  #endif
        return false;

    rc = mcdb_mmap_init(map, fd);

    (void) nointr_close(fd); /* close fd once it has been mmap'ed */

    return rc;
}

static bool  __attribute_noinline__
mcdb_mmap_reopen_thread(struct mcdb_mmap * const restrict map)
  __attribute_nonnull__  __attribute_warn_unused_result__;

static bool  __attribute_noinline__
mcdb_mmap_reopen_thread(struct mcdb_mmap * const restrict map)
{
    struct mcdb_mmap *next;
    bool rc = false;

    if (pthread_mutex_lock(&mcdb_global_mutex) != 0)
        return false;

    if (map->next != NULL) {  /* map->next already updated */
        pthread_mutex_unlock(&mcdb_global_mutex);
        return true;
    }

    if ((next = map->fn_malloc(sizeof(struct mcdb_mmap))) != NULL) {
        memcpy(next, map, sizeof(struct mcdb_mmap));
        next->ptr = NULL;
        if ((rc = mcdb_mmap_reopen(next)))
            map->next = next;
        else
            map->fn_free(next);
    }

    pthread_mutex_unlock(&mcdb_global_mutex);
    return rc;
}

/* check if constant db has been updated and refresh mmap
 * (for use with mcdb mmaps held open for any period of time)
 * (i.e. for any use other than mcdb_mmap_create(), query, mcdb_mmap_destroy())
 * caller may call mcdb_mmap_refresh() before mcdb_find() or mcdb_findstart(),
 * or at other scheduled intervals, or not at all, depending on program need.
 */
bool  __attribute_noinline__
mcdb_mmap_refresh(struct mcdb_mmap * const restrict map)
{
    struct stat st;

    if (map->fn_malloc == NULL) /*caller misconfigured mcdb_mmap; no fn_malloc*/
        return false;

  #if defined(__linux) || defined(__sun)
    if (fstatat(map->dfd, map->fname, &st, 0) != 0)
  #else
    if (stat(map->fname, &st, 0) != 0)
  #endif
        return false;

    if (map->mtime == st.st_mtime)
        return true;

    return mcdb_mmap_reopen_thread(map);
}

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

void  __attribute_noinline__
mcdb_mmap_free(struct mcdb_mmap * const restrict map)
{
    mcdb_mmap_unmap(map);
    if (map->fn_free)
        map->fn_free(map);
}

void  __attribute_noinline__
mcdb_mmap_destroy(struct mcdb_mmap * const restrict map)
{
    if (map->dfd > STDERR_FILENO)
        (void) nointr_close(map->dfd);
    mcdb_mmap_free(map);
}

struct mcdb_mmap *  __attribute_noinline__
mcdb_mmap_create(const char * const dname  __attribute_unused__,
                 const char * const fname,
                 void * (*fn_malloc)(size_t), void (*fn_free)(void *))
{
    struct mcdb_mmap * restrict map;
    const size_t len = strlen(fname);
    int dfd = -1;

    if (len >= sizeof(map->fname))
        return NULL;
    if ((map = fn_malloc(sizeof(struct mcdb_mmap))) == NULL)
        return NULL;
  #if defined(__linux) || defined(__sun)
    /* caller must have open STDIN, STDOUT, STDERR */
    if ((dfd = nointr_open(dname, O_RDONLY | O_CLOEXEC, 0)) > STDERR_FILENO) {
        if (O_CLOEXEC == 0)
            (void) fcntl(dfd, F_SETFD, FD_CLOEXEC);
    }
    else {
        fn_free(map);
        return NULL;
    }
  #endif

    /* initialize */
    memset(map, '\0', sizeof(struct mcdb_mmap));
    map->fn_malloc = fn_malloc;
    map->fn_free   = fn_free;
    map->dfd       = dfd;
    memcpy(map->fname, fname, len+1);
    if (mcdb_mmap_reopen(map)) {
        ++map->refcnt;
        return map;
    }
    else {
        /* error initializing in mcdb_mmap_reopen() */
        mcdb_mmap_destroy(map);
        return NULL;
    }
}

bool  __attribute_noinline__
mcdb_mmap_init(struct mcdb_mmap * const restrict map, int fd)
{
    struct stat st;
    void * restrict x;

    mcdb_mmap_unmap(map);

    /* size of mcdb is limited to 4 GB minus difference needed to page-align */
    if (fstat(fd,&st) != 0) return false;
    x = mmap(0, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (x == MAP_FAILED) return false;
    posix_madvise(x, st.st_size, POSIX_MADV_RANDOM);
    map->ptr   = (unsigned char *)x;
    map->size  = st.st_size;
    map->mtime = st.st_mtime;
    map->next  = NULL;
    map->refcnt= 0;
    return true;
}


/* GPS: document: persistent open mcdb with multiple references
 *        (not just use in threads)
 *      cloning (struct mcdb_mmap) requires a call to mcdb_register()
 *        to increment refcnt.  We probably want to create a convenience routine
 *        to encapsulate some of this.
 *      thread: update and munmap only upon initiation of new search
 *      Any long-running program should run mcdb_mmap_refresh(m->map) at
 *      periodic intervals to check if the file on disk has been replaced
 *      and needs to be re-mapped to obtain the new map.
 *      Threads should call mcdb_thread_refresh(mcdb) at regular intervals to
 *      help free up resources sooner.
 *      document: thread: mcdb_mmap_refresh(m->map); mcdb_thread_refresh(m);
 *        (might put this in a macro)  Call to mcdb_thread_refresh() is needed
 *        to decrement reference count (and clean up) previous mmap.
 */
