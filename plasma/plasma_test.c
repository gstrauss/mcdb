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
#include "plasma_feature.h"
#include "plasma_stdtypes.h"

#include <errno.h>
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



#ifdef PLASMA_FEATURE_POSIX

#include <pthread.h>
#include <unistd.h>   /* _POSIX_BARRIERS (if supported) */

/* pthread barriers are optional POSIX extension not implemented on Mac OSX */
#ifndef _POSIX_BARRIERS

/* Brent Priddy provides a simple implementation of pthread barriers in
 * http://stackoverflow.com/questions/3640853/performance-test-sem-t-v-s-dispatch-semaphore-t-and-pthread-once-t-v-s-dispat
 * Note: code has been modified from original posted on link above */

typedef int pthread_barrierattr_t;
typedef struct
{
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int count;
    int tripCount;
} pthread_barrier_t;

static int
pthread_barrier_init (pthread_barrier_t * const restrict barrier,
                      const pthread_barrierattr_t * const restrict attr
                        __attribute_unused__,
                      const unsigned int count)
{
    if (    0 != count
         && 0 == pthread_mutex_init(&barrier->mutex, 0)) {
        if (0 == pthread_cond_init(&barrier->cond, 0)) {
            barrier->tripCount = count;
            barrier->count = 0;
            return 0;
        }
        else
            pthread_mutex_destroy(&barrier->mutex);
    }
    else if (count == 0)
        errno = EINVAL;
    return -1;
}

static int
pthread_barrier_destroy (pthread_barrier_t * const restrict barrier)
{
    pthread_cond_destroy(&barrier->cond);
    pthread_mutex_destroy(&barrier->mutex);
    return 0;
}

static int
pthread_barrier_wait (pthread_barrier_t * const restrict barrier)
{
    int rc=0; /*(rc 0 for waiters, non-zero for PTHREAD_BARRIER_SERIAL_THREAD)*/
    pthread_mutex_lock(&barrier->mutex);
    ++barrier->count < barrier->tripCount
      ? pthread_cond_wait(&barrier->cond, &barrier->mutex)
      : (pthread_cond_broadcast(&barrier->cond), (rc = 1));
    pthread_mutex_unlock(&barrier->mutex);
    return rc;
}

#endif /* !_POSIX_BARRIERS */

#elif defined(_WIN32)

/* XXX: see http://locklessinc.com/articles/pthreads_on_windows/ */

#endif /* defined(_WIN32) */



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
