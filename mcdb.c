/* License: GPLv3 */

#define _XOPEN_SOURCE 600  /* POSIX_MADV_RANDOM */
#define _ATFILE_SOURCE     /* fstatat(), openat() */

#include "mcdb.h"
#include "mcdb_uint32.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <limits.h>
#include <string.h>

#ifdef _THREAD_SAFE
#include <pthread.h>       /* pthread_mutex_t */
#endif

uint32_t
mcdb_hash(uint32_t h, const void * const vbuf, const size_t sz)
{
    const unsigned char * const restrict buf = (const unsigned char *)vbuf;
    size_t i = SIZE_MAX;  /* (size_t)-1; will wrap around to 0 with first ++i */
    while (++i < sz)
        h = (h + (h << 5)) ^ buf[i];
    return h;
}

/* document: read mmap directly (for efficiency) instead of using mcdb_read for
 * bounds checks.  Assume contents of struct mcdb are sane and that contents of
 * cdb are sane.  Validate a cdb before putting it into place to be loaded.
 * Since the cdb should be consistent, the numbers pulled from it should be
 * sane, and we can avoid overhead of not trusting something that is already
 * consistent.
 */

bool
mcdb_findstart(struct mcdb * const restrict m,
               const char * const restrict key, const size_t klen)
{
    const unsigned char * restrict ptr;
    const uint32_t khash = mcdb_hash(MCDB_HASH_INIT,key,klen);

  #ifdef _THREAD_SAFE
    (void) mcdb_refresh_thread(m);
    /* (ignore rc; continue with previous map in case of failure) */
  #endif

    ptr = m->map->ptr + ((khash << 3) & MCDB_HEADER_MASK) + 4;
    m->hslots = mcdb_uint32_unpack_bigendian_aligned_macro(ptr);
    if (!m->hslots)
        return false;
    m->hpos  = mcdb_uint32_unpack_bigendian_aligned_macro(ptr-4);
    m->kpos  = m->hpos + (((khash >> 8) % m->hslots) << 3);
    m->khash = khash;
    m->loop  = 0;
    return true;
}

bool
mcdb_findnext(struct mcdb * const restrict m,
              const char * const restrict key, const size_t klen)
{
    const unsigned char * ptr;
    const unsigned char * const restrict mptr = m->map->ptr;
    uint32_t vpos, khash, len;

    while (m->loop < m->hslots) {
        ptr = mptr + m->kpos + 4;
        vpos = mcdb_uint32_unpack_bigendian_aligned_macro(ptr);
        if (!vpos)
            return false;
        khash = mcdb_uint32_unpack_bigendian_aligned_macro(ptr-4);
        m->kpos += 8;
        if (m->kpos == m->hpos + (m->hslots << 3))
            m->kpos = m->hpos;
        m->loop += 1;
        if (khash == m->khash) {
            ptr = mptr + vpos;
            len = mcdb_uint32_unpack_bigendian_macro(ptr);
            if (len == klen && memcmp(key, ptr+8, klen) == 0) {
                m->dlen = mcdb_uint32_unpack_bigendian_macro(ptr+4);
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


#ifdef _THREAD_SAFE

static pthread_mutex_t mcdb_global_mutex = PTHREAD_MUTEX_INITIALIZER;

bool  __attribute__((noinline))
mcdb_register_access(struct mcdb_mmap ** const restrict mcdb_mmap,
                     const bool add)
{
    struct mcdb_mmap *map;
    struct mcdb_mmap *next;

    if (pthread_mutex_lock(&mcdb_global_mutex) != 0)
        return false;

    map = *mcdb_mmap;                   /* ? need memory barrier after mutex ?*/
    next = map->next;

    if (add && next != NULL) {
        while (next->next != NULL)
            next = next->next;
        ++next->refcnt;                 /* ? need atomic incr to L2 cache ? */
        *mcdb_mmap = next;
    }
                                        /* ? need atomic decr to L2 cache ? */
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
        mcdb_mmap_free(map);
        if (!add)
            *mcdb_mmap = NULL;
    }

    pthread_mutex_unlock(&mcdb_global_mutex);
    return true;
}

static bool
mcdb_mmap_reopen(struct mcdb_mmap * const restrict map);

static bool  __attribute__((noinline))
mcdb_mmap_reopen_thread(struct mcdb_mmap * const restrict map)
{
    struct mcdb_mmap *next;
    bool rc = false;

    if (pthread_mutex_lock(&mcdb_global_mutex) != 0)
        return false;
                              /* ? need StoreLoad memory barrier ? */
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

#endif  /* _THREAD_SAFE */


static bool  __attribute__((noinline))
mcdb_mmap_reopen(struct mcdb_mmap * const restrict map)
{
    int fd;
    bool rc;

  #if defined(__linux) || defined(__sun)
    if ((fd = openat(map->dfd, map->fname, O_RDONLY|O_NONBLOCK, 0)) != -1)
  #else
    if ((fd = open(map->fname, O_RDONLY|O_NONBLOCK, 0)) != -1)
  #endif
        return false;

    rc = mcdb_mmap_init(map, fd);

    /* close fd once it has been mmap'ed */
    while (close(fd) != 0 && errno == EINTR)
        ;

    return rc;
}

/* check if constant db has been updated and refresh mmap
 * caller may call mcdb_mmap_refresh() before mcdb_find() or mcdb_findstart(),
 * or at other scheduled intervals, or not at all, depending on program need.
 */
bool  __attribute__((noinline))
mcdb_mmap_refresh(struct mcdb_mmap * const restrict map)
{
    struct stat st;

  #if defined(__linux) || defined(__sun)
    if (fstatat(map->dfd, map->fname, &st, 0) != 0)
  #else
    if (fstat(map->fname, &st, 0) != 0)
  #endif
        return false;

    if (map->mtime == st.st_mtime)
        return true;

  #ifndef _THREAD_SAFE
    return mcdb_mmap_reopen(map);
  #else
    return mcdb_mmap_reopen_thread(map);
  #endif
}

static void  inline
mcdb_mmap_unmap(struct mcdb_mmap * const restrict map)
{
    if (map->ptr)
        munmap(map->ptr, map->size);
    map->ptr  = NULL;
    map->size = 0;    /* map->size initialization required for mcdb_read() */
}

void  __attribute__((noinline))
mcdb_mmap_free(struct mcdb_mmap * const restrict map)
{
    mcdb_mmap_unmap(map);
    if (map->fn_free)
        map->fn_free(map);
}

void  __attribute__((noinline))
mcdb_mmap_destroy(struct mcdb_mmap * const restrict map)
{
    if (map->dfd > STDERR_FILENO) {
        while (close(map->dfd) != 0 && errno == EINTR)
            ;
    }
    mcdb_mmap_free(map);
}

struct mcdb_mmap *  __attribute__((noinline))
mcdb_mmap_create(const char * const dname, const char * const fname,
                 void * (*fn_malloc)(size_t), void (*fn_free)(void *))
{
    struct mcdb_mmap * map;
    const size_t len = strlen(fname);
    int dfd = -1;

    if (len >= sizeof(map->fname))
        return NULL;
    if ((map = fn_malloc(sizeof(struct mcdb_mmap))) == NULL)
        return NULL;
  #if defined(__linux) || defined(__sun)
    dfd = open(dname, O_RDONLY, 0);
    if (dfd <= STDERR_FILENO) {/* caller must have open STDIN, STDOUT, STDERR */
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
      #ifdef _THREAD_SAFE
        ++map->refcnt;
      #endif
        return map;
    }
    else {
        /* error initializing in mcdb_mmap_reopen() */
        mcdb_mmap_destroy(map);
        return NULL;
    }
}

bool  __attribute__((noinline))
mcdb_mmap_init(struct mcdb_mmap * const restrict map, int fd)
{
    const size_t psz = (size_t) sysconf(_SC_PAGESIZE);
    struct stat st;
    void * restrict x;

    mcdb_mmap_unmap(map);

    /* size of mcdb is limited to 4 GB minus difference needed to page-align */
    if (fstat(fd,&st) != 0 || st.st_size > (UINT_MAX & ~(psz-1))) return false;
    map->size = (st.st_size & (psz-1))
      ? (st.st_size & ~(psz-1)) + psz
      : st.st_size;
    x = mmap(0,map->size,PROT_READ,MAP_SHARED,fd,0);
    if (x == MAP_FAILED) return false;
    posix_madvise(x, map->size, POSIX_MADV_RANDOM);
    map->ptr = (unsigned char *)x;
    map->mtime = st.st_mtime;
  #ifdef _THREAD_SAFE
    map->next  = NULL;
    map->refcnt= 0;
  #endif
    return true;
}


/* GPS: document: thread: update and munmap only upon initiation of new search
 *      Threads should call mcdb_refresh_thread(mcdb) at regular intervals to
 *      help free up resources sooner.
 *      document: thread: mcdb_mmap_refresh(m->map); mcdb_refresh_thread(m);
 *        (might be this in a macro)  Call to mcdb_refresh_thread() is needed
 *        to decrement reference count (and clean up) previous mmap.
 */
/* GPS: TODO portable __attribute__((noinline)) macro substitutions
 * document the ((noinline)) for reducing code bloat for less frequent paths
 */
