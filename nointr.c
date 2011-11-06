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
#define _XOPEN_SOURCE 600
#endif
#ifndef _ATFILE_SOURCE /* openat() */
#define _ATFILE_SOURCE
#endif

/* inlined functions defined in header
 * (generate external linkage definition in GCC versions earlier than GCC 4.3)
 * (nointr.h does not include other headers defining other inline functions
 *  in header, so simply disable C99INLINE to generate external linkage
 *  definition for all inlined functions seen (i.e. those in nointr.h))
 */
#if defined(__GNUC__) && !defined(__GNUC_STDC_INLINE__)
#define C99INLINE
#undef  NO_C99INLINE
#endif

#include "nointr.h"

/* inlined functions defined in header
 * (generate external linkage definition in C99-compliant compilers)
 * (need to -duplicate- definition from header for non-C99-compliant compiler)
 */
#if !defined(__GNUC__) || defined(__GNUC_STDC_INLINE__)
extern inline
int nointr_dup(int);
int nointr_dup(int);
extern inline
int nointr_open(const char * restrict, int, mode_t);
int nointr_open(const char * restrict, int, mode_t);
extern inline
int nointr_close(int);
int nointr_close(int);
extern inline
int nointr_ftruncate(int, off_t);
int nointr_ftruncate(int, off_t);
extern inline
ssize_t nointr_write(int, const char * restrict, size_t);
ssize_t nointr_write(int, const char * restrict, size_t);

#if defined(__linux__) || defined(__sun)
extern inline
int nointr_openat(int, const char * restrict, int, mode_t);
int nointr_openat(int, const char * restrict, int, mode_t);
#endif

#endif
