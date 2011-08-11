/* Copyright 2010  Glue Logic LLC
 * License: GPLv3
 */

#ifdef __GNUC__
#define __USE_STRING_INLINES
#endif

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif
#ifndef _XOPEN_SOURCE /* 600 for posix_fallocate(), >= 500 for fdatasync() */
#define _XOPEN_SOURCE 600
#endif
/* gcc -std=c99 hides MAP_ANONYMOUS
 * _BSD_SOURCE or _SVID_SOURCE needed for mmap MAP_ANONYMOUS on Linux */
#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif
/* large file support needed for mmap() offset,ftruncate() on cdb > 2 GB */
#if defined(_AIX)
#ifndef _LARGE_FILES
#define _LARGE_FILES
#endif
#else /*#elif defined(__linux) || defined(__sun) || defined(__hpux)*/
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif
#ifndef _LARGEFILE_SOURCE
#define _LARGEFILE_SOURCE 1
#endif
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE 1
#endif
#endif

#include "mcdb_make.h"
#include "mcdb.h"
#include "nointr.h"
#include "uint32.h"
#include "code_attributes.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>  /* memcpy() */
#include <stdint.h>  /* uint32_t uintptr_t */
#include <stdlib.h>  /* posix_fallocate() */
#include <fcntl.h>   /* posix_fallocate() */
#include <limits.h>  /* UINT_MAX, INT_MAX */

#define MCDB_HPLIST 4000

struct mcdb_hp { uintptr_t p; uint32_t h; uint32_t l; };

struct mcdb_hplist {
  uint32_t num;  /* index into struct mcdb_hp hp[MCDB_HPLIST] */
  struct mcdb_hplist *next;
  struct mcdb_hp hp[MCDB_HPLIST];
};

static struct mcdb_hplist *  __attribute_noinline__  __attribute_malloc__
mcdb_hplist_alloc(struct mcdb_make * const restrict m)
  __attribute_nonnull__  __attribute_warn_unused_result__;
static struct mcdb_hplist *
mcdb_hplist_alloc(struct mcdb_make * const restrict m)
{
    struct mcdb_hplist * const restrict head =
      (struct mcdb_hplist *) m->fn_malloc(sizeof(struct mcdb_hplist));
    if (!head) return NULL;
    head->num  = 0;
    head->next = m->head;
    return (m->head = head);
}

static bool  inline
mcdb_mmap_commit(struct mcdb_make * const restrict m,
                 char header[MCDB_HEADER_SZ])
  __attribute_nonnull__  __attribute_warn_unused_result__;
static bool  inline
mcdb_mmap_commit(struct mcdb_make * const restrict m,
                 char header[MCDB_HEADER_SZ])
{
    if (m->fd == -1) return true; /*(m->fd == -1 during large mcdb size tests)*/

    return (    0 == nointr_ftruncate(m->fd, (off_t)m->pos)
            &&  0 == posix_fallocate(m->fd, m->offset, m->pos - m->offset)
            &&  0 == msync(m->map, m->pos - m->offset, MS_SYNC)
            && -1 != lseek(m->fd, 0, SEEK_SET)
            && -1 != nointr_write(m->fd, header, MCDB_HEADER_SZ)
            &&  0 == fdatasync(m->fd));
    /* fdatasync() on Linux happens to ensure prior msync() MS_ASYNC complete.
     * If there is no way on other platforms to ensure this, then use MS_SYNC
     * unconditionally in mcdb_mmap_upsize() call to msync().  Most (all?)
     * modern UNIX use a unified page cache, which should fsync as we expect. */
}

static bool  __attribute_noinline__
mcdb_mmap_upsize(struct mcdb_make * const restrict m, const size_t sz)
  __attribute_nonnull__  __attribute_warn_unused_result__;
static bool  __attribute_noinline__
mcdb_mmap_upsize(struct mcdb_make * const restrict m, const size_t sz)
{
    size_t offset;
    size_t msz;

    /* flush and munmap prior mmap */
    if (m->map != MAP_FAILED) {
        if (m->fd != -1) { /* (m->fd == -1 during some large mcdb size tests) */
            /* msync MS_ASYNC, except MS_SYNC for mmap containing mcdb header */
            if (posix_fallocate(m->fd, m->offset, m->pos - m->offset) != 0
                || msync(m->map, m->pos - m->offset,
                         m->offset >= MCDB_HEADER_SZ ? MS_ASYNC : MS_SYNC) != 0)
                return false;
        }
        if (munmap(m->map, m->msz) != 0) return false;
        m->map = MAP_FAILED;
    }

  #if !defined(_LP64) && !defined(__LP64__)  /* (no 4 GB limit in 64-bit) */
    /* limit max size of mcdb to (4 GB - pagesize) */
    if (sz > (UINT_MAX & m->pgalign)) { errno = EOVERFLOW; return false; }
  #endif

    offset = m->offset + ((m->pos - m->offset) & m->pgalign);
    msz = (MCDB_MMAP_SZ > sz - offset) ? MCDB_MMAP_SZ : sz - offset;
  #if !defined(_LP64) && !defined(__LP64__)  /* (no 4 GB limit in 64-bit) */
    if (offset > (UINT_MAX & m->pgalign) - msz)
        msz = (UINT_MAX & m->pgalign) - offset;
  #endif

    m->fsz = offset + msz; /* (mcdb_make mmap region is always to end of file)*/
    if (m->fd != -1 && nointr_ftruncate(m->fd,(off_t)m->fsz) != 0) return false;

    /* (compilation with large file support enables off_t max > 2 GB in cast) */
    m->map = (m->fd != -1) /* (m->fd == -1 during some large mcdb size tests) */
      ? (char *)mmap(0, msz, PROT_WRITE, MAP_SHARED, m->fd, (off_t)offset)
      : (char *)mmap(0, msz, PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (m->map == MAP_FAILED) return false;
    posix_madvise(m->map, msz, POSIX_MADV_SEQUENTIAL);
    m->offset = offset;
    m->msz = msz;
    return true;
}

int
mcdb_make_addbegin(struct mcdb_make * const restrict m,
                   const size_t keylen, const size_t datalen)
{
    /* validate/allocate space for next key/data pair
     * (could cause swap thrash if key and data are larger than available mem)*/
    char *p;
    const size_t pos = m->pos;
    const size_t len = 8 + keylen + datalen;/* arbitrary ~2 GB limit for lens */
    struct mcdb_hplist * const head =
      m->head->num < MCDB_HPLIST ? m->head : mcdb_hplist_alloc(m);
    if (head == NULL) return -1;
    head->hp[head->num].p = pos;
    head->hp[head->num].h = UINT32_HASH_DJB_INIT;
    head->hp[head->num].l = (uint32_t)keylen;
    if (keylen > INT_MAX-8 || datalen > INT_MAX-8) { errno=EINVAL; return -1; }
  #if !defined(_LP64) && !defined(__LP64__)  /* (no 4 GB limit in 64-bit) */
    if (pos > UINT_MAX-len)                        { errno=ENOMEM; return -1; }
  #endif
    if (m->fsz < pos+len && !mcdb_mmap_upsize(m,pos+len))          return -1;
    p = m->map + pos - m->offset;
    uint32_strpack_bigendian_macro(p,keylen);
    uint32_strpack_bigendian_macro(p+4,datalen);
    m->pos += 8;
    return 0;
}

void  inline
mcdb_make_addbuf_data(struct mcdb_make * const restrict m,
                      const char * const restrict buf, const size_t len)
{
    /* len validated in mcdb_make_addbegin(); passing any other len is wrong,
     * unless the len is shorter from partial contents of buf. */
    memcpy(m->map + m->pos - m->offset, buf, len);
    m->pos += len;
}

void  inline
mcdb_make_addbuf_key(struct mcdb_make * const restrict m,
                     const char * const restrict buf, const size_t len)
{
    /* len validated in mcdb_make_addbegin(); passing any other len is wrong,
     * unless the len is shorter from partial contents of buf. */
    uint32_t * const h = &m->head->hp[m->head->num].h;
    *h = uint32_hash_djb(*h, buf, len);
    mcdb_make_addbuf_data(m, buf, len);
}

void  inline
mcdb_make_addend(struct mcdb_make * const restrict m)
{
    ++m->head->num;
}

void  inline
mcdb_make_addrevert(struct mcdb_make * const restrict m)
{
    m->pos = m->head->hp[m->head->num].p;
}

int
mcdb_make_add(struct mcdb_make * const restrict m,
              const char * const restrict key, const size_t keylen,
              const char * const restrict data, const size_t datalen)
{
    if (mcdb_make_addbegin(m, keylen, datalen) == 0) {
        mcdb_make_addbuf_key(m, key, keylen);
        mcdb_make_addbuf_data(m, data, datalen);
        mcdb_make_addend(m);
        return 0;
    }
    return -1;
}

/* Note: it is recommended that fd be the fd returned from a call to mkstemp()
 * and that the temporary file be renamed (by the caller) upon success */
int
mcdb_make_start(struct mcdb_make * const restrict m, const int fd,
                void * (*fn_malloc)(size_t), void (*fn_free)(void *))
{
    m->map       = MAP_FAILED;
    m->pos       = MCDB_HEADER_SZ;
    m->offset    = 0;
    m->head      = NULL;
    m->fd        = fd;
    m->fn_malloc = fn_malloc;
    m->fn_free   = fn_free;
    m->pgalign   = ~( ((size_t)sysconf(_SC_PAGESIZE)) - 1 );

    if (mcdb_mmap_upsize(m, MCDB_MMAP_SZ)  /* MCDB_MMAP_SZ >= MCDB_HEADER_SZ */
        && mcdb_hplist_alloc(m) != NULL) { /*init to skip NULL check every add*/
        m->head->hp[m->head->num].p = m->pos;
        return 0;
    }
    else {
        const int errsave = errno;
        mcdb_make_destroy(m);
        errno = errsave;
        return -1;
    }
}

int
mcdb_make_finish(struct mcdb_make * const restrict m)
{
    /* Current code below effectively limits mcdb to approx 2 billion entries.
     * (256 million when compiled 32-bit, in order to keep hash table in memory,
     *  and in practice, size of data fitting in 4 GB will impose lower limit)
     * Use of 32-bit hash is the basis for continuing to use 32-bit structures.
     * Even a mostly uniform distribution of hash keys will likely show
     * increasing number of collisions as number of keys approaches 2 billion.*/
    uint32_t u;
    uint32_t i;
    uintptr_t d;
    uint32_t len;
    uint32_t cnt;
    uint32_t w;
    struct mcdb_hp *hash;
    struct mcdb_hp *split;
    struct mcdb_hp *hp;
    struct mcdb_hplist *x;
    char *p;
    uint32_t count[MCDB_SLOTS];
    uint32_t start[MCDB_SLOTS];
    char header[MCDB_HEADER_SZ];

    memset(count, '\0', sizeof(count));

    cnt = 0; /* num entries */
    for (x = m->head; x; x = x->next) {
        u = x->num;
        if (u > UINT_MAX - cnt)                { errno = ENOMEM; return -1; }
        cnt += u;
        while (u--)
            ++count[MCDB_SLOT_MASK & x->hp[u].h];
    }

    len = 1; /* memsize */
    for (i = 0; i < MCDB_SLOTS; ++i) {
        u = count[i] << 1;
        if (u > len)
            len = u;
    }

    /* check for integer overflow and that sufficient space allocated in file */
    if (cnt > INT_MAX || len > INT_MAX)        { errno = ENOMEM; return -1; }
    len += cnt;
  #if !defined(_LP64) && !defined(__LP64__)
    if (len > UINT_MAX/sizeof(struct mcdb_hp)) { errno = ENOMEM; return -1; }
    if (cnt > (UINT_MAX>>5))                   { errno = ENOMEM; return -1; }
    u = cnt << 5; /* multiply by 2 and then by 16 (for 16 chars) */
    if (m->pos > (UINT_MAX-u))                 { errno = ENOMEM; return -1; }
  #endif

    /* add "hole" for alignment; incompatible with djb cdbdump */
    /* padding to align hash tables to MCDB_PAD_ALIGN bytes (16) */
    d = (MCDB_PAD_ALIGN - (m->pos & MCDB_PAD_MASK)) & MCDB_PAD_MASK;
  #if !defined(_LP64) && !defined(__LP64__)
    if (d > (UINT_MAX-(m->pos+u)))             { errno = ENOMEM; return -1; }
  #endif
    if (m->fsz < m->pos+d && !mcdb_mmap_upsize(m,m->pos+d)) return -1;
    if (d) memset(m->map + m->pos - m->offset, '\0', d);
    m->pos += d;                     /* clear hole for binary cmp of mcdbs */

    split = (struct mcdb_hp *) m->fn_malloc((size_t)len*sizeof(struct mcdb_hp));
    if (!split) return -1;
    hash = split + cnt;

    for (u = 0, i = 0; i < MCDB_SLOTS; ++i) {
        u += count[i]; /* bounded by cnt number of entries, so no overflow */
        start[i] = u;
    }

    for (x = m->head; x; x = x->next) {
        u = x->num;
        while (u--)
            split[--start[MCDB_SLOT_MASK & x->hp[u].h]] = x->hp[u];
    }

    for (i = 0; i < MCDB_SLOTS; ++i) {
        cnt = count[i];
        len = cnt << 1; /* no overflow possible */
        d   = m->pos;

        /* check for sufficient space in mmap to write hash table for this slot
         * (integer overflow not possible: total size checked outside loop) */
        if (m->fsz < d+((size_t)len << 4)
            && !mcdb_mmap_upsize(m, d+((size_t)len << 4)))
            break;

        /* constant header (16 bytes per header slot, so multiply by 16) */
        p = header + (i << 4);  /* (i << 4) == (i * 16) */
        uint64_strpack_bigendian_aligned_macro(p,(uint64_t)d); /* hpos */
        uint32_strpack_bigendian_aligned_macro(p+8,len);       /* hslots */
        *(uint32_t *)(p+12) = 0;     /*(fill hole with 0 only for consistency)*/

        /* generate hash table for this slot */
        memset(hash, 0, (size_t)len * sizeof(struct mcdb_hp));
        hp = split + start[i];
        for (u = 0; u < cnt; ++u) {
            w = (hp->h >> MCDB_SLOT_BITS) % len;
            while (hash[w].p)
                if (++w == len)
                    w = 0;
            hash[w] = *hp++;
        }

        /* write hash table directly to map; allocated space checked above */
        for (u = 0; u < len; ++u) {
            p = m->map + m->pos - m->offset;
            m->pos += 16; /* sizeof(struct mcdb_hp) */
            uint32_strpack_bigendian_aligned_macro(p,hash[u].h);   /*khash*/
            uint32_strpack_bigendian_aligned_macro(p+4,hash[u].l); /*klen,dpos*/
            uint64_strpack_bigendian_aligned_macro(p+8,(uint64_t)hash[u].p);
        }
    }
    m->fn_free(split);

    return (i == MCDB_SLOTS) && mcdb_mmap_commit(m, header)
        ? mcdb_make_destroy(m)
        : -1;
}

/* caller should call mcdb_make_destroy() upon errors from mcdb_make_*() calls
 * (already called when mcdb_make_finish() is successful)
 * m->fd is not closed here since mcdb_make_start() takes open file descriptor
 * (caller should cleanup m->fd)
 */
int __attribute_noinline__
mcdb_make_destroy(struct mcdb_make * const restrict m)
{
    void * const map = m->map;
    struct mcdb_hplist *n;
    struct mcdb_hplist *node = m->head;
    while ((n = node)) {
        node = node->next;
        m->fn_free(n);
    }
    m->head = NULL;
    m->map = MAP_FAILED;
    return (map != MAP_FAILED) ? munmap(map, m->msz) : 0;
}
