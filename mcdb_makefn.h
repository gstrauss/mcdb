/*
 * mcdb_makefn - create temp file for mcdb creation and atomically install
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

#ifndef INCLUDED_MCDB_MAKEFN_H
#define INCLUDED_MCDB_MAKEFN_H

#include "plasma/plasma_feature.h"
#include "plasma/plasma_attr.h"
#include "plasma/plasma_stdtypes.h"  /* size_t */
#include "mcdb_make.h"
PLASMA_ATTR_Pragma_once

#ifdef __cplusplus
extern "C" {
#endif

__attribute_nonnull__
__attribute_warn_unused_result__
EXPORT extern int
mcdb_makefn_start (struct mcdb_make * restrict, const char * restrict,
                   void * (*)(size_t), void (*)(void *));

__attribute_nonnull__
__attribute_nothrow__
__attribute_warn_unused_result__
EXPORT extern int
mcdb_makefn_finish (struct mcdb_make * restrict, const bool);

__attribute_nonnull__
EXPORT extern int
mcdb_makefn_cleanup (struct mcdb_make * restrict);

#ifdef __cplusplus
}
#endif

#endif
