/*
 * testzero - size test for mcdb creation: generate given num of 64K records
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
 *
 *
 * mcdb is originally based upon the Public Domain cdb-0.75 by Dan Bernstein
 */

#include "mcdb.h"
#include "mcdb_make.h"
#include "mcdb_error.h"
#include "nointr.h"
#include "uint32.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>     /* open() */
#include <stdlib.h>    /* strtoul() */
#include <unistd.h>    /* close() */

static const char data[65536];

int
main(int argc,char **argv)
{
    struct mcdb_make m;
    unsigned long loop = argc > 1 ? strtoul(argv[1], NULL, 10) : 0;
    const int fd = argc > 2 ? nointr_open(argv[2], O_RDWR|O_CREAT, 0666) : -1;
    char buf[4];
    /*(avoid gcc warning using key with uint32_strpack_bigendian_aligned_macro:
     * dereferencing type-punned pointer will break strict-aliasing rules)*/
    char * const key = buf;

    if (mcdb_make_start(&m,fd,malloc,free) == -1)
        return mcdb_error(MCDB_ERROR_WRITE, "testzero", "");

    while (loop--) {
        uint32_strpack_bigendian_aligned_macro(key,loop);
        if (mcdb_make_add(&m,key,sizeof(uint32_t),data,sizeof(data)) == -1)
            return mcdb_error(MCDB_ERROR_WRITE, "testzero", "");
    }

    if (mcdb_make_finish(&m) == -1 || (fd != -1 && nointr_close(fd) != 0))
        return mcdb_error(MCDB_ERROR_WRITE, "testzero", "");
    /* Note: fdatasync(m.fd) is not called here due to type of usage here.
     * See comments in mcdb_make.c:mcdb_mmap_commit() for when to use
     * fsync() or fdatasync(). */

    return 0;
}
