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

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif
#ifndef _XOPEN_SOURCE /* posix_fallocate() requires _XOPEN_SOURCE 600 */
#define _XOPEN_SOURCE 600
#endif
/* gcc -std=c99 hides MAP_ANONYMOUS
 * _BSD_SOURCE or _SVID_SOURCE needed for mmap MAP_ANONYMOUS on Linux */
#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
/* _DARWIN_C_SOURCE needed for mmap MAP_ANON on MacOSX */
#define PLASMA_FEATURE_ENABLE_BSD_SOURCE_TO_DARWIN_C_SOURCE
/* large file support needed for mmap() offset,ftruncate() on mcdb > 2 GB */
#define PLASMA_FEATURE_ENABLE_LARGEFILE
/* enable extra glibc string inlines
 * (tested and improves perf for mcdb_make.c) */
#define PLASMA_FEATURE_ENABLE_GLIBC_STRING_INLINES

#include "mcdb_make.h"
#include "mcdb.h"
#include "nointr.h"
#include "uint32.h"
#include "plasma/plasma_stdtypes.h"
#include "plasma/plasma_sysconf.h"

#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>   /* posix_fallocate() */
#include <string.h>  /* memcpy() */
#include <limits.h>  /* UINT_MAX, INT_MAX */

#ifdef _AIX
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS 0x10
#endif
#endif

#if defined(__APPLE__) && defined(__MACH__)
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif
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

#define MCDB_HPLIST 250

struct mcdb_hplist {
  uint32_t num;  /* index into struct mcdb_hp hp[MCDB_HPLIST] */
  struct mcdb_hplist *next;
  struct mcdb_hplist *pend;
  struct mcdb_hp hp[MCDB_HPLIST];
};

/* routine marked to indicate unlikely branch;
 * __attribute_cold__ can be used instead of __builtin_expect() */
__attribute_cold__
__attribute_noinline__
static int
mcdb_make_err(struct mcdb_make * const restrict m, int errnum)
{
    if (m != NULL) mcdb_make_destroy(m);
    errno = errnum;
    return -1;
}

__attribute_noinline__
__attribute_nonnull__
__attribute_warn_unused_result__
static bool
mcdb_hplist_alloc(struct mcdb_make * const restrict m);

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

#if !defined(__GLIBC__)
/* emulate posix_fallocate() with statvfs() and pwrite(); all POSIX.1-2001 std*/
/* (_XOPEN_SOURCE 600 (posix_fallocate), _XOPEN_SOURCE 500 (pwrite)) */
#ifdef __sun  /* bug in statvfs.h; copy define to avoid using __EXTENSIONS__ */
#define FSTYPSZ 16
#endif
#include <sys/statvfs.h>
__attribute_noinline__
__attribute_warn_unused_result__
static int
mcdb_make_fallocate(const int fd, off_t offset, off_t len);

static int
mcdb_make_fallocate(const int fd, off_t offset, off_t len)
{
    /* Assumptions:
     * - off_t is sizeof(long) or else is sizeof(long long) w/ 32-bit +largefile
     * - (struct statvfs).f_bsize is power of 2
     * Contract: modifies file only within given range [offset,offset+len)
     * Note: most efficient use is allocating file with multiple of block size*/
    struct statvfs stvfs;
    struct stat st;
    off_t st_size;
    int buf;

    if (offset < 0 || len <= 0)
        return EINVAL;

    if (fstatvfs(fd, &stvfs) != 0 || fstat(fd, &st) != 0)
        return errno;

  #if (defined(_FILE_OFFSET_BITS) && _FILE_OFFSET_BITS-0 == 64)  \
   || defined(_LARGEFILE_SOURCE) || defined(_LARGEFILE64_SOURCE) \
   || defined(_LARGE_FILES)
    if (len <= (long long)(LLONG_MAX-(stvfs.f_bsize-1))
        && ((len+stvfs.f_bsize-1) & ~(stvfs.f_bsize-1))
            <= (unsigned long long)(LLONG_MAX-offset))
  #else
    if (len <= (long long)(LONG_MAX-(stvfs.f_bsize-1))
        && ((len+stvfs.f_bsize-1) & ~(stvfs.f_bsize-1))
            <= (unsigned long)(LONG_MAX-offset))
  #endif
        len += offset;
    else
        return EFBIG;  /* integer overflow */

    st_size = st.st_size < len ? st.st_size : len;
    while (offset < st_size
           && pread(fd,&buf,1,offset) != -1 && pwrite(fd,&buf,1,offset) != -1)
        offset += stvfs.f_bsize;

    if (offset < st_size || (st.st_size != len && ftruncate(fd, len) != 0))
        return errno;

    buf = 0;

    /* one-off to not overwrite part of partial block before end-of-file */
    if (offset < len && offset == st_size && (offset & (stvfs.f_bsize-1))) {
        if (pwrite(fd,&buf,1,offset) != -1)
            offset += stvfs.f_bsize;
        else
            return errno;
    }

    offset &= ~(stvfs.f_bsize-1);/*set offset to beginning of filesystem block*/

    while (offset < len && pwrite(fd,&buf,1,offset) != -1)
        offset += stvfs.f_bsize;

    return offset >= len ? 0 : errno;
}
#endif

__attribute_nonnull__
__attribute_warn_unused_result__
static bool  inline
mcdb_mmap_commit(struct mcdb_make * const restrict m,
                 char header[MCDB_HEADER_SZ]);

static bool  inline
mcdb_mmap_commit(struct mcdb_make * const restrict m,
                 char header[MCDB_HEADER_SZ])
{
    if (m->fd == -1) { /*(m->fd == -1 during large mcdb size tests)*/
        if (m->offset == 0)
            memcpy(m->map, header, MCDB_HEADER_SZ);
        return true;
    }

    return (
          #ifndef __CYGWIN__  /* (see comments in mcdb_make_destroy()) */
                0 == nointr_ftruncate(m->fd, (off_t)m->pos)
            && 
          #endif
                (0== m->pos - m->offset ||/*(avoid 0-sized msync; portability)*/
                 0== msync(m->map, m->pos - m->offset, MS_ASYNC))
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
     * OS crashes, then the updated mcdb can be corrupted. */
}

__attribute_noinline__
__attribute_nonnull__
__attribute_warn_unused_result__
static bool
mcdb_mmap_upsize(struct mcdb_make * const restrict m, const size_t sz,
                 const bool sequential);

__attribute_noinline__
static bool
mcdb_mmap_upsize(struct mcdb_make * const restrict m, const size_t sz,
                 const bool sequential)
{
    const size_t offset = m->pos & m->pgalign; /* mmap offset must be aligned */
    size_t msz;

    /*(caller should check size and not call upsize unless resize needed)*/
    /*(avoid overhead of less-frequently called subroutine; marked noinline)*/

  #if !defined(_LP64) && !defined(__LP64__)  /* (no 4 GB limit in 64-bit) */
    /* limit max size of mcdb to (4 GB - pagesize) */
    if (sz > (UINT_MAX & m->pgalign)) { errno = EOVERFLOW; return false; }
  #endif

    msz = (MCDB_MMAP_SZ > sz - offset)
      ? MCDB_MMAP_SZ
      : (sz - offset + ~m->pgalign) & m->pgalign;
  #if !defined(_LP64) && !defined(__LP64__)  /* (no 4 GB limit in 64-bit) */
    if (offset > (UINT_MAX & m->pgalign) - msz)
        msz = (UINT_MAX & m->pgalign) - offset;
  #endif

    /* increase file size by at least msz (prefer multiple of disk block size)
     * (reduce to MCDB_MMAP_SZ for 1st (and maybe 2nd) mmap for small mcdb) */
    if (m->fd != -1 && m->fsz < offset + msz) {
        m->fsz = (m->offset != 0)
          ? ((offset + msz + (MCDB_BLOCK_SZ-1)) & ~(size_t)(MCDB_BLOCK_SZ-1))
          : ((offset + msz + (MCDB_MMAP_SZ-1))  & ~(size_t)(MCDB_MMAP_SZ-1));
      #if defined(__GLIBC__)/* glibc emulates if not natively supported by fs */
        if ((errno = posix_fallocate(m->fd, (off_t)m->osz,
                                     (off_t)(m->fsz-m->osz))) == 0)
      #elif defined(__SunOS_5_11)/*not sure about Solaris 11; not tested by me*/
        /* disabled for defined(_AIX) since mcdb_make_fallocate() is faster
         * and because posix_fallocate() in 32-bit can result in SIGSEGV.
         * Observed on AIX TL6 SP3: posix_fallocate() fails on initial resize
         * and mcdb_make_fallocate() succeeds, but then posix_fallocate()
         * returns 0 on second call to extend file, but later access invalid.
         * Prior issues others had with posix_fallocate() on AIX:
         * http://thr3ads.net/dovecot/2009/07/1089409-AIX-and-posix_fallocate
         * https://www-304.ibm.com/support/docview.wss?uid=isg1IZ46957 */
        /*defined(_AIX)*//*AIX errno=ENOTSUP if not natively supported by fs*/
        if ((errno = posix_fallocate(m->fd, (off_t)m->osz,
                                     (off_t)(m->fsz-m->osz))) == 0
            || (errno != ENOSPC
                && (errno = mcdb_make_fallocate(m->fd, (off_t)m->osz,
                                                (off_t)(m->fsz-m->osz))) == 0))
      #else /*emulate posix_fallocate() on earlier __sun, on __hpux and others*/
        if ((errno = mcdb_make_fallocate(m->fd, (off_t)m->osz,
                                         (off_t)(m->fsz-m->osz))) == 0)
      #endif
            m->osz = m->fsz;
        else
            return false;
    }

    /* flush and munmap prior mmap */
    if (m->map != MAP_FAILED) {
        if ((  -1 == m->fd     /*(m->fd==-1 during some large mcdb size tests)*/
             || 0 == m->pos - m->offset   /*(avoid 0-sized msync; portability)*/
             || 0 == msync(m->map, m->pos - m->offset, MS_ASYNC))
            &&  0 == munmap(m->map, m->msz))
            m->map = MAP_FAILED;
        else
            return false;
    }

    /* (compilation with large file support enables off_t max > 2 GB in cast) */
    m->map = (m->fd != -1) /* (m->fd == -1 during some large mcdb size tests) */
      ? (char *)mmap(0, msz, PROT_WRITE, MAP_SHARED, m->fd, (off_t)offset)
      : (char *)mmap(0, msz, PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (m->map == MAP_FAILED) return false;
    m->offset = offset;
    m->msz = msz;
    if (sequential)
        posix_madvise(m->map, msz, POSIX_MADV_SEQUENTIAL);
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
    if (m->map == MAP_FAILED && m->fd != -1)  return mcdb_make_err(NULL,EPERM);
    if (m->hp.l== ~0 && !mcdb_hplist_alloc(m))return mcdb_make_err(NULL,errno);
    m->hp.p = pos;
    m->hp.h = m->hash_init;
    if (keylen>INT_MAX-8 || datalen>INT_MAX-8)return mcdb_make_err(NULL,EINVAL);
    m->hp.l = (uint32_t)keylen;
  #if !defined(_LP64) && !defined(__LP64__)  /* (no 4 GB limit in 64-bit) */
    if (pos > UINT_MAX-len)                   return mcdb_make_err(NULL,ENOMEM);
  #endif
    if (m->offset+m->msz < pos+len && !mcdb_mmap_upsize(m, pos+len, true))
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
    m->hp.h = (m->hash_fn == uint32_hash_djb)
      ? uint32_hash_djb(m->hp.h, buf, len)
      : m->hash_fn(m->hp.h, buf, len);
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
{   /* e.g. discard in-progress incremental addbuf, or immediately prior add */
    m->pos = m->hp.p;  /* addrevert can be used up until next add or addbegin */
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
    m->hash_init = UINT32_HASH_DJB_INIT;
    m->hash_fn   = uint32_hash_djb;
    m->fsz       = 0;
    m->osz       = 0;
    m->msz       = 0;
    m->hp.p      = MCDB_HEADER_SZ;
    m->hp.h      = 0;
    m->hp.l      = 0;
    m->fd        = fd;
    m->fn_malloc = fn_malloc;
    m->fn_free   = fn_free;
    m->pgalign   = ~( ((size_t)plasma_sysconf_pagesize()) - 1u );
    m->head[0]   = (struct mcdb_hplist *)
                   fn_malloc(sizeof(struct mcdb_hplist) * MCDB_SLOTS);
    memset(m->count, 0, MCDB_SLOTS * sizeof(uint32_t));
    /* do not modify m->fname, m->fntmp, m->st_mode; may already have been set*/
    /* (defer mcdb_mmap_upsize() if fd==-1 to allow caller to set custom map) */
    if (m->head[0] != NULL
        && (fd == -1 || mcdb_mmap_upsize(m, MCDB_MMAP_SZ, true))) {
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
    if (m->pos > ((size_t)UINT_MAX-u))         return mcdb_make_err(m,ENOMEM);
  #endif

    /* add "hole" for alignment; incompatible with djb cdbdump */
    /* padding to align hash tables to MCDB_PAD_ALIGN bytes (16) */
    d = (MCDB_PAD_ALIGN - (m->pos & MCDB_PAD_MASK)) & MCDB_PAD_MASK;
  #if !defined(_LP64) && !defined(__LP64__)
    if (d > (UINT_MAX-(m->pos+u)))             return mcdb_make_err(m,ENOMEM);
  #endif
    if (m->offset+m->msz < m->pos+d && !mcdb_mmap_upsize(m, m->pos+d, false))
                                               return mcdb_make_err(m,errno);
    if (d) memset(m->map + m->pos - m->offset, ~0, d);
    m->pos += d; /*set all bits in hole so code can detect end of data padding*/

    /* undo POSIX_MADV_SEQUENTIAL advice to avoid crash on Solaris
     * (madvise is supposed to be advice, not promise; Solaris crash is bug) */
    posix_madvise(m->map, m->msz, POSIX_MADV_NORMAL);

    b = (m->pos < UINT_MAX) ? 3u : 4u;
    for (i = 0; i < MCDB_SLOTS; ++i) {
        len = count[i] << 1;
        d   = m->pos;

        /* mmap sufficient space into which to write hash table for this slot */
        if (m->offset+m->msz < d+((uintptr_t)len << b)
            && !mcdb_mmap_upsize(m, d+((uintptr_t)len << b), false))
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
            /* (could be made into a subroutine taking (len, p, m->head[i]) */
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

    u = (uint32_t)(i == MCDB_SLOTS && mcdb_mmap_commit(m, header));
    return (u ? 0 : -1) | mcdb_make_destroy(m);
}

/* caller should call mcdb_make_destroy() upon errors from mcdb_make_*() calls
 * (already called unconditionally in mcdb_make_finish() (successful or not))
 * m->fd is not closed here since mcdb_make_start() takes open file descriptor
 * (caller should cleanup m->fd)
 */
__attribute_noinline__
int
mcdb_make_destroy(struct mcdb_make * const restrict m)
{
    int rc = 0;
    if (m->map != MAP_FAILED && m->fd != -1) {
        const int errsave = errno;
        rc = munmap(m->map, m->msz);
        m->map = MAP_FAILED;
        if (errsave != 0)
            errno = errsave;
      #ifdef __CYGWIN__
        /* cygwin: defer ftruncate from mcdb_mmap_commit() to after munmap() */
        /* (ftruncate appears to fails EPERM if offset overlaps with mmap) */
        else
            rc |= nointr_ftruncate(m->fd, (off_t)m->pos);
      #endif
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


/* alias symbols with hidden visibility for use in DSO linking static mcdb.o
 * (Reference: "How to Write Shared Libraries", by Ulrich Drepper)
 * (optimization)
 * The aliases below are not a complete set of mcdb_make symbols */
#ifdef PLASMA_ATTR_ALIAS
HIDDEN extern __typeof (mcdb_make_add)
                        mcdb_make_add_h
  __attribute_alias__ ("mcdb_make_add");
HIDDEN extern __typeof (mcdb_make_addbegin)
                        mcdb_make_addbegin_h
  __attribute_alias__ ("mcdb_make_addbegin");
HIDDEN extern __typeof (mcdb_make_addbuf_data)
                        mcdb_make_addbuf_data_h
  __attribute_alias__ ("mcdb_make_addbuf_data");
HIDDEN extern __typeof (mcdb_make_addbuf_key)
                        mcdb_make_addbuf_key_h
  __attribute_alias__ ("mcdb_make_addbuf_key");
HIDDEN extern __typeof (mcdb_make_addend)
                        mcdb_make_addend_h
  __attribute_alias__ ("mcdb_make_addend");
#endif
