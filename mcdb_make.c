/* Copyright 2010  Glue Logic LLC
 * License: GPLv3
 */

#define _POSIX_C_SOURCE 200112L
/* _GNU_SOURCE needed for mremap() */
#define _GNU_SOURCE
/* large file support needed for ftruncate() on cdb larger than 2 GB */
#define _FILE_OFFSET_BITS 64
/* GPS: TODO add large file support for other operating systems */

#include "mcdb_make.h"
#include "mcdb.h"
#include "mcdb_uint32.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>  /* malloc() */
#include <string.h>  /* memcpy() */

static struct mcdb_hplist *  __attribute__((noinline))
mcdb_hplist_alloc(struct mcdb_make * const restrict m)
{
    struct mcdb_hplist * const restrict head =
      (struct mcdb_hplist *) m->fn_malloc(sizeof(struct mcdb_hplist));
    if (!head) return NULL;
    head->num  = 0;
    head->next = m->head;
    return (m->head = head);
}

static bool  __attribute__((noinline))
mcdb_ftruncate_retry(const int fd, const size_t sz)
{
    int rv;
    /* (compilation with large file support enables off_t max > 2 GB) */
    while ((rv = ftruncate(fd, (off_t)sz)) != 0 && errno == EINTR)
        ;
    return (rv == 0);
}

static bool  __attribute__((noinline))
mcdb_mmap_resize(struct mcdb_make * const restrict m, const size_t sz)
{
    size_t fsz = (m->fsz < sz ? m->fsz : sz);
    void *x;

    /* assert(sz <= UINT_MAX); *//* caller must check or else infinite loop */

    while (fsz < sz)
        fsz = (fsz < (UINT_MAX>>2) ? (fsz << 2) : UINT_MAX);

    if (!mcdb_ftruncate_retry(m->fd, fsz)) return false;

  #ifdef __GLIBC__

    x = mremap(m->map, m->fsz, fsz, MREMAP_MAYMOVE); /*fsz page-sized multiple*/
    if (x == MAP_FAILED) return false;

  #else

  #if defined(_POSIX_MAPPED_FILES) && _POSIX_MAPPED_FILES-0 \
   && defined(_POSIX_SYNCHRONIZED_IO) && _POSIX_SYNCHRONIZED_IO-0
    if (msync(m->map, m->pos, MS_SYNC) != 0) return false;
  #endif
    if (munmap(m->map, m->fsz) != 0) return false;
    x = mmap(0,fsz,PROT_WRITE,MAP_SHARED,fd,0);      /*fsz page-sized multiple*/
    if (x == MAP_FAILED) return false;
    posix_madvise(x, fsz, POSIX_MADV_SEQUENTIAL);

  #endif

    m->map = (char *)x;
    m->fsz = fsz;
    return true;
}

static bool  inline
mcdb_mmap_commit(struct mcdb_make * const restrict m)
{
    if (!mcdb_ftruncate_retry(m->fd, m->pos)) return false;

  #if defined(_POSIX_MAPPED_FILES) && _POSIX_MAPPED_FILES-0 \
   && defined(_POSIX_SYNCHRONIZED_IO) && _POSIX_SYNCHRONIZED_IO-0
    if (msync(m->map, m->pos, MS_SYNC) != 0) return false;
  #endif

    return true;
}

void  inline
mcdb_make_addbuf_data(struct mcdb_make * const restrict m,
                      const char * const restrict buf, size_t len)
{
    /* len validated in mcdb_make_addbegin(); passing any other len is wrong,
     * unless the len is shorter from partial contents of buf. */
    memcpy(m->map + m->pos, buf, len);
    m->pos += len;
}

void  inline
mcdb_make_addbuf_key(struct mcdb_make * const restrict m,
                     const char * const restrict buf, size_t len)
{
    /* len validated in mcdb_make_addbegin(); passing any other len is wrong,
     * unless the len is shorter from partial contents of buf. */
    uint32_t * const h = &m->head->hp[m->head->num].h;
    *h = mcdb_hash(*h, buf, len);
    mcdb_make_addbuf_data(m, buf, len);
}

int  __attribute__((noinline))
mcdb_make_addbegin(struct mcdb_make * const restrict m,
                   const size_t keylen, const size_t datalen)
{
    /* validate/allocate space for next key/data pair */
    char * const p = m->map + m->pos;
    const size_t pos = m->pos;
    const size_t len = 8 + keylen + datalen;/* arbitrary ~2 GB limit for lens */
    struct mcdb_hplist * const head =
      m->head->num < MCDB_HPLIST ? m->head : mcdb_hplist_alloc(m);
    if (head == NULL) return -1;
    head->hp[head->num].h = MCDB_HASH_INIT;
    head->hp[head->num].p = (uint32_t)pos;  /* arbitrary ~2 GB limit for lens */
    if (keylen > INT_MAX-8 || datalen > INT_MAX-8) { errno=EINVAL; return -1; }
    if (pos > UINT_MAX-len)                        { errno=ENOMEM; return -1; }
    if (m->fsz < pos+len && !mcdb_mmap_resize(m,pos+len))          return -1;
    mcdb_uint32_pack_macro(p,keylen);
    mcdb_uint32_pack_macro(p+4,datalen);
    m->pos += 8;
    return 0;
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
    /* MCDB_INITIAL_SZ is page-sized multiple for mmap convienence */
    if (!mcdb_ftruncate_retry(fd, MCDB_INITIAL_SZ)) return -1;
    m->map = mmap(0, MCDB_INITIAL_SZ, PROT_WRITE, MAP_SHARED, fd, 0);
    if (m->map == MAP_FAILED) return -1;
    posix_madvise(m->map, MCDB_INITIAL_SZ, POSIX_MADV_SEQUENTIAL);

    m->pos = MCDB_HEADER_SZ;
    m->fsz = MCDB_INITIAL_SZ;
    m->head = NULL;
    m->fd = fd;
    m->fn_malloc = fn_malloc;
    m->fn_free = fn_free;
    if (mcdb_hplist_alloc(m) != NULL) {  /* init to skip NULL check every add */
        m->head->hp[m->head->num].p = (uint32_t)m->pos;/*mcdb_make_addrevert()*/
        return 0;
    }
    else {
        mcdb_make_destroy(m);
        return -1;
    }
}

int mcdb_make_finish(struct mcdb_make * const restrict m)
{
    uint32_t u;
    uint32_t i;
    uint32_t d;
    uint32_t len;
    uint32_t cnt;
    uint32_t where;
    struct mcdb_hp *hash;
    struct mcdb_hp *split;
    struct mcdb_hp *hp;
    struct mcdb_hplist *x;
    char *p;
    uint32_t count[MCDB_SLOTS];
    uint32_t start[MCDB_SLOTS];
    char final[MCDB_HEADER_SZ];

    memset(count, '\0', sizeof(count));

    cnt = 0; /* num entries */
    for (x = m->head; x; x = x->next) {
        cnt += (u = x->num);
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
    if (cnt > (UINT_MAX>>4) || len > INT_MAX)  { errno = ENOMEM; return -1; }
    u = cnt << 4; /* multiply by 2 and then by 8 (for 8 chars) */
    if (m->pos <= (UINT_MAX-(u)))              { errno = ENOMEM; return -1; }
    u += m->pos;
    if (m->fsz < u && !mcdb_mmap_resize(m,u)) return -1;
    len += cnt; /* no overflow possible up to now */
    if (len > UINT_MAX/sizeof(struct mcdb_hp)) { errno = ENOMEM; return -1; }

    split = (struct mcdb_hp *) m->fn_malloc(len * sizeof(struct mcdb_hp));
    if (!split) return -1;
    hash = split + cnt;

    u = 0;
    for (i = 0; i < MCDB_SLOTS; ++i) {
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

        p = final + (i << 3);  /* (i << 3) == (i * 8) */
        u = m->pos;
        mcdb_uint32_pack_macro(p,u);
        len = cnt << 1; /* no overflow possible */
        mcdb_uint32_pack_macro(p+4,len);

        for (u = 0; u < len; ++u)
            hash[u].h = hash[u].p = 0;

        hp = split + start[i];
        for (u = 0; u < cnt; ++u) {
            where = (hp->h >> 8) % len;
            while (hash[where].p)
                if (++where == len)
                    where = 0;
            hash[where] = *hp++;
        }

        for (u = 0; u < len; ++u) {
            /* write directly to map; allocated space checked above outer loop*/
            p = m->map + m->pos;
            m->pos += 8;
            d = hash[u].h;
            mcdb_uint32_pack_macro(p,d);
            d = hash[u].p;
            mcdb_uint32_pack_macro(p+4,d);
        }
    }
    m->fn_free(split);

    /* write constant header directly to map
     * (space was allocated for header when mmap created; can not fail) */
    memcpy(m->map, final, sizeof(final));

    return mcdb_mmap_commit(m) ? mcdb_make_destroy(m) : -1;
}

/* caller should call mcdb_make_destroy() upon errors from mcdb_make_*() calls
 * (already called when cdb_make_finish() is successful)
 * m->fd is not closed here since mcdb_make_start() takes open file descriptor
 * (caller should cleanup m->fd)
 */
int __attribute__((noinline))
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
    return (map != MAP_FAILED) ? munmap(map, m->fsz) : 0;
}
