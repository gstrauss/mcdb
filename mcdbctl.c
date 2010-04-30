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

static int
mcdbctl_query(const int argc, char ** restrict argv)
{
    /* assert(argc >= 3); *//* must be checked by caller */
    struct mcdb m;
    struct mcdb_mmap map;
    int rv = MCDB_ERROR_USAGE;
    /* Note: not retrying open() and close() on EINTR here
     * since the program is not catching any signals */
    char * const fname = argv[2];
    const int fd = open(fname, O_RDONLY, 0);
    if (fd == -1) return MCDB_ERROR_READ;

    memset(&map, '\0', sizeof(map));
    rv = mcdb_mmap_init(&map, fd);
    close(fd);
    if (!rv) return MCDB_ERROR_READ;
    memset(&m, '\0', sizeof(m));
    m.map = &map;

    switch (argc) {
      case 3:
        /* read and dump data section of mcdb
         * start of hash tables is end of key/data section (eod)
         * subtract 7 for maximal added alignment between data and hash table
         * (each record key,len,keystr,datastr is at least 8 bytes) */
        if (0 == strcmp(argv[1], "dump")) {
            /* (If higher performance needed, allocate iovecs and use writev) */
            unsigned char * restrict p;
            uint32_t klen;
            uint32_t dlen;
            unsigned char * const eod =
              map.ptr + mcdb_uint32_unpack_bigendian_aligned_macro(map.ptr) - 7;
            setvbuf(stdout, NULL, _IOFBF, 0); /* attempt to use full buffered */
            for (p = map.ptr+MCDB_HEADER_SZ; p < eod; p += 8+klen+dlen) {
                klen = mcdb_uint32_unpack_bigendian_macro(p);
                dlen = mcdb_uint32_unpack_bigendian_macro(p+4);
                printf("+%u,%u:%.*s->%.*s\n",klen,dlen,klen,p+8,dlen,p+8+klen);
            }
            puts(""); /* append blank line ("\n") to indicate end of data */
            fflush(stdout);
            rv = 0;
        }
        else if (0 == strcmp(argv[1], "stats")) {
            unsigned char * restrict p;
            uint32_t klen;
            uint32_t dlen;
            unsigned char * const eod =
              map.ptr + mcdb_uint32_unpack_bigendian_aligned_macro(map.ptr) - 7;
            unsigned long nrec = 0;
            unsigned long numd[11] = { 0,0,0,0,0,0,0,0,0,0,0 };
            bool rc;
            for (p = map.ptr+MCDB_HEADER_SZ; p < eod; p += 8+klen+dlen) {
                klen = mcdb_uint32_unpack_bigendian_macro(p);
                dlen = mcdb_uint32_unpack_bigendian_macro(p+4);
                /* search for key,data and track number of tries before found */
                if ((rc = mcdb_findstart(&m, (char *)p+8, klen))) {
                    do { rc = mcdb_findnext(&m, (char *)p+8, klen);
                    } while (rc && map.ptr+m.dpos != p+8+klen);
                }
                if (!rc) return MCDB_ERROR_READFORMAT;
                ++nrec;
                ++numd[ ((m.loop < 11) ? m.loop - 1 : 10) ];
            }
            printf("records %lu\n", nrec);
            for (rv = 0; rv < 10; ++rv)
                printf("d%d      %lu\n", rv, numd[rv]);
            printf(">9      %lu\n", numd[10]);
            fflush(stdout);
            rv = 0;
        }
        break;
      case 5:
        if (0 == strcmp(argv[1], "get")) {
            char *endptr;
            char * const restrict key = argv[3];
            const size_t klen = strlen(key);
            unsigned long seq = strtoul(argv[4], &endptr, 10);
            if (seq != ULONG_MAX && argv[4] != endptr && *endptr == '\0') {
                bool rc;
                if ((rc = mcdb_findstart(&m, key, klen))) {
                    while ((rc = mcdb_findnext(&m, key, klen)) && seq--)
                        ;
                    if (rc) printf("%.*s\n",mcdb_datalen(&m),mcdb_dataptr(&m));
                }
                if (!rc) exit(100); /* not found: exit nonzero without errmsg */
                fflush(stdout);
                rv = EXIT_SUCCESS;
            }
        }
        break;
      default:
        break;
    }

    mcdb_mmap_free(&map);
    return rv;
}

static int
mcdbctl_make(const int argc, char ** restrict argv)
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
main(const int argc, char ** restrict argv)
{
    const int rv = (argc == 4 && 0 == strcmp(argv[1], "make"))
      ? mcdbctl_make(argc, argv)
      : (argc >= 3)
          ? mcdbctl_query(argc, argv)
          : MCDB_ERROR_USAGE;

    mcdb_usage = "mcdbctl make  <fname.mcdb> < input.txt | - >\n"
                 "mcdbctl dump  <fname.mcdb>\n"
                 "mcdbctl stats <fname.mcdb>\n"
                 "mcdbctl get   <fname.mcdb> <key> <seq>\n";

    return rv == EXIT_SUCCESS ? EXIT_SUCCESS : mcdb_error(rv,basename(argv[0]));
}
