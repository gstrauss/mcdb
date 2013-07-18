/*
 * nointr - convenience routines to retry system calls if errno == EINTR
 *
 * Copyright (c) 2010, Glue Logic LLC. All rights reserved. code()gluelogic.com
 *
 *  This file is part of mcdb.
 *
 *  mcdb is free software: you can redistribute it and/or modify it under
 *  the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 2.1 of the License, or
 *  (at your option) any later version.
 *
 *  mcdb is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with mcdb.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _XOPEN_SOURCE /* _XOPEN_SOURCE >= 500 for XSI-compliant ftruncate() */
#define _XOPEN_SOURCE 700
#endif
/* (_ATFILE_SOURCE or _XOPEN_SOURCE >= 700) */
#ifndef _ATFILE_SOURCE /* openat() */
#define _ATFILE_SOURCE
#endif

#include "nointr.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

int
nointr_dup(const int fd)
{ int r; retry_eintr_do_while((r = dup(fd)), (r != -1)); return r; }

int
nointr_open(const char * const restrict fn, const int flags, const mode_t mode)
{ int r; retry_eintr_do_while((r = open(fn,flags,mode)), (r == -1)); return r; }

int
nointr_close(const int fd)
{ int r; retry_eintr_do_while((r = close(fd)), (r != 0)); return r; }

ssize_t
nointr_write(const int fd, const char * restrict buf, size_t sz)
{
    ssize_t w;
    do { w = write(fd,buf,sz); } while (w!=-1 ? (buf+=w,sz-=w) : errno==EINTR);
    return w;
}

#if defined(_XOPEN_SOURCE) && _XOPEN_SOURCE-0 >= 500
#if defined(__hpux) && defined(_LARGEFILE64_SOURCE) \
 && !defined(_LP64) && !defined(__LP64__)
#define ftruncate(fd,sz)  __ftruncate64((fd),(sz))
#endif
int
nointr_ftruncate(const int fd, const off_t sz)
{ int r; retry_eintr_do_while((r = ftruncate(fd, sz)),(r != 0)); return r; }
#endif

#ifdef AT_FDCWD
int
nointr_openat(const int dfd, const char * const restrict fn,
              const int flags, const mode_t mode)
{ int r; retry_eintr_do_while((r=openat(dfd,fn,flags,mode)),(r==-1)); return r;}
#endif
