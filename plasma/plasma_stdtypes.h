/*
 * plasma_stdtypes - standard types from sys/types.h, stdint.h, stdbool.h
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

#ifndef INCLUDED_PLASMA_STDTYPES_H
#define INCLUDED_PLASMA_STDTYPES_H

#include "plasma_feature.h"

#include <sys/types.h>
#include <stdint.h>     /* (<inttypes.h> is more portable to pre-C99 systems) */

/*
 * bool  (stdbool.h)
 *
 * http://pubs.opengroup.org/onlinepubs/007904875/basedefs/stdbool.h.html
 * (workaround Solaris stdbool.h #error if __cplusplus or not C99 compilation)
 */

#if __STDC_VERSION__-0 >= 199901L /* C99 */ \
 || !defined(__sun) /* (Solaris is alone in having an unfriendly stdbool.h) */

#if defined(bool) && !defined(__bool_true_false_are_defined)
#undef bool
#endif

#include <stdbool.h>

#elif defined(__cplusplus)

#ifndef _Bool
#define _Bool bool
#endif
#ifndef bool
#define bool bool
#endif
#ifndef true
#define true true
#endif
#ifndef false
#define false false
#endif
#ifndef __bool_true_false_are_defined
#define __bool_true_false_are_defined 1
#endif

#else  /* stdbool */

/* stdbool.h implementation
 * - #define _Bool instead of typedef to avoid some compiler keyword warnings
 * - no #ifndef bool; intentionally conflict w/ prior #define bool if different
 * - keep sizeof(bool) == 1 for compatibility with C++ bool
 *   (all funcs using (bool *) param should agree bool is stable size of 1 byte)
 * - _Bool should promote to int (ISO C99 6.3.1.1p2) */
#if defined(__GNUC__) || defined(__clang__)  /*_Bool is builtin gcc/clang type*/
#define _Bool uint8_t;
#endif
#define bool _Bool
#define true  1
#define false 0
#define __bool_true_false_are_defined 1

#endif /* stdbool */


#endif
