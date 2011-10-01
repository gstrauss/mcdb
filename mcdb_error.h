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

#ifndef INCLUDED_MCDB_ERROR_H
#define INCLUDED_MCDB_ERROR_H

#include "code_attributes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* MCDB_ERROR_* enum error values are expected to be < 0 */
enum {
  MCDB_ERROR_READFORMAT = -1,
  MCDB_ERROR_READ       = -2,
  MCDB_ERROR_WRITE      = -3,
  MCDB_ERROR_MALLOC     = -4,
  MCDB_ERROR_USAGE      = -5
};

extern int
mcdb_error (int, const char * restrict, const char * restrict)
  __attribute_nonnull__  __attribute_cold__  __attribute_warn_unused_result__;

#ifdef __cplusplus
}
#endif

#endif
