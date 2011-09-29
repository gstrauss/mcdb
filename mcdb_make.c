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

#ifdef __GNUC__
#define __USE_STRING_INLINES
#endif

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
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
#include <stdbool.h> /* bool true false */
#include <stdint.h>  /* uint32_t uintptr_t */
#include <limits.h>  /* UINT_MAX, INT_MAX */

#define MCDB_HPLIST 250

struct mcdb_hplist {
  uint32_t num;  /* index into struct mcdb_hp hp[MCDB_HPLIST] */
  struct mcdb_hplist *next;
  struct mcdb_hplist *pend;
  struct mcdb_hp hp[MCDB_HPLIST];
};

/* routine marked to indicate unlikely branch;
 * __attribute_cold__ can be used instead of __builtin_expect() */
static int  __attribute_noinline__  __attribute_cold__
mcdb_make_err(struct mcdb_make * const restrict m, int errnum)
{
    if (m != NULL) mcdb_make_destroy(m);
    errno = errnum;
    return -1;
}

static bool  __attribute_noinline__
mcdb_hplist_alloc(struct mcdb_make * const restrict m)
  __attribute_nonnull__  __attribute_warn_unused_result__;
static bool
mcdb_hplist_alloc(struct mcdb_make * const restrict m)
{
    uint32_t i = m->hp.h & MCDB_SLOT_MASK;
    struct mcdb_hplist * const head = m->head[i];
    struct mcdb_hplist * const pend = head->pend;
    if (pend != NULL) {
        pend->next = head;
        m->head[i] = pend;
        return true;
    }
    else {
        uint32_t cnt;
        const uint32_t * const count = m->count;
        struct mcdb_hplist * const restrict hplist = (struct mcdb_hplist *)
          m->fn_malloc(sizeof(struct mcdb_hplist) * MCDB_SLOTS);
        if (!hplist) return false;
        for (cnt = 0, i = 0; i < MCDB_SLOTS; ++i) {
            hplist[i].num  = 0;
            hplist[i].pend = NULL;
            if (m->head[i]->num != MCDB_HPLIST) {
                if (NULL != m->head[i]->pend)
                    hplist[i].pend = m->head[i]->pend;
                m->head[i]->pend = hplist+i;
            }
            else {
                hplist[i].next = m->head[i];
                m->head[i] = hplist+i;
            }
            cnt += count[i];
        }
        /* detect if we have already passed 2 gibibyte records
         * (not exact, but ok; will abort in mcdb_make_finish() if > INT_MAX) */
        return (cnt < INT_MAX) ? true : (errno = ENOMEM, false);
    }
}

static bool  inline
mcdb_mmap_commit(struct mcdb_make * const restrict m,
                 char header[MCDB_HEADER_SZ])
  __attribute_nonnull__  __attribute_warn_unused_result__;
static bool  inline
mcdb_mmap_commit(struct mcdb_make * const restrict m,
                 char header[MCDB_HEADER_SZ])
{
    if (m->fd == -1) { /*(m->fd == -1 during large mcdb size tests)*/
        if (m->offset == 0)
            memcpy(m->map, header, MCDB_HEADER_SZ);
        return true;
    }

    return (    0 == nointr_ftruncate(m->fd, (off_t)m->pos)
            &&  0 == msync(m->map, m->pos - m->offset, MS_ASYNC)
            && -1 != lseek(m->fd, 0, SEEK_SET)
            && -1 != nointr_write(m->fd, header, MCDB_HEADER_SZ));
    /* Most (all?) modern UNIX use a unified VM page cache, so the difference
     * between writing to mmap and then write() to fd should have identical
     * (and coherent) results.  Calling msync with MS_SYNC can be as expensive
     * as fsync() on whole file, so avoid unless necessary on specific platform.
     * Caller of mcdb_make_finish() (which calls mcdb_mmap_commit()) may wish to
     * call fsync() or fdatasync() on fd to ensure data is written to disk,
     * e.g. in case when writing new mcdb to temporary file, before renaming
     * temporary file to overwrite existing mcdb.  If not sync'd to disk and
     * OS crashes, then the update mcdb can be corrupted. */
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
    if (m->map != MAP_FAILED) {/*(m->fd==-1 during some large mcdb size tests)*/
        if ((m->fd == -1 || msync(m->map, m->pos - m->offset, MS_ASYNC) == 0)
            && munmap(m->map, m->msz) == 0)
            m->map = MAP_FAILED;
        else
            return false;
    }

  #if !defined(_LP64) && !defined(__LP64__)  /* (no 4 GB limit in 64-bit) */
    /* limit max size of mcdb to (4 GB - pagesize) */
    if (sz > (UINT_MAX & m->pgalign)) { errno = EOVERFLOW; return false; }
  #endif

    offset = m->offset + ((m->pos - m->offset) & m->pgalign);
    msz = (MCDB_MMAP_SZ > sz - offset)
      ? MCDB_MMAP_SZ
      : (sz - offset + ~m->pgalign) & m->pgalign;
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
    posix_madvise(m->map, msz, POSIX_MADV_SEQUENTIAL | POSIX_MADV_WILLNEED);
    m->offset = offset;
    m->msz = msz;
    return true;
}

int
mcdb_make_addbegin(struct mcdb_make * const restrict m,
                   const size_t keylen, const size_t datalen)
{
    /* validate/allocate space for next key/data pair */
    char *p;
    const size_t pos = m->pos;
    const size_t len = 8 + keylen + datalen;/* arbitrary ~2 GB limit for lens */
    if (m->map == MAP_FAILED)                 return mcdb_make_err(NULL,EPERM);
    if (m->hp.l== ~0 && !mcdb_hplist_alloc(m))return mcdb_make_err(NULL,errno);
    m->hp.p = pos;
    m->hp.h = UINT32_HASH_DJB_INIT;
    if (keylen>INT_MAX-8 || datalen>INT_MAX-8)return mcdb_make_err(NULL,EINVAL);
    m->hp.l = (uint32_t)keylen;
  #if !defined(_LP64) && !defined(__LP64__)  /* (no 4 GB limit in 64-bit) */
    if (pos > UINT_MAX-len)                   return mcdb_make_err(NULL,ENOMEM);
  #endif
    if (m->fsz < pos+len && !mcdb_mmap_upsize(m, pos+len))
                                              return mcdb_make_err(NULL,errno);
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
    m->hp.h = uint32_hash_djb(m->hp.h, buf, len);
    mcdb_make_addbuf_data(m, buf, len);
}

void  inline
mcdb_make_addend(struct mcdb_make * const restrict m)
{
    /* copy hp data structure into list for hp slot mask */
    const uint32_t slot_idx = m->hp.h & MCDB_SLOT_MASK;
    const uint32_t i = m->head[slot_idx]->num++;
    m->head[slot_idx]->hp[i] = m->hp;
    ++m->count[slot_idx];
    if (i == MCDB_HPLIST-1)
        m->hp.l = ~0; /* set flag for mcdb_make_start() to allocate lists */
}

void  inline
mcdb_make_addrevert(struct mcdb_make * const restrict m)
{
    m->pos = m->hp.p;
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
    m->fsz       = 0;
    m->msz       = 0;
    m->hp.p      = MCDB_HEADER_SZ;
    m->hp.h      = 0;
    m->hp.l      = 0;
    m->fd        = fd;
    m->fn_malloc = fn_malloc;
    m->fn_free   = fn_free;
    m->pgalign   = ~( ((size_t)sysconf(_SC_PAGESIZE)) - 1 );
    m->head[0]   = (struct mcdb_hplist *)
                   fn_malloc(sizeof(struct mcdb_hplist) * MCDB_SLOTS);
    memset(m->count, 0, MCDB_SLOTS * sizeof(uint32_t));
    /* do not modify m->fname, m->fntmp, m->st_mode; may already have been set*/
    if (m->head[0] != NULL && mcdb_mmap_upsize(m, MCDB_MMAP_SZ)) {
        for (uint32_t u = 0; u < MCDB_SLOTS; ++u) {
            m->head[u] = m->head[0]+u;
            m->head[u]->num  = 0;
            m->head[u]->next = NULL;
            m->head[u]->pend = NULL;
        }
        return 0;
    }
    else {
        mcdb_make_destroy(m);
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
    uint32_t b;
    char *p;
    const uint32_t * const restrict count = m->count;
    char header[MCDB_HEADER_SZ];
    if (m->map == MAP_FAILED)                  return mcdb_make_err(m,EPERM);

    for (u = 0, i = 0; i < MCDB_SLOTS; ++i)
        u += count[i];  /* no overflow; limited in mcdb_hplist_alloc */

    /* check for integer overflow and that sufficient space allocated in file */
    if (u > INT_MAX)                           return mcdb_make_err(m,ENOMEM);
  #if !defined(_LP64) && !defined(__LP64__)
    if (u > (UINT_MAX>>4))                     return mcdb_make_err(m,ENOMEM);
    u <<= 4;  /* 8 byte hash entries in 32-bit; x 2 for space in table */
    if (m->pos > (UINT_MAX-u))                 return mcdb_make_err(m,ENOMEM);
  #endif

    /* add "hole" for alignment; incompatible with djb cdbdump */
    /* padding to align hash tables to MCDB_PAD_ALIGN bytes (16) */
    d = (MCDB_PAD_ALIGN - (m->pos & MCDB_PAD_MASK)) & MCDB_PAD_MASK;
  #if !defined(_LP64) && !defined(__LP64__)
    if (d > (UINT_MAX-(m->pos+u)))             return mcdb_make_err(m,ENOMEM);
  #endif
    if (m->fsz < m->pos+d && !mcdb_mmap_upsize(m,m->pos+d))
                                               return mcdb_make_err(m,errno);
    if (d) memset(m->map + m->pos - m->offset, ~0, d);
    m->pos += d; /*set all bits in hole so code can detect end of data padding*/

    b = (m->pos < UINT_MAX) ? 3 : 4;
    for (i = 0; i < MCDB_SLOTS; ++i) {
        len = count[i] << 1; /* no overflow possible */
        d   = m->pos;

        /* check for sufficient space in mmap to write hash table for this slot
         * (integer overflow not possible: total size checked outside loop) */
        if (m->fsz < d+((size_t)len << b)
            && !mcdb_mmap_upsize(m, d+((size_t)len << b)))
            break;

        /* constant header (16 bytes per header slot, so multiply by 16) */
        p = header + (i << 4);  /* (i << 4) == (i * 16) */
        uint64_strpack_bigendian_aligned_macro(p,(uint64_t)d); /* hpos */
        uint32_strpack_bigendian_aligned_macro(p+8,len);       /* hslots */
        *(uint32_t *)(p+12) = 0;     /*(fill hole with 0 only for consistency)*/

        /* generate hash table for slot, writing directly to mmap */
        p = m->map + m->pos - m->offset;
        m->pos += ((uintptr_t)len << b);
        memset(p, 0, (size_t)len << b);
        if (b == 3) { /* data section ends < 4 GB; use 32-bit dpos offset */
            /* (could be make into a subroutine taking (len, p, m->head[i]) */
            /* layout in memory: 4-byte khash, 4-byte dpos */
            for (const struct mcdb_hplist *x = m->head[i]; x; x = x->next) {
                const struct mcdb_hp * restrict hp = x->hp;
                char * restrict q;
                for (uint32_t w = x->num; w; --w, ++hp) {
                    q = p+4;  /*(4 is offset of dpos)*/
                    u = (hp->h >> MCDB_SLOT_BITS) % len;
                    /* find empty entry in open hash table (dpos == 0) */
                    while (*(uint32_t *)(q+((uintptr_t)u<<3)))
                        if (++u == len)
                            u = 0;
                    q += (u<<3);
                    uint32_strpack_bigendian_aligned_macro(q-4,hp->h); /*khash*/
                    uint32_strpack_bigendian_aligned_macro(q,(uint32_t)hp->p);
                }                                                      /*dpos*/
            }
        }
        else {/*b==4*//* data section crosses 4 GB; need 64-bit dpos offset */
            /* (could be made into a subroutine taking (len, p, m->head[i]) */
            /* layout in memory: 4-byte khash, 4-byte klen, 8-byte dpos */
            for (const struct mcdb_hplist *x = m->head[i]; x; x = x->next) {
                const struct mcdb_hp * restrict hp = x->hp;
                char * restrict q;
                for (uint32_t w = x->num; w; --w, ++hp) {
                    q = p+8;  /*(8 is offset of dpos)*/
                    u = (hp->h >> MCDB_SLOT_BITS) % len;
                    /* find empty entry in open hash table (dpos == 0) */
                    while (*(uintptr_t *)(q+((uintptr_t)u<<4)))
                        if (++u == len)
                            u = 0;
                    q += (u<<4);
                    uint32_strpack_bigendian_aligned_macro(q-8,hp->h); /*khash*/
                    uint32_strpack_bigendian_aligned_macro(q-4,hp->l); /*klen*/
                    uint64_strpack_bigendian_aligned_macro(q,(uint64_t)hp->p);
                }                                                      /*dpos*/
            }
        }
    }

    u = (i == MCDB_SLOTS && mcdb_mmap_commit(m, header));
    return mcdb_make_destroy(m) | (u ? 0 : -1);
}

/* caller should call mcdb_make_destroy() upon errors from mcdb_make_*() calls
 * (already called unconditionally in mcdb_make_finish() (successful or not))
 * m->fd is not closed here since mcdb_make_start() takes open file descriptor
 * (caller should cleanup m->fd)
 */
int __attribute_noinline__
mcdb_make_destroy(struct mcdb_make * const restrict m)
{
    int rc = 0;
    if (m->map != MAP_FAILED && m->fd != -1) {
        const int errsave = errno;
        rc = munmap(m->map, m->msz);
        m->map = MAP_FAILED;
        if (errsave != 0)
            errno = errsave;
    }
    if (m->head[0] != NULL) {
        struct mcdb_hplist *n;
        struct mcdb_hplist *node;
        node = m->head[0]->pend;
        while ((n = node)) {
            node = node->pend;
            m->fn_free(n);
        }
        node = m->head[0];
        while ((n = node)) {
            node = node->next;
            m->fn_free(n);
        }
        m->head[0] = NULL;
    }
    return rc;
}
