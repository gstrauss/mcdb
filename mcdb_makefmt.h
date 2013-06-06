/*
 * mcdb_makefmt - create mcdb from cdbmake input format
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

#ifndef INCLUDED_MCDB_MAKEFMT_H
#define INCLUDED_MCDB_MAKEFMT_H

#include "plasma/plasma_feature.h"
#include "plasma/plasma_attr.h"
#include "plasma/plasma_stdtypes.h"  /* size_t */

#ifdef __cplusplus
extern "C" {
#endif

/* Note: ensure output file is open() O_RDWR if calling mcdb_makefmt_fdintofd()
 * or else mmap() may fail.
 * Note: caller of mcdb_makefmt_fdintofd() should choose whether or not to then
 * call fsync() or fdatasync().  See notes in mcdb_make.c:mcdb_mmap_commit() */

int
mcdb_makefmt_fdintofd (int, char * restrict, size_t,
                       int, void * (*)(size_t), void (*)(void *))
  __attribute_nonnull__  __attribute_warn_unused_result__;

int
mcdb_makefmt_fdintofile (const int, char * restrict, size_t,
                         const char * restrict,
                         void * (*)(size_t), void (*)(void *))
  __attribute_nonnull__  __attribute_warn_unused_result__;
int
mcdb_makefmt_fileintofile (const char * restrict, const char * restrict,
                           void * (*)(size_t), void (*)(void *))
  __attribute_nonnull__  __attribute_warn_unused_result__;

#ifdef __cplusplus
}
#endif

#endif
