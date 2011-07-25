#ifndef _POSIX_C_SOURCE /* 200112L for XSI-compliant sterror_r() */
#define _POSIX_C_SOURCE 200112L
#endif

#include "mcdb_error.h"

#include <errno.h>
#include <stdio.h>   /* fprintf() */
#include <string.h>  /* strerror_r() */
#include <stdlib.h>  /* EXIT_SUCCESS */

const char *mcdb_usage = "";

int
mcdb_error (const int rv, const char * const restrict prefix)
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
