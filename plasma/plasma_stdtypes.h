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


/* C++ header equivalents to C headers: http://www.cplusplus.com/reference/ */


/* sys/types.h
 *
 * http://www.opengroup.org/onlinepubs/9699919799/basedefs/sys_types.h.html
 * http://www.gnu.org/software/gnulib/manual/html_node/sys_002ftypes_002eh.html
 */
#include <sys/types.h>


/* stddef.h
 *
 * http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/stddef.h.html
 * http://www.gnu.org/software/gnulib/manual/html_node/stddef_002eh.html
 */
#if defined(__cplusplus)
#include <cstddef>
#else
#include <stddef.h>
#endif


/* stdint.h
 *
 * stdint.h provides width-based integral types (int8_t, int16_t, int32_t, etc)
 * inttypes.h more portable than stdint.h; stdint.h introduced with C99, C++11
 * inttypes.h includes stdint.h and additionally provides printf,scanf modifiers
 * (inttypes.h included as fallback when stdint.h not detected as available.
 *  Callers should include stdio.h,inttypes.h or cstdio,cinttypes for printf
 *  and scanf with width-based integral types)
 * http://www.opengroup.org/onlinepubs/9699919799/basedefs/stdint.h.html
 * http://www.opengroup.org/onlinepubs/9699919799/basedefs/inttypes.h.html
 * http://www.gnu.org/software/gnulib/manual/html_node/stdint_002eh.html
 * http://www.gnu.org/software/gnulib/manual/html_node/inttypes_002eh.html
 *
 * MS Visual Studio does not provide stdint.h until VS 2010
 * When using older MSC, caller will need to provide inttypes.h or stdint.h:
 * http://code.google.com/p/msinttypes/     (3-clause BSD license)
 * http://snipplr.com/view/18199/stdinth/   (public domain stdint.h)
 *
 * MSC-specific width-based integral types
 *   http://msdn.microsoft.com/en-us/library/29dh1w7z%28VS.80%29.aspx
 */
#if defined(__STDC_VERSION__) && __STDC_VERSION__-0 >= 199901L /* C99 */
#include <stdint.h>
#elif defined(__cplusplus) && __cplusplus >= 201103L /* C++11 */
#include <cstdint>
#elif defined(_MSC_VER) && _MSC_VER-0 >= 1600 /* MS Visual Studio 2010 */
#include <stdint.h>     /* (MS VS does not provide inttypes.h (as of VS 2012) */
#else
#include <inttypes.h>   /* (<inttypes.h> is more portable to pre-C99 systems) */
#endif


/* stdalign.h
 *
 * C11 <stdalign.h> and C++11 <cstdalign>
 * http://www.gnu.org/software/gnulib/manual/html_node/stdalign_002eh.html
 * quoting from above link to gnulib manual:
 * Not in POSIX yet, but we expect it will be. ISO C11 (latest free draft http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf) sections 6.5.3.4, 6.7.5, 7.15. C++11 (latest free draft http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2011/n3242.pdf) section 18.10.
 */
#if defined(__STDC_VERSION__) && __STDC_VERSION__-0 >= 201112L /* C11 */
#include <stdalign.h>
#elif defined(__cplusplus) && __cplusplus >= 201103L /* C++11 */
#include <cstdalign>
#ifndef __alignas_is_defined
#define __alignas_is_defined 1
#endif
#ifndef __alignof_is_defined
#define __alignof_is_defined 1
#endif
#endif

/* stdbool.h
 *
 * http://pubs.opengroup.org/onlinepubs/007904875/basedefs/stdbool.h.html
 * http://www.opengroup.org/onlinepubs/9699919799/basedefs/stdbool.h.html
 * http://www.gnu.org/software/gnulib/manual/html_node/stdbool_002eh.html
 * (workaround Solaris stdbool.h #error if __cplusplus or not C99 compilation)
 */

#if (defined(__STDC_VERSION__) && __STDC_VERSION__-0 >= 199901L) /* C99 */ \
 || !defined(__sun) /* (Solaris is alone in having an unfriendly stdbool.h) */

#if defined(bool) && !defined(__bool_true_false_are_defined)
#undef bool
#endif

#include <stdbool.h>

#elif defined(__cplusplus) && __cplusplus >= 201103L /* C++11 */

#include <cstdbool>

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
