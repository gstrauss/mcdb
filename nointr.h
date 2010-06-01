#ifndef INCLUDED_NOINTR_H
#define INCLUDED_NOINTR_H

#include "code_attributes.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

/* Note: please do not use these routines indescriminantly.
 * There are times when a library call should be interrupted by EINTR and
 * processed as-is.  One such example is when read(), where the data read
 * should be processed, unless a certain amount of data is required and it
 * is reasonable to block and wait longer.
 */

int  C99INLINE
nointr_open(const char * const restrict fn, const int flags, const mode_t mode)
  __attribute_nonnull__  __attribute_warn_unused_result__;
#if !defined(NO_C99INLINE)
int  C99INLINE
nointr_open(const char * const restrict fn, const int flags, const mode_t mode)
{ int r; retry_eintr_do_while((r = open(fn,flags,mode)), (r == -1)); return r; }
#endif

int  C99INLINE
nointr_close(const int fd);
#if !defined(NO_C99INLINE)
int  C99INLINE
nointr_close(const int fd)
{ int r; retry_eintr_do_while((r = close(fd)), (r != 0)); return r; }
#endif

ssize_t  C99INLINE
nointr_write(const int fd, const char * restrict buf, size_t sz)
  __attribute_nonnull__  __attribute_warn_unused_result__;
#if !defined(NO_C99INLINE)
ssize_t  C99INLINE
nointr_write(const int fd, const char * restrict buf, size_t sz)
{
    ssize_t w;
    do { w = write(fd,buf,sz); } while (w!=-1 ? (buf+=w,sz-=w) : errno==EINTR);
    return w;
}
#endif

/* caller must #define _XOPEN_SOURCE >= 500 for XSI-compliant ftruncate() */
#if defined(_XOPEN_SOURCE) && _XOPEN_SOURCE-0 >= 500
int  C99INLINE
nointr_ftruncate(const int fd, const off_t sz);
#if !defined(NO_C99INLINE)
int  C99INLINE
nointr_ftruncate(const int fd, const off_t sz)
{ int r; retry_eintr_do_while((r = ftruncate(fd, sz)),(r != 0)); return r; }
#endif
#endif

/* caller must #define _ATFILE_SOURCE on Linux for openat() */
#if (defined(__linux) && defined(_ATFILE_SOURCE)) || defined(__sun)
int  C99INLINE
nointr_openat(const int dfd, const char * const restrict fn,
              const int flags, const mode_t mode)
  __attribute_nonnull__  __attribute_warn_unused_result__;
#if !defined(NO_C99INLINE)
int  C99INLINE
nointr_openat(const int dfd, const char * const restrict fn,
              const int flags, const mode_t mode)
{ int r; retry_eintr_do_while((r=openat(dfd,fn,flags,mode)),(r==-1)); return r;}
#endif
#endif

#endif
