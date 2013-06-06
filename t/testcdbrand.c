/*
 * testcdbrand - performance test for cdb: query keys from input file
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

/* 
 $ CDB_BUILD_PATH=/path/to/build/of/cdb-0.75
 $ gcc -pipe -Wall -Winline -pedantic -ansi -std=c99 -D_THREAD_SAFE -O3 -g -o testcdbrand -I $CDB_BUILD_PATH testcdbrand.c $CDB_BUILD_PATH/cdb.a $CDB_BUILD_PATH/[abu]*.a
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif
#ifndef _XOPEN_SOURCE /* IOV_MAX */
#define _XOPEN_SOURCE 600
#endif

/* large file support needed for open() input file > 2 GB */
/*#define PLASMA_FEATURE_ENABLE_LARGEFILE*/
/*#include <plasma/plasma_feature.h>*/

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "cdb.h"

int main (int argc, char *argv[])
{
    const char *p;
    const char *end;
    struct cdb c;
    struct stat st;
    int fd;
    const unsigned int klen = 8;
    /* input stream must have keys of constant len 8 */

    if (argc < 3) return -1;

    /* open mcdb */
    if ((fd = open(argv[1], O_RDONLY, 0777)) == -1) {perror("open"); return -1;}
    memset(&c, '\0', sizeof(struct cdb));
    cdb_init(&c, fd);

    /* open input file */
    if ((fd = open(argv[2], O_RDONLY, 0777)) == -1) {perror("open"); return -1;}
    if (fstat(fd, &st) != 0)                        {perror("fstat");return -1;}
    p = (const char *)mmap(0, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED)                            {perror("mmap"); return -1;}
    close(fd);

    /* read each key from input mmap and query mcdb
     * (no error checking since key might not exist) */
    for (end = p+st.st_size; p < end; p += klen)
        cdb_find(&c, p, klen);
    return 0;
}
