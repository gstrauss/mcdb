/*
 * mcdb_error - mcdb error codes and messages
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

#ifndef _POSIX_C_SOURCE /* 200112L for XSI-compliant sterror_r() */
#define _POSIX_C_SOURCE 200112L
#endif

#include "mcdb_error.h"

#include <errno.h>
#include <stdio.h>   /* fprintf() */
#include <string.h>  /* strerror_r() */
#include <stdlib.h>  /* EXIT_SUCCESS */

int
mcdb_error (const int rv, const char * const restrict prefix,
            const char * const restrict mcdb_usage)
{
    char errstr[128];

    switch (rv) {

      case EXIT_SUCCESS:
        return EXIT_SUCCESS;

      case MCDB_ERROR_READFORMAT:
        fprintf(stderr, "%s: read input: bad format\n", prefix);
        return 111;

      case MCDB_ERROR_READ:
        if (strerror_r(errno, errstr, sizeof(errstr)) != 0) errstr[0] = '\0';
        fprintf(stderr, "%s: read input: %s\n", prefix, errstr);
        return 111;

      case MCDB_ERROR_WRITE:
        if (strerror_r(errno, errstr, sizeof(errstr)) != 0) errstr[0] = '\0';
        fprintf(stderr, "%s: write output: %s\n", prefix, errstr);
        return 111;

      case MCDB_ERROR_MALLOC:
        if (strerror_r(errno, errstr, sizeof(errstr)) != 0) errstr[0] = '\0';
        fprintf(stderr, "%s: malloc: %s\n", prefix, errstr);
        return 111;

      case MCDB_ERROR_USAGE:
        fprintf(stderr, "%s: %s", prefix, mcdb_usage);
        return 101;

      default:
        fprintf(stderr, "%s: (unknown error)\n", prefix);
        return 111;

    }
}
