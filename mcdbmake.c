#define _POSIX_C_SOURCE 200112L

#include "mcdb_make.h"

#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>     /* open() */
#include <stdbool.h>   /* bool */
#include <stdlib.h>    /* malloc(), free(), EXIT_SUCCESS */
#include <string.h>    /* memcpy(), memmove(), memchr() */
#include <stdio.h>     /* fprintf(), rename() */
#include <unistd.h>    /* read() */

/* const db input line format: "+nnnn,mmmm:xxxx->yyyy\n"
 *   nnnn = key len
 *   mmmm = data len
 *   xxxx = key string
 *   yyyy = data string
 *   + , : -> \n characters are separators and used to cross-check lens
 *
 * const db blank line ends input
 */

/* MCDB_MAKE_ERROR_* enum error values are expected to be < 0 */
enum {
  MCDB_MAKE_ERROR_READ       = -1,
  MCDB_MAKE_ERROR_READFORMAT = -2,
  MCDB_MAKE_ERROR_WRITE      = -3,
  MCDB_MAKE_ERROR_RENAME     = -4,
  MCDB_MAKE_ERROR_MALLOC     = -5,
  MCDB_MAKE_ERROR_USAGE      = -6
};

struct mcdb_input {
  char * restrict buf;
  size_t pos;
  size_t datasz;
  size_t bufsz;
  int fd;
};

/* GPS: move much of this code into cdb_make.c or cdb_make_fd.c */
/* GPS: also write example interface to cdb_make that uses an mmap input file.
 * Given filename, mmap and pass to mcdb_make_parse_fd () with -1 as fd, 
 * map, mapsz as args.
 */
/* GPS: add retry on EINTR for open() and close() */
/* GPS: rename mcdb_make_parse_fd() to better name and put in a header */

/* Note: __attribute__((noinline)) is used to mark less frequent code paths
 * to prevent inlining of seldoms used paths, hopefully improving instruction
 * cache hits.
 */

static void  __attribute__((noinline))
mcdb_bufrealign (struct mcdb_input * const restrict b)
{
    if (b->fd == -1) return;             /* we use fd == -1 as flag for mmap */
    if (b->datasz - b->pos < 128 && b->pos != 0) {
        if ((b->datasz -= b->pos))
            memmove(b->buf, b->buf + b->pos, b->datasz);
        b->pos = 0;
    }
    if (b->datasz == 0) b->buf[0] = '\0';  /* known char in buf when no data */
}

static ssize_t  __attribute__((noinline))
mcdb_bufread (struct mcdb_input * const restrict b)
{
    ssize_t r;
    if (b->fd == -1) return (ssize_t)-1; /* we use fd == -1 as flag for mmap */
    do { r = read(b->fd, b->buf + b->datasz, b->bufsz - b->datasz);
    } while (r == -1 && errno == EINTR);
    return r;
}

/* (separate routine from mcdb_bufread_preamble for less frequent code path) */
static ssize_t  __attribute__((noinline))
mcdb_bufread_preamble_fill (struct mcdb_input * const restrict b)
{
    /* mcdbmake lines begin "+nnnn,mmmm:...."; max 23 chars with 32-bit nums */
    ssize_t r;
    size_t pos;
    char * const buf = b->buf;
    mcdb_bufrealign(b);
    pos = b->pos;
    r = b->datasz - pos;  /* len to search in existing buf before next read() */
    while (b->datasz - pos < 23
           && memchr(buf + b->datasz - r, ':',(size_t)r)==NULL && buf[pos]!='\n'
           && (r = mcdb_bufread(b)) > 0)
        b->datasz += r;
    return r;  /* >= 0 is success; -1 is read error */
}

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
{
    /* mcdbmake lines begin "+nnnn,mmmm:...."; max 23 chars with 32-bit nums */
    /* mcdbmake blank line ends input */
    if (b->datasz - b->pos < 23 && mcdb_bufread_preamble_fill(b) == -1)
        return MCDB_MAKE_ERROR_READ;                /* -1  error read         */
    switch (b->buf[b->pos++]) {
      case  '+': break;
      case '\n': return EXIT_SUCCESS;               /*  0  done; EXIT_SUCCESS */
      default:   return MCDB_MAKE_ERROR_READFORMAT; /* -2  error read format  */
    }
    return (   mcdb_bufread_number(b,klen)
            && b->datasz - b->pos != 0 && b->buf[b->pos++] == ','
            && mcdb_bufread_number(b,dlen)
            && b->datasz - b->pos != 0 && b->buf[b->pos++] == ':')
            ? true                                  /*  1  valid preamble     */
            : MCDB_MAKE_ERROR_READFORMAT;           /* -2  error read format  */
}

static bool
mcdb_bufread_str (struct mcdb_input * const restrict b, size_t len,
                  struct mcdb_make * const restrict m,
                  void (* const fn_addbuf)(struct mcdb_make * restrict,
                                           const char * restrict, size_t))
{
    size_t u;
    do {
        if ((u = b->datasz - b->pos) != 0) {
            u = (len < u ? len : u);
            fn_addbuf(m, b->buf + b->pos, u);
            b->pos += u;
        }
    } while ((len -= u) != 0 && (mcdb_bufrealign(b), mcdb_bufread(b) != -1));
    return (len == 0);
}

static bool  __attribute__((noinline))
mcdb_bufread_xchars (struct mcdb_input * const restrict b, const size_t len)
{
    ssize_t r = 0;
    mcdb_bufrealign(b);
    while (b->datasz - b->pos < len && (r = mcdb_bufread(b)) > 0)
        b->datasz += r;
    return (b->datasz - b->pos >= len);
}

static bool  __attribute__((noinline))
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


int
mcdb_make_parse_fd (const int inputfd, char * const buf, const size_t bufsz,
                    const int outputfd, void * (* const fn_malloc)(size_t),
                    void (* const fn_free)(void *))
{
    struct mcdb_input in = { buf, 0, 0, bufsz, inputfd };
    struct mcdb_make m;
    size_t klen;
    size_t dlen;
    int rv;

    memset(&m, '\0', sizeof(struct mcdb_make));
    if (mcdb_make_start(&m, outputfd, fn_malloc, fn_free) == -1)
        return MCDB_MAKE_ERROR_WRITE;

    while ((rv = mcdb_bufread_preamble(&in,&klen,&dlen)) > 0) {

        /* optimized frequent path: entire data line buffered and available */
        /* (klen and dlen checked < INT_MAX-8; no integer overflow possible) */
        if (klen + dlen + 3 <= in.datasz - in.pos) {
            const char * const p = in.buf + in.pos;
            if (p[klen] == '-' && p[klen+1] == '>' && p[klen+2+dlen] == '\n') {
                if (mcdb_make_add(&m, p, klen, p+klen+2, dlen) == 0)
                    in.pos += klen + dlen + 3;
                else { rv = MCDB_MAKE_ERROR_WRITE;      break; }
            } else {   rv = MCDB_MAKE_ERROR_READFORMAT; break; }
        }
        else { /* entire data line is not buffered; handle in parts */
            if (mcdb_make_addbegin(&m, klen, dlen) != -1) {
                if (mcdb_bufread_rec(&m, klen, dlen, &in))
                    mcdb_make_addend(&m);
                else { rv = MCDB_MAKE_ERROR_READFORMAT; break; }
            } else {   rv = MCDB_MAKE_ERROR_WRITE;      break; }
        }

    }

    if (rv == EXIT_SUCCESS)
        rv = (mcdb_make_finish(&m) == 0) ? EXIT_SUCCESS : MCDB_MAKE_ERROR_WRITE;

    if (rv != EXIT_SUCCESS) {
        const int errsave = errno;
        mcdb_make_destroy(&m);
        errno = errsave;
    }

    return rv;
}

int
main(const int argc, char ** restrict argv)
{
    int fd = -1;
    int rv = 0;
    char *fn;
    char *fntmp;
    enum { BUFSZ = 65536 };  /* 64 KB buffer */
    char * restrict buf;
    int errsave;

    if (argc < 3 || !*(fn = *++argv) || !*(fntmp = *++argv))
        rv = MCDB_MAKE_ERROR_USAGE;
    else if ((buf = malloc(BUFSZ)) != NULL
        && (fd = open(fntmp, O_WRONLY | O_TRUNC | O_CREAT, 0644)) != -1
        && STDIN_FILENO != fd
        && (rv = mcdb_make_parse_fd(STDIN_FILENO,buf,BUFSZ,fd,malloc,free)) == 0
      #if defined(_POSIX_SYNCHRONIZED_IO) && (_POSIX_SYNCHRONIZED_IO-0)
        && fdatasync(fd) == 0     /* ?necessary after msync()? */
      #else
        && fsync(fd) == 0         /* ?necessary after msync()? */
      #endif
        && close(fd) == 0) {      /* NFS silliness */

        fd = -1;
        if (rename(fntmp,fn) == 0)
            rv = EXIT_SUCCESS;
        else
            rv = MCDB_MAKE_ERROR_RENAME;
    }
    else if (buf == NULL)
        rv = MCDB_MAKE_ERROR_MALLOC;
    else if (rv == 0)  /* something write-related failed if this is reached */
        rv = MCDB_MAKE_ERROR_WRITE;


    errsave = errno;  /* save errno value */

    if (fd != -1)
        close(fd);
    free(buf);


    #define FATAL "cdbmake: fatal: "

    switch (rv) {
      case EXIT_SUCCESS:
        return EXIT_SUCCESS;
      case MCDB_MAKE_ERROR_USAGE:
        fprintf(stderr, "cdbmake: usage: cdbmake f ftmp\n");
        return 100;
      case MCDB_MAKE_ERROR_READFORMAT:
        fprintf(stderr, "%sunable to read input: bad format\n", FATAL);
        return 111;
      case MCDB_MAKE_ERROR_READ:
        fprintf(stderr, "%sunable to read input: ", FATAL);
        errno = errsave; perror(NULL);
        return 111;
      case MCDB_MAKE_ERROR_WRITE:
        fprintf(stderr, "%sunable to write output (%s): ", FATAL, fntmp);
        errno = errsave; perror(NULL);
        return 111;
      case MCDB_MAKE_ERROR_RENAME:
        fprintf(stderr, "%sunable to rename %s to %s: ", FATAL, fntmp, fn);
        errno = errsave; perror(NULL);
        return 111;
      case MCDB_MAKE_ERROR_MALLOC:
        fprintf(stderr, "%sunable to malloc: ", FATAL);
        errno = errsave; perror(NULL);
        return 111;
      default:
        fprintf(stderr, "%sunknown error\n", FATAL);
        return 111;
    }
}
