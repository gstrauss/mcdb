#define _POSIX_C_SOURCE 200112L

#include "mcdb_make.h"

#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>     /* open() */
#include <stdlib.h>    /* malloc(), free(), strtoul(), EXIT_SUCCESS */
#include <string.h>    /* memcpy(), memmove(), memchr() */
#include <stdio.h>     /* fprintf(), rename() */
#include <unistd.h>    /* read() */

enum {
  MCDB_ERROR_USAGE = 1,
  MCDB_ERROR_READFORMAT,
  MCDB_ERROR_READ,
  MCDB_ERROR_WRITE,
  MCDB_ERROR_RENAME,
  MCDB_ERROR_MALLOC
};

struct mcdb_input {
  char * restrict buf;
  size_t pos;
  size_t datasz;
  size_t bufsz;
  int fd;
};

/* GPS: move much of this code into cdb_make.c or cdb_make_fd.c */

static void
mcdb_bufrealign (struct mcdb_input * const restrict b)
{
    if (b->datasz - b->pos < 256 && b->pos != 0) {
        if ((b->datasz -= b->pos))
            memmove(b->buf, b->buf + b->pos, b->datasz);
        b->pos = 0;
    }
    if (b->datasz == 0) b->buf[0] = '\0';  /* known char in buf when no data */
}

static ssize_t
mcdb_bufread (struct mcdb_input * const restrict b)
{
    ssize_t r;
/* GPS: need to store and pass separate fd for caller to be able to open
 * other socket or fd stream
 */
    do { r = read(b->fd, b->buf + b->datasz, b->bufsz - b->datasz);
    } while (r == -1 && errno == EINTR);
    return r;
}

static ssize_t
mcdb_bufread_startline (struct mcdb_input * const restrict b)
{
    ssize_t r;
    size_t pos;
    char * const buf = b->buf;

    mcdb_bufrealign(b);

    pos = b->pos;
    r = b->datasz - pos;       /* to search existing buf before reading more */
    /* mcdbmake lines begin "+nnnn,mmmm:...."; max 23 chars with 32-bit nums */
    while (b->datasz - pos < 23
           && memchr(buf + b->datasz - r,':',(size_t)r)==NULL && buf[pos]!='\n'
           && (r = mcdb_bufread(b)) > 0)
        b->datasz += r;
    return r;  /* -1 on error, any other value is success */
}

static int
mcdb_bufread_key (struct mcdb_input * const restrict b, size_t len,
                  struct mcdb_make * const restrict m)
{
    size_t u = b->datasz - b->pos;
    if (u != 0) {
        u = (len < u ? len : u);
        mcdb_make_addbuf_key(m, b->buf + b->pos, u);
        b->pos += u;
        len -= u;
    }
/* GPS: consistency in return values when I am defining the interface */
    return (len == 0) ? 0 : mcdb_make_addfdstream_key(m, b->fd, len);
}

static ssize_t
mcdb_bufread_arrow (struct mcdb_input * const restrict b)
{
    ssize_t r = 0;
    mcdb_bufrealign(b);
    /* mcdbmake lines contain "->" between key and data */
    while (b->datasz - b->pos < 2 && (r = mcdb_bufread(b)) > 0)
        b->datasz += r;
    return r;  /* -1 on error, any other value is success */
}

static int
mcdb_bufread_data (struct mcdb_input * const restrict b, size_t len,
                   struct mcdb_make * const restrict m)
{
    size_t u = b->datasz - b->pos;
    if (u != 0) {
        u = (len < u ? len : u);
        mcdb_make_addbuf_data(m, b->buf + b->pos, u);
        b->pos += u;
        len -= u;
    }
/* GPS: consistency in return values when I am defining the interface */
    return (len == 0) ? 0 : mcdb_make_addfdstream_data(m, b->fd, len);
}

static ssize_t
mcdb_bufread_endline (struct mcdb_input * const restrict b)
{
    ssize_t r = 0;
    mcdb_bufrealign(b);
    /* mcdbmake lines must end in '\n' */
    if (b->datasz == b->pos && (r = mcdb_bufread(b)) > 0)
        b->datasz += r;
    return r;  /* -1 on error, any other value is success */
}


/* GPS: take malloc and free as arguments? */
/* GPS: make this code clearer to read */
int mcdb_make_parse_fd (const int inputfd, const int outputfd)
{
    enum { BUFSZ = 65536 };
    struct mcdb_input in;
    struct mcdb_make m;
    char *p;
    char *q;
    size_t klen;
    size_t dlen;
    int rv = EXIT_SUCCESS;

    memset(&in, '\0', sizeof(struct mcdb_input));
    in.fd = inputfd;
    in.bufsz = BUFSZ;
    in.buf = malloc(BUFSZ);  /* 64 KB buffer */
    if (in.buf == NULL)
        return MCDB_ERROR_WRITE;

    memset(&m, '\0', sizeof(struct mcdb_make));
    if (mcdb_make_start(&m, outputfd, malloc, free) == -1) {
        free(in.buf);
        return MCDB_ERROR_WRITE;
    }

    /* line format: "+nnnn,mmmm:xxxx->yyyy\n"
     *   nnnn = key len
     *   mmmm = data len
     *   xxxx = key string
     *   yyyy = data string
     *   + , : -> \n characters are separators and used to cross-check lens
     */

    for (;;) {

        if (mcdb_bufread_startline(&in) != -1)
            p = in.buf + in.pos;
        else { rv = MCDB_ERROR_READ; break; }

        if (*p == '\n') {
            rv = (mcdb_make_finish(&m) == 0) ? EXIT_SUCCESS : MCDB_ERROR_WRITE;
            break;
        }

        if (*p == '+')
            ++p;
        else { rv = MCDB_ERROR_READFORMAT; break; }

        /* "keylen," */
        if ('0' <= *p && *p <= '9')
            klen = strtoul(p, &q, 10); /* mcdb_make_addbegin checks klen <2GB */
        else { rv = MCDB_ERROR_READFORMAT; break; }
        if (*q == ',')
            p = q + 1;
        else { rv = MCDB_ERROR_READFORMAT; break; }

        /* "datalen:" */
        if ('0' <= *p && *p <= '9')
            dlen = strtoul(p, &q, 10); /* mcdb_make_addbegin checks dlen <2GB */
        else { rv = MCDB_ERROR_READFORMAT; break; }
        if (*q == ':')
            in.pos = q - in.buf + 1;
        else { rv = MCDB_ERROR_READFORMAT; break; }

        /* shortcut if entire data line is buffered and available */
        if (klen < BUFSZ && dlen < BUFSZ && in.datasz-in.pos >= klen+dlen+3) {
            p = in.buf + in.pos;
            if (p[klen] == '-' && p[klen+1] == '>' && p[klen+2+dlen] == '\n') {
                if (mcdb_make_add(&m, p, klen, p+klen+2, dlen) == 0)
                    in.pos += klen + dlen + 3;
                else { rv = MCDB_ERROR_WRITE; break; }
            }
            else { rv = MCDB_ERROR_READFORMAT; break; }
        }
        else { /* entire data line is not buffered; handle in parts */
/* GPS: might extract to separate subroutine */
/* GPS: instead of code duplication, set flag for key/data difference fns? */
            if (mcdb_make_addbegin(&m, klen, dlen) == -1)
                { rv = MCDB_ERROR_WRITE; break; }
            if (mcdb_bufread_key(&in, klen, &m) == -1)
                { rv = MCDB_ERROR_READFORMAT; break; }
            if (mcdb_bufread_arrow(&in) == -1 || in.datasz - in.pos < 2
                || in.buf[in.pos] != '-' || in.buf[in.pos+1] != '>')
                { rv = MCDB_ERROR_READFORMAT; break; }
            in.pos += 2;
            if (mcdb_bufread_data(&in, klen, &m) == -1)
                { rv = MCDB_ERROR_READFORMAT; break; }
            mcdb_make_addend(&m);
            if (mcdb_bufread_endline(&in) == -1 || in.buf[in.pos] != '\n')
                { rv = MCDB_ERROR_READFORMAT; break; }
            ++in.pos;
        }

    }

    free(in.buf);

    if (rv != EXIT_SUCCESS)
        mcdb_make_destroy(&m);

    return rv;
}

int
main(const int argc, char ** restrict argv)
{
    int fd;
    int rv = 0;
    char *fn;
    char *fntmp;

    if (argc < 3 || !*(fn = *++argv) || !*(fntmp = *++argv)) {
        fprintf(stderr,"cdbmake: usage: cdbmake f ftmp\n");
        return 100;
    }


    if ((fd = open(fntmp, O_WRONLY | O_TRUNC | O_CREAT, 0644)) != -1
        && STDIN_FILENO != fd
        && (rv = mcdb_make_parse_fd(STDIN_FILENO, fd)) == 0
      #if defined(_POSIX_SYNCHRONIZED_IO) && (_POSIX_SYNCHRONIZED_IO-0)
        && fdatasync(fd) == 0     /* ?necessary after msync()? */
      #else
        && fsync(fd) == 0         /* ?necessary after msync()? */
      #endif
        && close(fd) == 0) {      /* NFS silliness */
        if (rename(fntmp,fn) == 0)
            rv = EXIT_SUCCESS;
        else
            rv = MCDB_ERROR_RENAME;
    }
    else if (rv == 0)
        rv = MCDB_ERROR_WRITE;


    #define FATAL "cdbmake: fatal: "

    switch (rv) {
      case EXIT_SUCCESS:
        return EXIT_SUCCESS;
      case MCDB_ERROR_USAGE:
        fprintf(stderr, "cdbmake: usage: cdbmake f ftmp\n");
        return 100;
      case MCDB_ERROR_READFORMAT:
        fprintf(stderr, "%sunable to read input: bad format\n", FATAL);
        return 111;
      case MCDB_ERROR_READ:
        fprintf(stderr, "%sunable to read input: ", FATAL);
        perror(NULL);
        return 111;
      case MCDB_ERROR_WRITE:
        fprintf(stderr, "%sunable to write output (%s): ", FATAL, fntmp);
        perror(NULL);
        return 111;
      case MCDB_ERROR_RENAME:
        fprintf(stderr, "%sunable to rename %s to %s: ", FATAL, fntmp, fn);
        perror(NULL);
        return 111;
      case MCDB_ERROR_MALLOC:
        fprintf(stderr, "%sunable to malloc: ", FATAL);
        perror(NULL);
        return 111;
      default:
        fprintf(stderr, "%sunknown error\n", FATAL);
        return 111;
    }
}
