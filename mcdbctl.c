/*
 * mcdbctl - mcdb command line tool: make, get, dump, stats
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
#ifndef _XOPEN_SOURCE /* IOV_MAX */
#define _XOPEN_SOURCE 500
#endif
/* large file support needed for open() input file > 2 GB */
#if defined(_AIX)
#ifndef _LARGE_FILES
#define _LARGE_FILES
#endif
#else /*#elif defined(__linux__) || defined(__sun) || defined(__hpux)*/
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

#include "mcdb.h"
#include "mcdb_makefmt.h"
#include "mcdb_makefn.h"
#include "mcdb_error.h"
#include "nointr.h"
#include "uint32.h"

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>   /* open(), O_RDONLY */
#include <stdbool.h> /* bool */
#include <stdio.h>   /* printf(), snprintf(), IOV_MAX */
#include <stdlib.h>  /* malloc(), free(), EXIT_SUCCESS */
#include <string.h>  /* strlen() */
#include <unistd.h>  /* STDIN_FILENO, STDOUT_FILENO */
#include <sys/uio.h> /* writev() */
#include <libgen.h>  /* basename() */
#include <limits.h>  /* SSIZE_MAX */


/* macros to hint to release memory pages every 32 MB (1u << 25) */
#define mcdb_madv_initmark(ptr) ((ptr) + (1u << 25))
#define mcdb_madv_dontneed(ptr, mark)                                          \
  do {                                                                         \
    if (__builtin_expect( ((ptr) >= (mark)), 0)) {                             \
      do {                                                                     \
        posix_madvise((mark) - (1u << 25), (1u << 25), POSIX_MADV_DONTNEED);   \
        (mark) += (1u << 25);                                                  \
      } while (__builtin_expect( ((ptr) >= (mark)), 0));                       \
    }                                                                          \
  } while (0)

static bool
writev_loop(const int fd, struct iovec * restrict iov, int iovcnt, ssize_t sz)
  __attribute_nonnull__  __attribute_warn_unused_result__;
static bool
writev_loop(const int fd, struct iovec * restrict iov, int iovcnt, ssize_t sz)
{
    /* Note: unlike writev(), this routine might modify the iovecs */
    ssize_t len;
    while (iovcnt && (len = writev(fd, iov, iovcnt)) != -1) {
        if ((sz -= len) == 0)
            return true;
        while (len != 0) {
            if (len >= iov->iov_len) {
                len -= iov->iov_len;
                --iovcnt;
                ++iov;
            }
            else {
                iov->iov_len -= len;
                iov->iov_base = ((char *)(iov->iov_base)) + len;
                break; /* len = 0; */
            }
        }
    }
    return (iovcnt == 0);
}

/* read and dump data section of mcdb */
static int
mcdbctl_dump(struct mcdb * const restrict m)
  __attribute_nonnull__  __attribute_warn_unused_result__;
static int
mcdbctl_dump(struct mcdb * const restrict m)
{
    struct mcdb_iter iter;
    uint32_t klen;
    uint32_t dlen;
    unsigned char *mark = mcdb_madv_initmark(m->map->ptr);
    int    iovcnt = 0;
    size_t iovlen = 0;
    size_t buflen = 0;
    enum { MCDB_IOVNUM = IOV_MAX };/* assert(IOV_MAX >= 7) */
    struct iovec iov[MCDB_IOVNUM]; /* each db entry uses 7 iovecs */
    char buf[(MCDB_IOVNUM * 3)];   /* each db entry might use (2) * 10 chars */
      /* oversized buffer since all num strings must add up to less than max */

    mcdb_iter_init(&iter, m);
    posix_madvise(iter.ptr, (size_t)(iter.eod - iter.ptr),
                  POSIX_MADV_SEQUENTIAL | POSIX_MADV_WILLNEED);
    while (mcdb_iter(&iter)) {

        mcdb_madv_dontneed(iter.ptr, mark);  /* hint to release memory pages */

        klen = mcdb_iter_keylen(&iter);
        dlen = mcdb_iter_datalen(&iter);

        /* avoid printf("%.*s\n",...) due to mcdb arbitrary binary data */
        /* klen, dlen each limited to (2GB - 8); space for extra tokens exists*/
        if (iovlen + klen + 5 > SSIZE_MAX || iovcnt + 7 >= MCDB_IOVNUM) {
            if (!writev_loop(STDOUT_FILENO, iov, iovcnt, (ssize_t)iovlen))
                return MCDB_ERROR_WRITE;
            iovcnt = 0;
            iovlen = 0;
            buflen = 0;
        }

        iov[iovcnt].iov_base = "+";
        iov[iovcnt].iov_len  = 1;
        ++iovcnt;

        iov[iovcnt].iov_base = buf+buflen;
        buflen += iov[iovcnt].iov_len = snprintf(buf+buflen, 11, "%u", klen);
        ++iovcnt;

        iov[iovcnt].iov_base = ",";
        iov[iovcnt].iov_len  = 1;
        ++iovcnt;

        iov[iovcnt].iov_base = buf+buflen;
        buflen += iov[iovcnt].iov_len = snprintf(buf+buflen, 11, "%u", dlen);
        ++iovcnt;

        iov[iovcnt].iov_base = ":";
        iov[iovcnt].iov_len  = 1;
        ++iovcnt;

        iov[iovcnt].iov_base = mcdb_iter_keyptr(&iter);
        iov[iovcnt].iov_len  = klen;
        ++iovcnt;

        iov[iovcnt].iov_base = "->";
        iov[iovcnt].iov_len  = 2;
        ++iovcnt;

        iovlen += klen + 5;

        if (iovlen + dlen + 1 > SSIZE_MAX) {
            if (!writev_loop(STDOUT_FILENO, iov, iovcnt, (ssize_t)iovlen))
                return MCDB_ERROR_WRITE;
            iovcnt = 0;
            iovlen = 0;
            buflen = 0;
        }

        iov[iovcnt].iov_base = mcdb_iter_dataptr(&iter);
        iov[iovcnt].iov_len  = dlen;
        ++iovcnt;

        iov[iovcnt].iov_base = "\n";
        iov[iovcnt].iov_len  = 1;
        ++iovcnt;

        iovlen += dlen + 1;

    }

    /* write out iovecs and append blank line ("\n") to indicate end of data */
    return (writev_loop(STDOUT_FILENO, iov, iovcnt, (ssize_t)iovlen)
            && write(STDOUT_FILENO, "\n", 1) == 1)
      ? EXIT_SUCCESS
      : MCDB_ERROR_WRITE;
}

/* Note: mcdbctl_stats() is equivalent test to pass/fail of djb cdbtest */
static int
mcdbctl_stats(struct mcdb * const restrict m)
  __attribute_nonnull__  __attribute_warn_unused_result__;
static int
mcdbctl_stats(struct mcdb * const restrict m)
{
    struct mcdb_iter iter;
    uintptr_t iter_dpos;
    char *k;
    unsigned char *mark = mcdb_madv_initmark(m->map->ptr + MCDB_HEADER_SZ);
    unsigned long nrec = 0;
    unsigned long numd[11] = { 0,0,0,0,0,0,0,0,0,0,0 };
    unsigned int rv;
    bool rc;
    posix_madvise(m->map->ptr, m->map->size,
                  POSIX_MADV_SEQUENTIAL | POSIX_MADV_WILLNEED);
    if (!mcdb_validate_slots(m))
        return MCDB_ERROR_READFORMAT;
    mcdb_iter_init(&iter, m);
    while (mcdb_iter(&iter)) {
        /* Search for key,data and track number of tries before found.
         * Technically, passing m (which contains m->map->ptr) and an
         * alias into the map (k) as key is in violation of C99 restrict
         * pointers, but is inconsequential since it is all read-only */
        k = (char *)mcdb_iter_keyptr(&iter);
        iter_dpos = mcdb_iter_datapos(&iter);
        if ((rc = mcdb_findstart(m, k, mcdb_iter_keylen(&iter)))) {
            do { rc = mcdb_findnext(m, k, mcdb_iter_keylen(&iter));
            } while (rc && mcdb_datapos(m) != iter_dpos);
        }
        if (!rc) return MCDB_ERROR_READFORMAT;
        ++numd[ ((m->loop < 11) ? m->loop - 1 : 10) ];
        ++nrec;
        mcdb_madv_dontneed(iter.ptr, mark);  /* hint to release memory pages */
    }
    printf("records %lu\n", nrec);
    for (rv = 0; rv < 10; ++rv)
        printf("d%d      %lu\n", rv, numd[rv]);
    printf(">9      %lu\n", numd[10]);
    fflush(stdout);
    return EXIT_SUCCESS;
}

static int
mcdbctl_getseq(struct mcdb * const restrict m,
               const char * const restrict key, unsigned long seq)
  __attribute_nonnull__  __attribute_warn_unused_result__;
static int
mcdbctl_getseq(struct mcdb * const restrict m,
               const char * const restrict key, unsigned long seq)
{
    const size_t klen = strlen(key);
    struct iovec iov[2];
    if (mcdb_findstart(m, key, klen)) {
        bool rc;
        while ((rc = mcdb_findnext(m, key, klen)) && seq--)
            ;
        if (rc) {
            /* avoid printf("%.*s\n",...) due to mcdb arbitrary binary data */
            iov[0].iov_base = mcdb_dataptr(m);
            iov[0].iov_len  = mcdb_datalen(m);
            iov[1].iov_base = "\n";
            iov[1].iov_len  = 1;
            return writev_loop(STDOUT_FILENO,iov,2,(ssize_t)(iov[0].iov_len+1))
              ? EXIT_SUCCESS
              : MCDB_ERROR_WRITE;
        }
    }
    return EXIT_FAILURE;
}

static int
mcdbctl_query(const int argc, char ** restrict argv)
  __attribute_nonnull__  __attribute_warn_unused_result__;
static int
mcdbctl_query(const int argc, char ** restrict argv)
{
    struct mcdb m;
    struct mcdb_mmap map;
    int rv;
    int fd;
    unsigned long seq = 0;
    enum { MCDBCTL_BAD_QUERY_TYPE, MCDBCTL_GET, MCDBCTL_DUMP, MCDBCTL_STATS }
      query_type = MCDBCTL_BAD_QUERY_TYPE;

    /* validate args  (query type string == argv[1]) */
    if (argc > 3 && 0 == strcmp(argv[1], "get")) {
        if (argc == 5) {
            char *endptr;
            seq = strtoul(argv[4], &endptr, 10);
            if (seq != ULONG_MAX && argv[4] != endptr && *endptr == '\0')
                query_type = MCDBCTL_GET;
        }
        else if (argc == 4)
            query_type = MCDBCTL_GET;
    }
    else if (argc == 3) {
        if (0 == strcmp(argv[1], "dump"))
            query_type = MCDBCTL_DUMP;
        else if (0 == strcmp(argv[1], "stats"))
            query_type = MCDBCTL_STATS;
    }

    if (query_type == MCDBCTL_BAD_QUERY_TYPE)
        return MCDB_ERROR_USAGE;

    /* open mcdb */
    fd = nointr_open(argv[2], O_RDONLY, 0);  /* fname = argv[2] */
    if (fd == -1) return MCDB_ERROR_READ;
    memset(&map, '\0', sizeof(map));  /*(init fn_free, fname)*/
    rv = mcdb_mmap_init(&map, fd);
    (void) nointr_close(fd);
    if (!rv) return MCDB_ERROR_READ;
    memset(&m, '\0', sizeof(m));      /*(not strictly necessary)*/
    m.map = &map;

    /* run query */
    switch (query_type) {
      case MCDBCTL_GET:
        rv = mcdbctl_getseq(&m, argv[3], seq);  /* key = argv[3] */
        if (rv != EXIT_SUCCESS)
            exit(100); /* not found: exit nonzero without errmsg */
        break;
      case MCDBCTL_DUMP:
        rv = mcdbctl_dump(&m);
        break;
      case MCDBCTL_STATS:
        rv = mcdbctl_stats(&m);
        break;
      default: /* should not happen */
        rv = MCDB_ERROR_USAGE;
        break;
    }

    mcdb_mmap_free(&map);
    return rv;
}

static int
mcdbctl_make(const int argc, char ** const restrict argv)
  __attribute_nonnull__  __attribute_warn_unused_result__;
static int
mcdbctl_make(const int argc, char ** const restrict argv)
{
    /* assert(argc == 4); */                   /* must be checked by caller */
    /* assert(0 == strcmp(argv[1], "make")); *//* must be checked by caller */
    enum { BUFSZ = 65536 }; /* 64 KB buffer size */
    char * restrict buf = NULL;
    char * const fname = argv[2];
    char * const input = argv[3];
    const int rv = (input[0] == '-' && input[1] == '\0')
      ? ((buf = malloc(BUFSZ)) != NULL)
        ? mcdb_makefmt_fdintofile(STDIN_FILENO, buf, BUFSZ, fname, malloc, free)
        : MCDB_ERROR_MALLOC
      : mcdb_makefmt_fileintofile(input, fname, malloc, free);
    free(buf);
    return rv;
}

static int
mcdbctl_has_unique_keys(struct mcdb * const restrict m)
  __attribute_nonnull__  __attribute_warn_unused_result__;
static int
mcdbctl_has_unique_keys(struct mcdb * const restrict m)
{
    struct mcdb_iter iter;
    char *k;
    unsigned char *mark = mcdb_madv_initmark(m->map->ptr + MCDB_HEADER_SZ);
    posix_madvise(m->map->ptr, m->map->size,
                  POSIX_MADV_SEQUENTIAL | POSIX_MADV_WILLNEED);
    if (!mcdb_validate_slots(m))
        return MCDB_ERROR_READFORMAT;
    mcdb_iter_init(&iter, m);
    while (mcdb_iter(&iter)) {
        /* Technically, passing m (which contains m->map->ptr) and an
         * alias into the map (k) as key is in violation of C99 restrict
         * pointers, but is inconsequential since it is all read-only */
        k = (char *)mcdb_iter_keyptr(&iter);
        if (mcdb_find(m, k, mcdb_iter_keylen(&iter))) {
            if (mcdb_findnext(m, k, mcdb_iter_keylen(&iter)))
                return false; /*keys not unique; bail on first dup encountered*/
        }
        else
            return MCDB_ERROR_READFORMAT;
        mcdb_madv_dontneed(iter.ptr, mark);  /* hint to release memory pages */
    }
    return true;  /*keys are unique in mcdb*/
}

static int
mcdbctl_make_unique_keys(struct mcdb * const restrict m, const bool first)
  __attribute_nonnull__  __attribute_warn_unused_result__;
static int
mcdbctl_make_unique_keys(struct mcdb * const restrict m, const bool first)
{
    struct mcdb_iter iter;
    struct mcdb_make mk;
    char *k, *data;
    unsigned char *mark = mcdb_madv_initmark(m->map->ptr + MCDB_HEADER_SZ);
    uint32_t dlen;
    int rv = EXIT_SUCCESS;
    posix_madvise(m->map->ptr, m->map->size,
                  POSIX_MADV_SEQUENTIAL | POSIX_MADV_WILLNEED);
    if (!mcdb_validate_slots(m))
        return MCDB_ERROR_READFORMAT;
    if (mcdb_makefn_start(&mk, m->map->fname, malloc, free) == 0
        && mcdb_make_start(&mk, mk.fd, malloc, free) == 0) {
        mcdb_iter_init(&iter, m);
        while (mcdb_iter(&iter) && rv == EXIT_SUCCESS) {
            /* Technically, passing m (which contains m->map->ptr) and an
             * alias into the map (k) as key is in violation of C99 restrict
             * pointers, but is inconsequential since it is all read-only */
            data = (char *)mcdb_iter_dataptr(&iter);
            dlen = mcdb_iter_datalen(&iter);
            k = (char *)mcdb_iter_keyptr(&iter);
            if (mcdb_find(m, k, mcdb_iter_keylen(&iter))) {
                if (data == (char *)mcdb_dataptr(m)) { /*first value for key*/
                    if (!first) {  /*!first: find last (final) value for key*/
                        while (mcdb_findnext(m, k, mcdb_iter_keylen(&iter))) {
                            data = (char *)mcdb_dataptr(m);
                            dlen = mcdb_datalen(m);
                        }
                    }
                    rv = mcdb_make_add_h(&mk, k, mcdb_iter_keylen(&iter),
                                         data, dlen);
                    if (__builtin_expect( (rv != 0), 0)) {
                        rv = MCDB_ERROR_WRITE;
                        break;
                    }
                }
            }
            else {
                rv = MCDB_ERROR_READFORMAT;
                break;
            }
            mcdb_madv_dontneed(iter.ptr, mark); /*hint to release memory pages*/
        }
        if (rv == EXIT_SUCCESS) {
            if (mcdb_make_finish(&mk) != 0 || mcdb_makefn_finish(&mk,true) != 0)
                rv = MCDB_ERROR_WRITE;
        }
    }
    else
        rv = MCDB_ERROR_WRITE;

    mcdb_make_destroy(&mk);
    mcdb_makefn_cleanup(&mk);
    return rv;
}

static int
mcdbctl_uniq(const int argc, char ** const restrict argv)
  __attribute_nonnull__  __attribute_warn_unused_result__;
static int
mcdbctl_uniq(const int argc, char ** const restrict argv)
{
    /* assert(argc == 3 || argc == 4); */      /* must be checked by caller */
    /* assert(0 == strcmp(argv[1], "uniq")); *//* must be checked by caller */
    struct mcdb m;
    int rv;
    bool first = true;
    if (argc == 4) {
        if (0 == strcmp(argv[3], "first"))
            first = true;
        else if (0 == strcmp(argv[3], "last"))
            first = false;
        else
            return MCDB_ERROR_USAGE;
    }

    m.map = mcdb_mmap_create(NULL,NULL,argv[2],malloc,free); /*fname=argv[2]*/
    if (m.map == NULL)
        return MCDB_ERROR_READ;

    rv = mcdbctl_has_unique_keys(&m);
    if (rv == true)
        rv = EXIT_SUCCESS;
    else if (rv == false)
        rv = mcdbctl_make_unique_keys(&m, first);

    mcdb_mmap_destroy(m.map);
    return rv;
}

static const char * const restrict mcdb_usage =
   "mcdbctl make  <fname.mcdb> <datafile|->\n"
   "         mcdbctl uniq  <fname.mcdb> [\"first\"|\"last\"]\n"
   "         mcdbctl dump  <fname.mcdb>\n"
   "         mcdbctl stats <fname.mcdb>\n"
   "         mcdbctl get   <fname.mcdb> <key> [seq]\n";

/*
 * mcdbctl get   <mcdb> <key> [seq]
 * mcdbctl dump  <mcdb>
 * mcdbctl stats <mcdb>
 * mcdbctl make  <mcdb> <input-file>
 * mcdbctl uniq  <mcdb> ["first"|"last"]
 *
 * mcdbctl tools require mcdb filename be specified on the command line.
 * djb cdb tools take cdb on stdin, since able to mmap stdin backed by file.
 */
int
main(const int argc, char ** const restrict argv)
{
    int rv;
    if (argc == 4 && 0 == strcmp(argv[1], "make"))
        rv = mcdbctl_make(argc, argv);
    else if ((argc == 3 || argc == 4) && 0 == strcmp(argv[1], "uniq"))
        rv = mcdbctl_uniq(argc, argv);
    else
        rv = mcdbctl_query(argc, argv);

    return rv == EXIT_SUCCESS
      ? EXIT_SUCCESS
      : mcdb_error(rv, basename(argv[0]), mcdb_usage);
}
