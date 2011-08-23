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

#include "mcdb.h"
#include "mcdb_makefmt.h"
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

/* read and dump data section of mcdb
 * start of hash tables is end of key/data section (eod)
 * subtract 7 for maximal added alignment between data and hash table
 * (each record key,len,keystr,datastr is at least 8 bytes) */
static int
mcdbctl_dump(struct mcdb * const restrict m)
  __attribute_nonnull__  __attribute_warn_unused_result__;
static int
mcdbctl_dump(struct mcdb * const restrict m)
{
    unsigned char * restrict p = m->map->ptr;
    uint32_t klen;
    uint32_t dlen;
    unsigned char * const eod =
      p + uint64_strunpack_bigendian_aligned_macro(p) - MCDB_PAD_MASK;
    unsigned char *j = p + (1u << 25); /* add 32 MB */
    int    iovcnt = 0;
    size_t iovlen = 0;
    size_t buflen = 0;
    enum { MCDB_IOVNUM = IOV_MAX };/* assert(IOV_MAX >= 7) */
    struct iovec iov[MCDB_IOVNUM]; /* each db entry uses 7 iovecs */
    char buf[(MCDB_IOVNUM * 3)];   /* each db entry might use (2) * 10 chars */
      /* oversized buffer since all num strings must add up to less than max */

    posix_madvise(p, (size_t)(eod - p),
                  POSIX_MADV_SEQUENTIAL | POSIX_MADV_WILLNEED);
    for (p += MCDB_HEADER_SZ; p < eod; p += 8+klen+dlen) {

        /* hint to release memory pages every 32 MB */
        if (__builtin_expect( (p >= j), 0)) {
            posix_madvise(j - (1u << 25), (1u << 25), POSIX_MADV_DONTNEED);
            j += (1u << 25); /* add 32 MB */
        }

        klen = uint32_strunpack_bigendian_macro(p);
        dlen = uint32_strunpack_bigendian_macro(p+4);

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

        iov[iovcnt].iov_base = p+8;
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

        iov[iovcnt].iov_base = p+8+klen;
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
    unsigned char * const map_ptr = m->map->ptr;
    unsigned char *p;
    uint32_t klen;
    uint32_t dlen;
    unsigned char * const eod =
      map_ptr+uint64_strunpack_bigendian_aligned_macro(map_ptr)-MCDB_PAD_MASK;
    unsigned char *j = map_ptr + MCDB_HEADER_SZ + (1u << 25); /* add 32 MB */
    unsigned long nrec = 0;
    unsigned long numd[11] = { 0,0,0,0,0,0,0,0,0,0,0 };
    unsigned int rv;
    bool rc;
    posix_madvise(map_ptr, m->map->size,
                  POSIX_MADV_SEQUENTIAL | POSIX_MADV_WILLNEED);
    for (p = map_ptr+MCDB_HEADER_SZ; p < eod; p += klen+dlen) {
        klen = uint32_strunpack_bigendian_macro(p);
        dlen = uint32_strunpack_bigendian_macro(p+4);
        /* Search for key,data and track number of tries before found.
         * Technically, passing m (which contains m->map->ptr) and an
         * alias into the map (p+8) as key is in violation of C99 restrict
         * pointers, but is inconsequential since it is all read-only */
        p += 8;
        if ((rc = mcdb_findstart(m, (char *)p, klen))) {
            do { rc = mcdb_findnext(m, (char *)p, klen);
            } while (rc && map_ptr+m->dpos != p+klen);
        }
        if (!rc) return MCDB_ERROR_READFORMAT;
        ++numd[ ((m->loop < 11) ? m->loop - 1 : 10) ];
        ++nrec;
        /* hint to release memory pages every 32 MB */
        if (__builtin_expect( (p >= j), 0)) {
            posix_madvise(j - (1u << 25), (1u << 25), POSIX_MADV_DONTNEED);
            j += (1u << 25); /* add 32 MB */
        }

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
    memset(&map, '\0', sizeof(map));
    rv = mcdb_mmap_init(&map, fd);
    (void) nointr_close(fd);
    if (!rv) return MCDB_ERROR_READ;
    memset(&m, '\0', sizeof(m));
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

/*
 * mcdbctl get   <mcdb> <key> [seq]
 * mcdbctl dump  <mcdb>
 * mcdbctl stats <mcdb>
 * mcdbctl make  <mcdb> <input-file>
 *
 * mcdbctl tools require mcdb filename be specified on the command line.
 * djb cdb tools take cdb on stdin, since able to mmap stdin backed by file.
 */
int
main(const int argc, char ** const restrict argv)
{
    const int rv = (argc == 4 && 0 == strcmp(argv[1], "make"))
      ? mcdbctl_make(argc, argv)
      : mcdbctl_query(argc, argv);

    mcdb_usage = "mcdbctl make  <fname.mcdb> <datafile|->\n"
                 "         mcdbctl dump  <fname.mcdb>\n"
                 "         mcdbctl stats <fname.mcdb>\n"
                 "         mcdbctl get   <fname.mcdb> <key> [seq]\n";

    return rv == EXIT_SUCCESS ? EXIT_SUCCESS : mcdb_error(rv,basename(argv[0]));
}
