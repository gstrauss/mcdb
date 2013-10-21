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

#ifndef INCLUDED_PLASMA_TEST_H
#define INCLUDED_PLASMA_TEST_H

#include "plasma_attr.h"
#include "plasma_stdtypes.h"

#ifdef __cplusplus
extern "C" {
#endif


void
plasma_test_free (void *ptr);

__attribute_malloc__
__attribute_warn_unused_result__
void *
plasma_test_malloc (size_t size);

__attribute_malloc__
__attribute_warn_unused_result__
void *
plasma_test_calloc (size_t nmemb, size_t size);

__attribute_malloc__
__attribute_warn_unused_result__
void *
plasma_test_realloc (void *ptr, size_t size);


/* FUTURE: prefer C99 __func__ where available */
#ifdef __cplusplus

#define PLASMA_TEST_COND(cond) \
  ((cond) || plasma_test_cond_failure(__FILE__, __LINE__, "("#cond")"))

#define PLASMA_TEST_COND_IDX(cond,i) \
  ((cond) || plasma_test_cond_idx_failure(__FILE__, __LINE__, "("#cond")", (i)))

#define PLASMA_TEST_PERROR_ABORT(errstr, errnum) \
  plasma_test_perror_abort(__FILE__, __LINE__, (errstr), (errnum))

#else

#define PLASMA_TEST_COND(cond) \
  ((cond) || plasma_test_cond_failure(__func__, __LINE__, "("#cond")"))

#define PLASMA_TEST_COND_IDX(cond,i) \
  ((cond) || plasma_test_cond_idx_failure(__func__, __LINE__, "("#cond")", (i)))

#define PLASMA_TEST_PERROR_ABORT(errstr, errnum) \
  plasma_test_perror_abort(__func__, __LINE__, (errstr), (errnum))

#endif


__attribute_cold__
__attribute_noinline__
int
plasma_test_cond_failure (const char *func, const int line, const char *errstr)
  __attribute_nonnull__;


__attribute_cold__
__attribute_noinline__
int
plasma_test_cond_idx_failure (const char *func, const int line,
                              const char *errstr, const int loopidx)
  __attribute_nonnull__;


__attribute_cold__
__attribute_noinline__
__attribute_noreturn__
void
plasma_test_perror_abort (const char *func, const int line,
                          const char *errstr, int errnum)
  __attribute_nonnull__;


__attribute_noinline__
void
plasma_test_nthreads (const int nthr, void *(*thr_func)(void *),
                      void **thr_args, void **thr_rv)
  __attribute_nonnull_x__((2));


int
plasma_test_barrier_wait (void);


__attribute_noinline__
uint32_t
plasma_test_sysconf_nprocs (void);


#ifdef __cplusplus
}
#endif

#endif
