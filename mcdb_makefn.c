/*
 * mcdb_makefn - create temp file for mcdb creation and atomically install
 *
 * Copyright (c) 2011, Glue Logic LLC. All rights reserved. code()gluelogic.com
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
 *
 *
 * mcdb is originally based upon the Public Domain cdb-0.75 by Dan Bernstein
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif
#ifndef _XOPEN_SOURCE /* >= 500 on Linux for mkstemp(), fchmod(), fdatasync() */
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

#include "mcdb_makefn.h"
#include "mcdb_make.h"
#include "mcdb_error.h"
#include "nointr.h"
#include "code_attributes.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>  /* fchmod() */
#include <stdbool.h>   /* true */
#include <stdlib.h>    /* mkstemp(), EXIT_SUCCESS */
#include <string.h>    /* memcpy(), strlen() */
#include <stdio.h>     /* rename() */
#include <unistd.h>    /* unlink() */

int
mcdb_makefn_start (struct mcdb_make * const restrict m,
                   const char * const restrict fname,
                   void * (* const fn_malloc)(size_t),
                   void (* const fn_free)(void *))
{
    struct stat st;
    const size_t len = strlen(fname);
    char * restrict fntmp;

    m->head[0] = NULL;
    m->fntmp   = NULL;
    m->fd      = -1;

    /* preserve permission modes if previous mcdb exists; else make read-only
     * (since mcdb is *constant* -- not modified -- after creation) */
    if (stat(fname, &st) != 0) {
        st.st_mode = S_IRUSR;
        if (errno != ENOENT)
            return -1;
    }
    else if (!S_ISREG(st.st_mode)) {
        errno = EINVAL;
        return -1;
    }

    fntmp = fn_malloc((len<<1) + 9);
    if (fntmp == NULL)
        return -1;
    memcpy(fntmp, fname, len);
    memcpy(fntmp+len, ".XXXXXX", 8);
    memcpy(fntmp+len+8, fname, len+1);

    m->st_mode   = st.st_mode;
    m->fn_malloc = fn_malloc;
    m->fn_free   = fn_free;
    m->fd        = mkstemp(fntmp);
    if (m->fd != -1) {
        m->fntmp = fntmp;
        m->fname = fntmp+len+8;
        return EXIT_SUCCESS;
    }
    else {
        fn_free(fntmp);
        return -1;
    }
}

int
mcdb_makefn_finish (struct mcdb_make * const restrict m, const bool sync)
{
    return fchmod(m->fd, m->st_mode) == 0
        && (!sync || fdatasync(m->fd) == 0)
        && nointr_close(m->fd) == 0     /* NFS might report write errors here */
        && (m->fd = -2, rename(m->fntmp, m->fname) == 0) /*(fd=-2 flag closed)*/
      ? (m->fd = -1, EXIT_SUCCESS)
      : -1;
}

int
mcdb_makefn_cleanup (struct mcdb_make * const restrict m)
{
    const int errsave = errno;
    if (m->fd != -1) {                       /* (fd == -1 if mkstemp() fails) */
        unlink(m->fntmp);
        if (m->fd >= 0)
            (void) nointr_close(m->fd);
    }
    if (m->fntmp != NULL)
        m->fn_free(m->fntmp);
    if (errsave != 0)
        errno = errsave;
    return -1;
}
