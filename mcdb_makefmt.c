#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif
#ifndef _XOPEN_SOURCE /* >= 500 on Linux for mkstemp(), fchmod() prototypes */
#define _XOPEN_SOURCE 500
#endif
/* large file support needed for stat(),fstat() input file > 2 GB */
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

#include "mcdb_makefmt.h"
#include "mcdb_make.h"
#include "mcdb_error.h"
#include "nointr.h"
#include "code_attributes.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/mman.h>  /* mmap(), munmap() */
#include <sys/stat.h>  /* fchmod(), umask() */
#include <fcntl.h>     /* open() */
#include <stdbool.h>   /* bool */
#include <stdlib.h>    /* mkstemp(), EXIT_SUCCESS */
#include <string.h>    /* memcpy(), memmove(), memchr() */
#include <stdint.h>    /* SIZE_MAX */
#include <stdio.h>     /* rename() */
#include <unistd.h>    /* read(), unlink(), STDIN_FILENO */

/* const db input line format: "+nnnn,mmmm:xxxx->yyyy\n"
 *   nnnn = key len
 *   mmmm = data len
 *   xxxx = key string
 *   yyyy = data string
 *   + , : -> \n characters are separators and used to cross-check lens
 *
 * const db blank line ("\n") ends input
 */

struct mcdb_input {
  char * restrict buf;
  size_t pos;
  size_t datasz;
  size_t bufsz;
  int fd;
};

/* Note: __attribute_noinline__ is used to mark less frequent code paths
 * to prevent inlining of seldoms used paths, hopefully improving instruction
 * cache hits.
 */

static ssize_t  __attribute_noinline__
mcdb_bufread_fd (struct mcdb_input * const restrict b)
  __attribute_nonnull__  __attribute_warn_unused_result__;
static ssize_t
mcdb_bufread_fd (struct mcdb_input * const restrict b)
{
    ssize_t r;
    if (b->fd == -1) return (ssize_t)-1; /* we use fd == -1 as flag for mmap */
    if (b->datasz - b->pos < 128 && b->pos != 0) {
        if ((b->datasz -= b->pos))
            memmove(b->buf, b->buf + b->pos, b->datasz);
        b->pos = 0;
    }
    retry_eintr_do_while(
      (r = read(b->fd, b->buf + b->datasz, b->bufsz - b->datasz)), (r == -1));
    if (r > 0) b->datasz += r;
    return r;
}

/* (separate routine from mcdb_bufread_preamble for less frequent code path) */
static ssize_t  __attribute_noinline__
mcdb_bufread_preamble_fill (struct mcdb_input * const restrict b)
  __attribute_nonnull__  __attribute_warn_unused_result__;
static ssize_t
mcdb_bufread_preamble_fill (struct mcdb_input * const restrict b)
{
    /* mcdbmake lines begin "+nnnn,mmmm:...."; max 23 chars with 32-bit nums */
    char * const buf = b->buf;
    ssize_t r = b->datasz - b->pos; /*len to search in buf before next read()*/
    while (memchr(buf + b->datasz - r, ':',(size_t)r) == NULL
           && (r == 0 || buf[b->pos] != '\n') /* must test if r=0 then read 1 */
           && (r = mcdb_bufread_fd(b)) > 0
           && b->datasz - b->pos < 23)
        ;
    return r;  /* >= 0 is success; -1 is read error */
}

static bool
mcdb_bufread_number (struct mcdb_input * const restrict b,
                     size_t * const restrict rv)
  __attribute_nonnull__  __attribute_warn_unused_result__;
static bool
mcdb_bufread_number (struct mcdb_input * const restrict b,
                     size_t * const restrict rv)
{
    /* custom num accumulation to ensure we stay in bounds of buf; no strtoul */
    /* const db sets limit for individual element size to INT_MAX - 8, so ensure
     * that we will not overflow that value by multiplying by 10 and adding 9
     * (214748363 * 10 + 9 == INT_MAX - 8) happens to work out perfectly.  */
    size_t num = 0;
    size_t pos = b->pos;
    const char * const buf = b->buf;
    const size_t datasz = b->datasz;

    while (datasz != pos && ((size_t)(buf[pos]-'0'))<=9uL && num <= 214748363uL)
        num = num * 10 + (buf[pos++]-'0');
    *rv = num;

    if (b->pos != pos && (datasz == pos || ((size_t)(buf[pos]-'0')) > 9uL)) {
        b->pos = pos;
        return true;
    }
    return false; /*error: no digits or too large; not bothering to set ERANGE*/
}

static int
mcdb_bufread_preamble (struct mcdb_input * const restrict b,
                       size_t * const restrict klen,
                       size_t * const restrict dlen)
  __attribute_nonnull__  __attribute_warn_unused_result__;
static int
mcdb_bufread_preamble (struct mcdb_input * const restrict b,
                       size_t * const restrict klen,
                       size_t * const restrict dlen)
{
    /* mcdbmake lines begin "+nnnn,mmmm:...."; max 23 chars with 32-bit nums */
    /* mcdbmake blank line ends input */
    if (b->datasz - b->pos < 23 && mcdb_bufread_preamble_fill(b) == -1)
        return MCDB_ERROR_READ;                /* -2  error read         */
    switch (b->buf[b->pos++]) {
      case  '+': break;
      case '\n': return EXIT_SUCCESS;          /*  0  done; EXIT_SUCCESS */
      default:   return MCDB_ERROR_READFORMAT; /* -1  error read format  */
    }
    return (   mcdb_bufread_number(b,klen)
            && b->datasz - b->pos != 0 && b->buf[b->pos++] == ','
            && mcdb_bufread_number(b,dlen)
            && b->datasz - b->pos != 0 && b->buf[b->pos++] == ':')
            ? true                             /*  1  valid preamble     */
            : MCDB_ERROR_READFORMAT;           /* -1  error read format  */
}

static bool
mcdb_bufread_str (struct mcdb_input * const restrict b, size_t len,
                  struct mcdb_make * const restrict m,
                  void (* const fn_addbuf)(struct mcdb_make * restrict,
                                           const char * restrict, size_t))
  __attribute_nonnull__  __attribute_warn_unused_result__;
static bool
mcdb_bufread_str (struct mcdb_input * const restrict b, size_t len,
                  struct mcdb_make * const restrict m,
                  void (* const fn_addbuf)(struct mcdb_make * restrict,
                                           const char * restrict, size_t))
{
    size_t u;
    do {
        if ((u = b->datasz - b->pos) > len)
            u = len;
        fn_addbuf(m, b->buf + b->pos, u);
        b->pos += u;
    } while ((len -= u) != 0 && mcdb_bufread_fd(b) > 0);
    return (len == 0);
}

static bool  __attribute_noinline__
mcdb_bufread_xchars (struct mcdb_input * const restrict b, const size_t len)
  __attribute_nonnull__  __attribute_warn_unused_result__;
static bool
mcdb_bufread_xchars (struct mcdb_input * const restrict b, const size_t len)
{
    while (b->datasz - b->pos < len && mcdb_bufread_fd(b) > 0)
        ;
    return (b->datasz - b->pos >= len);
}

static bool  __attribute_noinline__
mcdb_bufread_rec (struct mcdb_make * const restrict m,
                  const size_t klen, const size_t dlen,
                  struct mcdb_input * const restrict b)
  __attribute_nonnull__  __attribute_warn_unused_result__;
static bool
mcdb_bufread_rec (struct mcdb_make * const restrict m,
                  const size_t klen, const size_t dlen,
                  struct mcdb_input * const restrict b)
{
    return (   mcdb_bufread_str(b, klen, m, mcdb_make_addbuf_key)
            && (b->datasz - b->pos >= 2 || mcdb_bufread_xchars(b, 2))
               && b->buf[b->pos++] == '-' && b->buf[b->pos++] == '>'
            && mcdb_bufread_str(b, dlen, m, mcdb_make_addbuf_data)
            && (b->datasz - b->pos != 0 || mcdb_bufread_xchars(b, 1))
               && b->buf[b->pos++] == '\n'   );
}


/* Above are private data struct, static routines used by mcdb_makefmt_fdintofd
 *   struct mcdb_input
 *   mcdb_bufread_preamble()
 *   mcdb_bufread_rec()
 */ 


int  __attribute_noinline__
mcdb_makefmt_fdintofd (const int inputfd,
                       char * const restrict buf,
                       const size_t bufsz,
                       const int outputfd,
                       void * (* const fn_malloc)(size_t),
                       void (* const fn_free)(void *))
{
    struct mcdb_input b = { buf, 0, 0, bufsz, inputfd };
    struct mcdb_make m;
    size_t klen;
    size_t dlen;
    int rv;

    if (mcdb_make_start(&m, outputfd, fn_malloc, fn_free) == -1)
        return MCDB_ERROR_WRITE;

    if (b.fd == -1)  /* we use fd == -1 as flag for mmap */
        b.datasz = b.bufsz;

    while ((rv = mcdb_bufread_preamble(&b,&klen,&dlen)) > 0) {

        /* optimized frequent path: entire data line buffered and available */
        /* (klen and dlen checked < INT_MAX-8; no integer overflow possible) */
        if (klen + dlen + 3 <= b.datasz - b.pos) {
            const char * const p = b.buf + b.pos;
            if (p[klen] == '-' && p[klen+1] == '>' && p[klen+2+dlen] == '\n') {
                if (mcdb_make_add(&m, p, klen, p+klen+2, dlen) == 0)
                    b.pos += klen + dlen + 3;
                else { rv = MCDB_ERROR_WRITE;      break; }
            } else {   rv = MCDB_ERROR_READFORMAT; break; }
        }
        else { /* entire data line is not buffered; handle in parts */
            if (mcdb_make_addbegin(&m, klen, dlen) == 0) {
                if (mcdb_bufread_rec(&m, klen, dlen, &b))
                    mcdb_make_addend(&m);
                else { rv = MCDB_ERROR_READFORMAT; break; }
            } else {   rv = MCDB_ERROR_WRITE;      break; }
        }

    }

    if (rv == EXIT_SUCCESS)
        rv = (mcdb_make_finish(&m) == 0) ? EXIT_SUCCESS : MCDB_ERROR_WRITE;

    if (rv != EXIT_SUCCESS) {
        const int errsave = errno;
        mcdb_make_destroy(&m);
        errno = errsave;
    }

    return rv;
}

/* Examples:
 * - read from stdin:
 *     mcdb_makefmt_fdintofile(STDIN_FILENO,buf,BUFSZ,"fname.cdb",malloc,free);
 *
 * - read from mmap:  (see mcdb_makefmt_fileintofile())
 *     mcdb_makefmt_fdintofile(-1,mmap_ptr,mmap_sz,"fname.cdb",malloc,free);
 *
 * Note: no mechanism provided to clean up fd or temporary file created by
 * mkstemp() if application receives a signal that causes program termination.
 * Application can create temp file itself and call mcdb_makefmt_fdintofd()
 * directly, if that level of control is needed.
 */
int  __attribute_noinline__
mcdb_makefmt_fdintofile (const int inputfd,
                         char * const restrict buf, const size_t bufsz,
                         const char * const restrict fname,
                         void * (* const fn_malloc)(size_t),
                         void (* const fn_free)(void *))
{
    int rv = 0;
    int fd;

    struct stat st;
    const size_t len = strlen(fname);
    char * restrict fnametmp;

    /* preserve permission modes if previous mcdb exists; else make read-only
     * (since mcdb is *constant* -- not modified -- after creation) */
    if (stat(fname, &st) != 0) {
        st.st_mode = S_IRUSR;
        if (errno != ENOENT)
            return MCDB_ERROR_WRITE;
    }

    fnametmp = fn_malloc(len + 8);
    if (fnametmp == NULL)
        return MCDB_ERROR_MALLOC;
    memcpy(fnametmp, fname, len);
    memcpy(fnametmp+len, ".XXXXXX", 8);

    if ((fd = mkstemp(fnametmp)) != -1
        && (rv=mcdb_makefmt_fdintofd(inputfd,buf,bufsz,fd,fn_malloc,fn_free))==0
        && fchmod(fd, st.st_mode) == 0
        && nointr_close(fd) == 0        /* NFS might report write errors here */
        && (fd = -2, rename(fnametmp,fname) == 0)) /* (fd=-2 so not re-closed)*/
        rv = EXIT_SUCCESS;
    else {
        const int errsave = errno;
        if (rv == 0)
            rv = MCDB_ERROR_WRITE;
        if (fd != -1) {                      /* (fd == -1 if mkstemp() fails) */
            unlink(fnametmp);
            if (fd >= 0)
                (void) nointr_close(fd);
        }
        errno = errsave;
    }

    fn_free(fnametmp);
    return rv;
}

int  __attribute_noinline__
mcdb_makefmt_fileintofile (const char * const restrict infile,
                           const char * const restrict fname,
                           void * (* const fn_malloc)(size_t),
                           void (* const fn_free)(void *))
{
    void * restrict x = MAP_FAILED;
    int rv = MCDB_ERROR_READ;
    int fd;
    struct stat st;
    int errsave = 0;

    if ((fd = nointr_open(infile,O_RDONLY,0)) == -1)
        return MCDB_ERROR_READ;

    if (fstat(fd, &st) == -1
        || ((x = mmap(0,st.st_size,PROT_READ,MAP_SHARED,fd,0)) == MAP_FAILED))
        errsave = errno;

    /* close fd after mmap (no longer needed),check mmap succeeded,create cdb */
    if (nointr_close(fd) == 0 && x != MAP_FAILED) {
        posix_madvise(x, st.st_size, POSIX_MADV_SEQUENTIAL);
        /* pass entire map and size as params; fd -1 elides read()/remaps */
        rv = mcdb_makefmt_fdintofile(-1,x,st.st_size,fname,fn_malloc,fn_free);
    }

    if (x != MAP_FAILED)
        munmap(x, st.st_size);
    else if (errsave != 0)
        errno = errsave;

    return rv;
}
