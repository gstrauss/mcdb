#ifndef _GNU_SOURCE /* enable O_CLOEXEC on GNU systems */
#define _GNU_SOURCE 1
#endif

#include "nss_mcdb_make.h"
#include "mcdb_make.h"
#include "nointr.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h> /* mmap() munmap() */
#include <fcntl.h>    /* open() */
#include <stdbool.h>  /* true false */
#include <stdio.h>    /* snprintf() */
#include <string.h>   /* memcpy() strcmp() strncmp() strrchr() */
#include <unistd.h>   /* fstat() close() */
#include <errno.h>

#ifndef O_CLOEXEC /* O_CLOEXEC available since Linux 2.6.23 */
#define O_CLOEXEC 0
#endif

#ifndef NSSWITCH_CONF_PATH
#define NSSWITCH_CONF_PATH "/etc/nsswitch.conf"
#endif

static bool
nss_mcdb_nsswitch(const char * restrict svc,
                  char * const restrict data,
                  const size_t datasz)
{
    static void * restrict nsswitch = MAP_FAILED;
    if (nsswitch == MAP_FAILED) {/*mmap /etc/nsswitch.conf and keep mmap open*/
        struct stat st;
        const int fd =
          nointr_open(NSSWITCH_CONF_PATH, O_RDONLY|O_NONBLOCK|O_CLOEXEC, 0);
        if (fd != -1) {
            if (fstat(fd, &st) == 0)
                nsswitch = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
            (void) nointr_close(fd);
        }
        else if (errno != ENOENT)
            return false;   /* failed to open /etc/nsswitch.conf that exists */
    }

    if (nsswitch != MAP_FAILED) { /* search /etc/nsswitch.conf for svc entry */
        const char * restrict p = strrchr(svc, '/');
        const size_t svclen = strlen((p != NULL) ? (svc = p+1) : svc);
        for (p = (char *)nsswitch; *p != '\0'; ++p) {
            while (*p == ' ' || *p == '\t') ++p;
            if (*p == *svc && 0 == strncmp(p, svc, svclen)) {
                p += svclen;
                while (*p == ' ' || *p == '\t') ++p;
                if (*p == ':') {  /* colon must be on same line as svc label */
                    do { ++p; } while (*p == ' ' || *p == '\t');
                    if (*p == '\0') break;
                    if (*p != '#' && *p != '\n') {
                        /* copy and normalize entry
                         * tab -> space and squash multiple whitespace chars */
                        size_t i = 0;
                        for (;*p!='#' && *p!='\n' && *p!='\0' && i<datasz; ++p){
                            if (*p == '\\' && *(p+1) == '\n')
                                ++p;  /* (join continuation lines) */
                            else if (*p != ' ' && *p != '\t')
                                data[i++] = *p;
                            else if (*(p+1) != ' ' && *(p+1) != '\t')
                                data[i++] = ' ';
                        }
                        if (*p != '#' && *p != '\n' && *p != '\0')
                            return false;  /* insufficient space in data buf */
                        if (data[i-1] == ' ')
                            --i;
                        data[i] = '\0';
                        return true;
                    }
                }
            }
            /* read to end of line, handling continuation lines */
            while (*p != '\n' && *p != '\0')
                p += (*p != '\\' || *(p+1) != '\n') ? 1 : 2;
            if (*p == '\0') break;
        }
    }

    /* svc not found in /etc/nsswitch.conf
     * default entry is "mcdb" since mcdb is being created if this code is run
     * (default entry is "mcdb dns" for hosts database) */
    if (sizeof("mcdb dns") >= datasz)
        return false;
    if (0 != strcmp(svc, "hosts"))
        memcpy(data, "mcdb", sizeof("mcdb"));
    else
        memcpy(data, "mcdb dns", sizeof("mcdb dns"));
    return true;
}

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
        struct mcdb_make * const restrict m = wbuf->m;
        if (mcdb_make_addbegin(m, w->klen+1, w->dlen) == 0) {
            mcdb_make_addbuf_key(m, &w->tagc, 1);
            mcdb_make_addbuf_key(m, w->key, w->klen);
            mcdb_make_addbuf_data(m, w->data, w->dlen);
            mcdb_make_addend(m);
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
nss_mcdb_make_dbfile( struct nss_mcdb_make_winfo * const restrict w,
                      const char * const restrict input,
                      bool (* const parse_mmap)
                           (struct nss_mcdb_make_winfo * restrict,
                            char * restrict) )
{
    struct nss_mcdb_make_wbuf * const wbuf = &w->wbuf;
    struct mcdb_make * const m = wbuf->m;
    void * restrict map = MAP_FAILED;
    struct stat st;
    int fd = -1;
    ino_t  inode;
    off_t  fsize = 0;
    time_t mtime;
    bool rc = false;

    /* make db from input.  attempt to detect if input changes during parse.
     * note: still a race condition if file was modified same second that
     * mcdbctl started processing it, and then modified again (while mcdbctl
     * parsed file) and modification did not change input file size and
     * file was modified in-place, but you should not modify file in-place!)
     * (should create new file with modifications, and rename atomically)
     */

    do {
        /* sanity check if input file changed during parse (previous loop)*/
        if (fd != -1) { /* file changed during parse; reset and redo loop */
            munmap(map, fsize);
            map = MAP_FAILED;
            rc = false;
            if (nointr_close(fd) == -1)
                break;
            if (lseek(wbuf->fd,0,SEEK_SET) == (off_t)-1) /*rewind output file*/
                break;
        }

        /* open input file */
        fd = open(input, O_RDONLY | O_NONBLOCK | O_CLOEXEC, 0);
        if (fd == -1 || fstat(fd, &st) != 0)
            break;
        inode = st.st_ino;
        fsize = st.st_size;
        mtime = st.st_mtime;
        map = mmap(NULL, fsize, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
        if (map == MAP_FAILED)
            break;

        /* generate mcdb make info */
        wbuf->offset = 0;
        if (m != NULL) {
            if (mcdb_make_start(m, wbuf->fd, m->fn_malloc, m->fn_free) != 0)
                break;
        }

        /* create first item in mcdb data as entry from nsswitch.conf
         * (optional; currently unused, but libc implementations could
         *  optimistically open mcdb, read nsswitch config, and continue.
         *  (No hash lookup needed; first data entry contains nsswitch config)
         *  If mcdb exists, it is likely going to be first database to
         *  search, and database file will already be open and mmap'd) */
        if (!nss_mcdb_nsswitch(input, w->data, w->datasz))
            break;
        w->dlen = strlen(w->data);
        w->tagc = '\0';
        w->key  = "";
        w->klen = 0;
        if (!nss_mcdb_make_mcdbctl_write(w))
            break;

        rc = parse_mmap(w,map) && (w->flush == NULL || w->flush(w));/*callback*/
        if (!rc)
            break;

        /* finish writing mcdb make output file */
        if (m != NULL)
            rc = (mcdb_make_finish(m) == 0);
        else {
            /* append blank line to end 'mcdbctl make' data, flush write buf */
            if (__builtin_expect( wbuf->offset == wbuf->bufsz, 0)) {/*buf full*/
                rc = (nointr_write(wbuf->fd, wbuf->buf, wbuf->offset) != -1);
                if (!rc)
                    break;
                wbuf->offset = 0;
            }
            wbuf->buf[wbuf->offset++] = '\n';
            rc = (nointr_write(wbuf->fd, wbuf->buf, wbuf->offset) != -1);
            wbuf->offset = 0;
        }
        if (!rc)
            break;

        /* sanity check if input file changed during parse */
        /* (stat() prior to close() to avoid A-B-A race where inode reused) */
        if (stat(input, &st) != 0);/*(stat() not fstat(); maybe new file)*/
            break;/*(proceed in event stat() fails since file parse succeeded)*/
    } while (inode != st.st_ino || fsize != st.st_size || mtime != st.st_mtime);

    if (map != MAP_FAILED)
        munmap(map, st.st_size);
    if (fd != -1) {
        if (nointr_close(fd) != 0)
            rc = false;
    }

    return rc;
}
