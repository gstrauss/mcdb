#include "mcdb_makefmt.h"
#include "mcdb_error.h"

#include <stdlib.h>  /* malloc(), free(), EXIT_SUCCESS */
#include <unistd.h>  /* STDIN_FILENO */
#include <libgen.h>  /* basename() */

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
