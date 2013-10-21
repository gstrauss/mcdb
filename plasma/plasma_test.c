/*
 * plasma_test - basic test framework support
 *
 * Copyright (c) 2013, Glue Logic LLC. All rights reserved. code()gluelogic.com
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

/* _XOPEN_SOURCE 600 for pthread_barrier_t */
#define _XOPEN_SOURCE 600

#include "plasma_test.h"
#include "plasma_attr.h"
#include "plasma_stdtypes.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>    /* fprintf() */
#include <stdlib.h>   /* malloc() free() calloc() realloc() abort() */
#include <string.h>   /* strerror() */

void
plasma_test_free (void *ptr)
{
    free(ptr);
}

void *
plasma_test_malloc (size_t size)
{
    return malloc(size);
}

void *
plasma_test_calloc (size_t nmemb, size_t size)
{
    return calloc(nmemb, size);
}

void *
plasma_test_realloc (void *ptr, size_t size)
{
    return realloc(ptr, size);
}


int
plasma_test_cond_failure (const char *func, const int line, const char *errstr)
{
    fprintf(stderr, "%s:%d: test %s\n", func, line, errstr);
    return false;
}

int
plasma_test_cond_idx_failure (const char *func, const int line,
                              const char *errstr, const int loopidx)
{
    fprintf(stderr, "%s:%d: test %s ((idx=%d))\n", func, line, errstr, loopidx);
    return false;
}

void
plasma_test_perror_abort (const char *func, const int line,
                          const char *errstr, int errnum)
{
    if (errnum == 0)
        errnum = errno;
    fprintf(stderr, "%s:%d: %s: %s\n", func, line, errstr, strerror(errnum));
    abort();
}


/* simple convenience framework to assist in running multithreaded tests
 *
 * thr_func() may (optionally) employ plasma_test_barrier_wait();
 * thr_arg might be struct that includes shared storage array for thread status,
 * as well as other data for thread, e.g. loop iterations for concurrent tests
 *
 * (Possible alternate implementation could have had caller provide array of
 *  initialized thr_arg that would be passed into each thread (thr_arg[n]) and
 *  stored back into that array upon pthread_join().)
 */

static pthread_barrier_t plasma_test_barrier;

void
plasma_test_nthreads (const int nthreads, void *(*thr_func)(void *),
                      void **thr_args, void **thr_rv)
{
    pthread_t * const restrict thr_ids =
      plasma_test_malloc(nthreads * sizeof(pthread_t));
    void ** const thr_args_alloc = plasma_test_calloc(nthreads, sizeof(void*));
    int n, rc;
    if (thr_args == NULL)
        thr_args = thr_args_alloc;
    if (thr_rv == NULL)
        thr_rv = thr_args_alloc;
    if (thr_ids == NULL || thr_args == NULL || thr_rv == NULL)
        PLASMA_TEST_PERROR_ABORT("plasma_test_malloc", errno);
    if (0 != (rc = pthread_barrier_init(&plasma_test_barrier, NULL,
                                        (unsigned int)nthreads)))
        PLASMA_TEST_PERROR_ABORT("pthread_barrier_init", rc);
    pthread_setconcurrency(nthreads);
    for (n=0; n < nthreads; ++n) {
        if (0 != (rc = pthread_create(&thr_ids[n],NULL,thr_func,thr_args[n])))
            PLASMA_TEST_PERROR_ABORT("pthread_create", rc);
    }
    for (n=0; n < nthreads; ++n) {
        if (0 != (rc = pthread_join(thr_ids[n], &thr_rv[n])))
            PLASMA_TEST_PERROR_ABORT("pthread_join", rc);
    }
    pthread_setconcurrency(0);
    pthread_barrier_destroy(&plasma_test_barrier);
    plasma_test_free(thr_args_alloc);
    plasma_test_free(thr_ids);
}

int
plasma_test_barrier_wait (void)
{
    return pthread_barrier_wait(&plasma_test_barrier);
}


/* plasma_test_sysconf_nproc()
 * sysconf _SC_NPROCESSORS_ONLN and _SC_NPROCESSORS_CONF are not required by
 * standards, but are provided on numerous unix platforms and documented as
 * optional by Open Group.
 * http://www.opengroup.org/austin/docs/austin_512.txt
 * http://austingroupbugs.net/view.php?id=339
 * http://www.mail-archive.com/tech@openbsd.org/msg01435.html
 * Implemented by AIX/Tru64/Solaris/Linux/FreeBSD/NetBSD/OpenBSD/Darwin.
 * https://developer.apple.com/library/mac/documentation/Darwin/Reference/ManPages/man3/sysconf.3.html
 * http://stackoverflow.com/questions/4586405/get-number-of-cpus-in-linux-using-c
 */

#ifdef PLASMA_FEATURE_POSIX
#include <unistd.h>
#elif defined(_WIN32)
#include <winbase.h>
#endif

#ifdef _AIX  /*(AIX requires _ALL_SOURCE defined for this definition)*/
#define _SC_NPROCESSORS_ONLN            72
#endif

uint32_t
plasma_test_sysconf_nprocs (void)
{
    static uint32_t nprocs;
    long nprocs_onln;

    if (nprocs)
        return nprocs;

    #ifdef _SC_NPROCESSORS_ONLN
    nprocs_onln = sysconf(_SC_NPROCESSORS_ONLN);
    #elif defined(_WIN32)
    /* SYSTEM_INFO structure
     * http://msdn.microsoft.com/en-us/library/windows/desktop/ms724958%28v=vs.85%29.aspx
     * GetLogicalProcessorInformation (contains more complex code example)
     * http://msdn.microsoft.com/en-us/library/windows/desktop/ms683194%28v=vs.85%29.aspx */
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    nprocs_onln = info.dwNumberOfProcessors;
    #else
    #error "sysconf(_SC_NPROCESSORS_ONLN) not implemented"
    #endif

    if (nprocs_onln < 1)
        nprocs_onln = 1;

    return (nprocs = (uint32_t)nprocs_onln);
}
