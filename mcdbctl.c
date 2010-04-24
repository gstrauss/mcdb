#include "mcdb.h"
#include "mcdb_makefmt.h"
#include "mcdb_error.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>   /* open(), O_RDONLY */
#include <stdbool.h> /* bool */
#include <stdio.h>   /* printf() */
#include <stdlib.h>  /* malloc(), free(), EXIT_SUCCESS */
#include <string.h>  /* strlen() */
#include <unistd.h>  /* STDIN_FILENO */
#include <libgen.h>  /* basename() */

int
print_kvalue(const char * const restrict fname,
             const char * const restrict key, unsigned int seq)
{
    struct mcdb m;
    struct mcdb_mmap map;
    const size_t klen = strlen(key);
    bool rc;
    const int fd = open(fname, O_RDONLY, 0);
    if (fd == -1) return MCDB_ERROR_READ;

    memset(&map, '\0', sizeof(map));
    rc = mcdb_mmap_init(&map, fd);
    close(fd);
    if (!rc) return MCDB_ERROR_READ;
    memset(&m, '\0', sizeof(m));
    m.map = &map;

    if ((rc = mcdb_findstart(&m, key, klen))) {
        while ((rc = mcdb_findnext(&m, key, klen)) && seq--) ;
        if (rc) printf("%.*s\n", mcdb_datalen(&m), mcdb_dataptr(&m));
    }

    mcdb_mmap_free(&map);
    return rc;
}

int
main(const int argc, char ** restrict argv)
{
    enum { BUFSZ = 65536 }; /* 64 KB buffer size */
    char * restrict buf = NULL;
    char *fname;
    int rv;

    mcdb_usage = "mcdbmake fname.cdb\n";

    rv = (argc == 2 && *(fname = argv[1]))
      ? ((buf = malloc(BUFSZ)) != NULL)
          ? mcdb_makefmt_fdintofile(STDIN_FILENO,buf,BUFSZ,fname,malloc,free)
          : MCDB_ERROR_MALLOC
      : MCDB_ERROR_USAGE;

    free(buf);
    return rv == EXIT_SUCCESS ? EXIT_SUCCESS : mcdb_error(rv,basename(argv[0]));
}
