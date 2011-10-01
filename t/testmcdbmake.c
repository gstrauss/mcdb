/*
 * testmcdbmake - performance test for mcdbctl make: generate key,value records
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
 */

#include "mcdb.h"
#include "mcdb_make.h"
#include "mcdb_error.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>     /* open() */
#include <stdio.h>     /* snprintf() */
#include <stdlib.h>    /* malloc(), free(), strtoul() */
#include <string.h>    /* memset() */
#include <unistd.h>    /* close() */

int
main (int argc, char **argv)
{
    char buf[16];
    unsigned long u = 0;
    unsigned long e;
    struct mcdb_make m;
    int fd;
    if (argc < 3) return -1;
    e = strtoul(argv[2], NULL, 10);
    if (e > 100000000u) return -1;  /*(only 8 decimal chars below; can change)*/
    unlink(argv[1]);   /* unlink for repeatable test; ignore error if missing */
    if ((fd = open(argv[1],O_RDWR|O_CREAT,0666)) != -1
        && mcdb_make_start(&m,fd,malloc,free) == 0) {
        /* generate and store records (generate 8-byte key and use as value)  */
        do { snprintf(buf, sizeof(buf), "%08lu", u);         /*generate record*/
        } while (0 == mcdb_make_add(&m,buf,8,buf,8) && ++u < e);/*store record*/
    } else e = 1; /* !u */
    return (u == e && mcdb_make_finish(&m) == 0 && close(fd) == 0)
      ? 0
      : mcdb_error(MCDB_ERROR_WRITE, "testmake", "");
    /* Note: fdatasync(fd) not called before close() due to type of usage here.
     * See comments in mcdb_make.c:mcdb_mmap_commit() for when to use fsync()
     * or fdatasync(). */
}
