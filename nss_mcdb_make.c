#include "nss_mcdb_make.h"
#include "mcdb_make.h"
#include "nointr.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h> /* mmap() munmap() */
#include <fcntl.h>    /* open() */
#include <stdbool.h>  /* true false */
#include <stdio.h>    /* snprintf() */
#include <string.h>   /* memcpy() */
#include <unistd.h>   /* fstat() */

/*
 * mechanism to create mcdb directly and mechanism to output mcdbctl make input
 * (allows for testing translation in and out)
 */
bool
nss_mcdb_make_mcdbctl_write(struct nss_mcdb_make_winfo * const restrict w)
{
    /* write data record into mcdb if struct mcdb_make * is provided */
    struct nss_mcdb_make_wbuf * const restrict wbuf = &w->wbuf;
    if (wbuf->m) {
        if (mcdb_make_addbegin(wbuf->m, w->klen+1, w->dlen) == 0) {
            mcdb_make_addbuf_key(wbuf->m, &w->tagc, 1);
            mcdb_make_addbuf_key(wbuf->m, w->key, w->klen);
            mcdb_make_addbuf_data(wbuf->m, w->data, w->dlen);
            mcdb_make_addend(wbuf->m);
            return true;
        }
        return false;
    }

    /* write data record into buffer in format for input to 'mcdbctl make'
     * (flush buffer to wbuf->fd if not enough space for next record)
     * (wbuf->bufsz must always be large enough to hold largest full line) */
    /* 6 bytes of mcdb input line overhead ("+,:->\n")
     * + max 10 digits for two nums + klen + dlen */

    if (__builtin_expect( w->klen+w->dlen+26 > wbuf->bufsz-wbuf->offset, 0)) {
        if (__builtin_expect( nointr_write(wbuf->fd, wbuf->buf,
                                                     wbuf->offset) == -1, 0))
            return false;
        wbuf->offset = 0;
    }

    wbuf->offset += snprintf(wbuf->buf + wbuf->offset, 25, "+%u,%u:%c",
                          w->klen+1, w->dlen, w->tagc);
    memcpy(wbuf->buf + wbuf->offset, w->key, w->klen);
    wbuf->offset += w->klen;
    wbuf->buf[wbuf->offset++] = '-';
    wbuf->buf[wbuf->offset++] = '>';
    memcpy(wbuf->buf + wbuf->offset, w->data, w->dlen);
    wbuf->offset += w->dlen;
    wbuf->buf[wbuf->offset++] = '\n';
    return true;
}


bool
nss_mcdb_make_dbfile_parse( struct nss_mcdb_make_winfo * const restrict w,
                            const char * const restrict dbfile,
                            bool (* const parse_mmap)
                                 (struct nss_mcdb_make_winfo * restrict,
                                  char * restrict) )
{
    void * restrict map = MAP_FAILED;
    struct stat st;
    const int fd = open(dbfile, O_RDONLY, 0);
    bool rc;

    if (fd != -1 && fstat(fd, &st) == 0)
        map = mmap(NULL, st.st_size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);

    if ((fd != -1 && nointr_close(fd) == -1) || map == MAP_FAILED) {
        if (map != MAP_FAILED)
            munmap(map, st.st_size);
        return false;
    }

    rc = parse_mmap(w, map);

    munmap(map, st.st_size);
    return rc;
}


bool
nss_mcdb_make_dbfile_flush( struct nss_mcdb_make_winfo * const restrict w )
{
    struct nss_mcdb_make_wbuf * const wbuf = &w->wbuf;

    if (w->flush != NULL) {  /* callback */
        if (__builtin_expect( !w->flush(w), 0))
            return false;
    }

    /* done writing data record into mcdb if struct mcdb_make * was provided */
    if (wbuf->m)
        return true;

    /* append blank line to end 'mcdbctl make' data and flush write buffer */
    if (__builtin_expect( wbuf->offset == wbuf->bufsz, 0)) { /* buffer full */
        if (__builtin_expect( nointr_write(wbuf->fd, wbuf->buf,
                                                     wbuf->offset) == -1, 0))
            return false;
        wbuf->offset = 0;
    }
    wbuf->buf[wbuf->offset++] = '\n';
    if (__builtin_expect( nointr_write(wbuf->fd,wbuf->buf,wbuf->offset)==-1, 0))
        return false;
    wbuf->offset = 0;

    return true;
}
