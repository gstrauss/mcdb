#include "mcdb.h"
#include "mcdb_makefmt.h"
#include "mcdb_error.h"
#include "mcdb_uint32.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>   /* open(), O_RDONLY */
#include <stdbool.h> /* bool */
#include <stdio.h>   /* printf() */
#include <stdlib.h>  /* malloc(), free(), EXIT_SUCCESS */
#include <string.h>  /* strlen() */
#include <unistd.h>  /* STDIN_FILENO */
#include <libgen.h>  /* basename() */

/* read and dump data section of mcdb
 * start of hash tables is end of key/data section (eod)
 * subtract 7 for maximal added alignment between data and hash table
 * (each record key,len,keystr,datastr is at least 8 bytes) */
static int
mcdbctl_dump(struct mcdb * const restrict m)
{
    /* (If higher performance needed, allocate iovecs and use writev) */
    unsigned char * restrict p = m->map->ptr;
    uint32_t klen;
    uint32_t dlen;
    unsigned char * const eod =
      p + mcdb_uint32_unpack_bigendian_aligned_macro(p) - 7;
    setvbuf(stdout, NULL, _IOFBF, 0); /* attempt to use fully buffered I/O */
    for (p += MCDB_HEADER_SZ; p < eod; p += 8+klen+dlen) {
        klen = mcdb_uint32_unpack_bigendian_macro(p);
        dlen = mcdb_uint32_unpack_bigendian_macro(p+4);
        printf("+%u,%u:%.*s->%.*s\n",klen,dlen,klen,p+8,dlen,p+8+klen);
    }
    puts(""); /* append blank line ("\n") to indicate end of data */
    fflush(stdout);
    return EXIT_SUCCESS;
}
/* Note: mcdbctl_stats() is equivalent test to pass/fail of djb cdbtest */
static int
mcdbctl_stats(struct mcdb * const restrict m)
{
    unsigned char * const map_ptr = m->map->ptr;
    unsigned char *p;
    uint32_t klen;
    uint32_t dlen;
    unsigned char * const eod =
      map_ptr + mcdb_uint32_unpack_bigendian_aligned_macro(map_ptr) - 7;
    unsigned long nrec = 0;
    unsigned long numd[11] = { 0,0,0,0,0,0,0,0,0,0,0 };
    unsigned int rv;
    bool rc;
    for (p = map_ptr+MCDB_HEADER_SZ; p < eod; p += klen+dlen) {
        klen = mcdb_uint32_unpack_bigendian_macro(p);
        dlen = mcdb_uint32_unpack_bigendian_macro(p+4);
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
        ++nrec;
        ++numd[ ((m->loop < 11) ? m->loop - 1 : 10) ];
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
{
    const size_t klen = strlen(key);
    if (mcdb_findstart(m, key, klen)) {
        bool rc;
        while ((rc = mcdb_findnext(m, key, klen)) && seq--)
            ;
        if (rc) {
            printf("%.*s\n",mcdb_datalen(m),mcdb_dataptr(m));
            fflush(stdout);
            return EXIT_SUCCESS;
        }
    }
    return EXIT_FAILURE;
}

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
    if (0 == strcmp(argv[1], "get")) {
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

    /* open mcdb
     * Note: not retrying open() and close() on EINTR here
     * since the program is not catching any signals */
    fd = open(argv[2], O_RDONLY, 0);  /* fname = argv[2] */
    if (fd == -1) return MCDB_ERROR_READ;
    memset(&map, '\0', sizeof(map));
    rv = mcdb_mmap_init(&map, fd);
    close(fd);
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
{
    /* assert(argc == 4); */                   /* must be checked by caller */
    /* assert(0 == strcmp(argv[1], "make")); *//* must be checked by caller */
    enum { BUFSZ = 65536 }; /* 64 KB buffer size */
    char * restrict buf = NULL;
    char * const fname = argv[2];
    char * const input = argv[3];
    const int rv = (0 == strcmp(input,"-"))
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
 */
int
main(const int argc, char ** const restrict argv)
{
    const int rv = (argc == 4 && 0 == strcmp(argv[1], "make"))
      ? mcdbctl_make(argc, argv)
      : mcdbctl_query(argc, argv);

    mcdb_usage = "mcdbctl make  <fname.mcdb> <input.txt|->\n"
                 "mcdbctl dump  <fname.mcdb>\n"
                 "mcdbctl stats <fname.mcdb>\n"
                 "mcdbctl get   <fname.mcdb> <key> [seq]\n";

    return rv == EXIT_SUCCESS ? EXIT_SUCCESS : mcdb_error(rv,basename(argv[0]));
}
