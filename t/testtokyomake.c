/* 
 $ TOKYOCABINET=/path/to/build/of/tokyocabinet-1.4.47
 $ gcc -o testtokyomake -O2 -std=c99 -fPIC -fsigned-char -Wall -DNDEBUG -D_GNU_SOURCE=1 -D_REENTRANT -D__EXTENSIONS__ -I $TOKYOCABINET testtokyomake.c $TOKYOCABINET/libtokyocabinet.a -lbz2 -lz -lpthread -lm
 */

#include <tcutil.h>
#include <tchdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main (int argc, char **argv)
{
    char buf[16];
    unsigned long u = 0;
    unsigned long e;
    TCHDB * const restrict hdb = tchdbnew();             /* create the object */
    if (argc < 3) return -1;
    e = strtoul(argv[2], NULL, 10);
    if (e > 100000000u) return -1;  /*(only 8 decimal chars below; can change)*/
    unlink(argv[1]);   /* unlink for repeatable test; ignore error if missing */
    if (tchdbopen(hdb, argv[1], HDBOWRITER|HDBOCREAT)) { /* open the database */
        /* generate and store records (generate 8-byte key and use as value)  */
        do { snprintf(buf, sizeof(buf), "%08lu", u);       /* generate record */
        } while (tchdbput(hdb, buf, 8, buf, 8) && ++u < e);   /* store record */
    } else e = 1; /* !u */
    return (u == e && tchdbclose(hdb))                  /* close the database */
      ? (tchdbdel(hdb), 0)                               /* delete the object */
      : (fprintf(stderr, "tchdb error: %s\n", tchdberrmsg(tchdbecode(hdb))),-1);
}
