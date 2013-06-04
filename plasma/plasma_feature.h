/*
 * plasma_feature - portability macros for OS and architecture features
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

/*
 * Recommended use:
 *   #include <plasma_feature.h> first in each header (for best results),
 *   and for each component, #include <sample.h> as first header in sample.c.
 * Prefer to define feature macros at top of .c or .cpp, and not in headers
 *   (except for documentation and standalone header compilation testing)
 *   since order of header inclusion depends on order specified by consumer.
 */

#ifndef INCLUDED_PLASMA_FEATURE_H
#define INCLUDED_PLASMA_FEATURE_H


#ifndef PLASMA_FEATURE_DISABLE_BSD_SOURCE_TO_DARWIN_C_SOURCE
#ifndef PLASMA_FEATURE_ENABLE_BSD_SOURCE_TO_DARWIN_C_SOURCE
#define PLASMA_FEATURE_ENABLE_BSD_SOURCE_TO_DARWIN_C_SOURCE
#endif
#endif
#ifdef PLASMA_FEATURE_ENABLE_BSD_SOURCE_TO_DARWIN_C_SOURCE
#if defined(_BSD_SOURCE) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE
#endif
#endif


#ifndef PLASMA_FEATURE_ENABLE_WIN32_FULLHDRS
#ifndef PLASMA_FEATURE_DISABLE_WIN32_FULLHDRS
#define PLASMA_FEATURE_DISABLE_WIN32_FULLHDRS
#endif
#endif
#ifdef PLASMA_FEATURE_DISABLE_WIN32_FULLHDRS /* exclude extra hdrs by default */
/* How to use VC_EXTRALEAN and WIN32_LEAN_AND_MEAN to enhance the build process in Visual C++
 * http://support.microsoft.com/kb/166474
 * Using the Windows Headers
 * http://msdn.microsoft.com/en-us/library/windows/desktop/aa383745%28v=vs.85%29.aspx
 * (see above links for more selective exclusion using NO* macros (e.g. NOCOMM))
 * #include <plasma/plasma_feature.h> prior to #include <windows.h>
 * and then include select header files for the features needed,
 * e.g. #include <winsock2.h> for windows sockets */
#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#endif


/*
 * Defensive defines to avoid namespace pollution
 * Avoid definitions of 'major' 'minor' 'makedev' disk device macros in
 * BSD header sys/sysmacros.h, as well as possibly other non-standard macros
 * (convenience macros provided differ between systems)
 * (sys/param.h includes sys/sysmacros.h on some systems)
 * (sys/param.h included by sys/types.h on some systems (e.g. HP-UX))
 * (Attempt to avoid sys/param.h since some systems (e.g. Darwin, AIX)
 *  define convenience macros in sys/param.h, but this means we can not
 *  include sys/param.h and then check defined(BSD) to discover *BSD systems
 *  (as is recommended in the FreeBSD Porter's Handbook))
 * (Darwin defines 'major' 'minor' 'makedev' in sys/types.h, but only if
 *  _DARWIN_C_SOURCE is defined or if _POSIX_C_SOURCE is not defined)
 */
#ifndef PLASMA_FEATURE_ENABLE_SYS_SYSMACROS
#ifndef PLASMA_FEATURE_DISABLE_SYS_SYSMACROS
#define PLASMA_FEATURE_DISABLE_SYS_SYSMACROS
#endif
#endif
#ifdef PLASMA_FEATURE_DISABLE_SYS_SYSMACROS
#if   defined(__linux__)
#ifndef _SYS_SYSMACROS_H
#define _SYS_SYSMACROS_H
#endif
#elif defined(__sun) && defined(__SVR4)
#ifndef _SYS_SYSMACROS_H
#define _SYS_SYSMACROS_H
#endif
#elif defined(__hpux)
#ifndef _SYS_SYSMACROS_INCLUDED
#define _SYS_SYSMACROS_INCLUDED
#endif
#elif defined(_AIX)
#ifndef _H_SYSMACROS
#define _H_SYSMACROS
#endif
#endif
#endif


#if defined(__sun) && defined(__SVR4)
#include <sys/isa_defs.h>  /* needed on Solaris for _LP64 or _ILP32 define */
#endif                     /* ok to include prior to setting feature macros */
#if defined(__i386) && !defined(__i386__)
#define __i386__
#endif
#if defined(__amd64) && !defined(__x86_64__)
#define __x86_64__
#endif


/*
 * large file support (> 2 GiB - 1)
 * enable largefile support by default (unless plasma macro is set to disable)
 * expose largefile64 typtes and interfaces (e.g. lseek64()) only if requested
 * http://opengroup.org/platform/lfs.html
 * http://www.unix.org/version2/whatsnew/lfs20mar.html
 */
#ifndef PLASMA_FEATURE_DISABLE_LARGEFILE
#ifndef PLASMA_FEATURE_ENABLE_LARGEFILE
#define PLASMA_FEATURE_ENABLE_LARGEFILE
#endif
#endif
#ifdef PLASMA_FEATURE_ENABLE_LARGEFILE
#if defined(__linux__) /* man standards, man feature_test_macros */ \
 || defined(__sun) /*man standards, man lf64, man lfcompile, man lfcompile64*/ \
 || defined(__hpux)
  #ifndef _FILE_OFFSET_BITS
  #define _FILE_OFFSET_BITS    64
  #endif
  #ifndef _LARGEFILE_SOURCE
  #define _LARGEFILE_SOURCE     1
  #endif
  #ifdef PLASMA_FEATURE_ENABLE_LARGEFILE64 /* largefile64 types, interfaces */
    #ifndef _LARGEFILE64_SOURCE
    #define _LARGEFILE64_SOURCE 1
    #endif
  #endif
#elif (defined(__APPLE__) && defined(__MACH__)) \
   || defined(__FreeBSD__) || defined(__NetBSD__) \
   || defined(__OpenBSD__) || defined(__DragonflyBSD__)
  /* *BSD, MacOSX (Darwin) use 64-bit off_t in both 32-bit and 64-bit compile
   * (BSD 4.4 and later) */
  #ifdef PLASMA_FEATURE_ENABLE_LARGEFILE64 /* largefile64 types, interfaces */
    #ifndef _LARGEFILE64_SOURCE
    #define _LARGEFILE64_SOURCE 1
    #endif
  #endif
#elif defined(_AIX)
  #ifndef _LARGE_FILES
  #define _LARGE_FILES
  #endif
  #ifdef PLASMA_FEATURE_ENABLE_LARGEFILE64 /* largefile64 types, interfaces */
    #ifndef _LARGE_FILE_API
    #define _LARGE_FILE_API
    #endif
  #endif
#elif defined(_WIN32)  /* (ILP32 or LLP64 ABI; never defines LP64) */
  /* (omitted; Windows I/O interface is very different from that of Unix98) */
#endif
#endif


/*
 * Feature Macros enable standard functionality and/or disable extensions.
 * Feature Macros must be defined prior to including any headers to take effect.
 * _POSIX_C_SOURCE
 * _XOPEN_SOURCE
 * _SVID_SOURCE
 * _BSD_SOURCE
 * ... and many more  (XXX: add publicly available links to standards)
 *
 * system-specific feature headers might define macros used by subsequent tests
 * e.g. __GLIBC__ is defined in <features.h> on Linux
 * (see comments in following headers in addition to referenced man pages)
 */
#if defined(__linux__)                    /* man -s 7 standards */
#include <features.h>                     /* man -s 7 feature_test_macros */
#elif defined(__sun)
#include <sys/feature_tests.h>            /* man -s 5 standards */
#elif defined(_AIX)
#include <standards.h>
#elif (defined(__APPLE__) && defined(__MACH__))  /* man -s 5 compat */ \
   || defined(__FreeBSD__) || defined(__NetBSD__) \
   || defined(__OpenBSD__) || defined(__DragonflyBSD__)
#include <sys/cdefs.h>
#elif defined(__hpux)
/*#include <XXX>*/
#endif


/*
 * byte order - __BIG_ENDIAN__ or __LITTLE_ENDIAN__
 *
 * gcc and clang on Mac OSX define __BIG_ENDIAN__ or __LITTLE_ENDIAN__ to 1.
 * Provide similar define for other compilers and platforms.
 */

#if defined(__LITTLE_ENDIAN__)

#if __LITTLE_ENDIAN-0 < 1
#undef  __LITTLE_ENDIAN__
#define __LITTLE_ENDIAN__ 1
#endif

#elif defined(__BIG_ENDIAN__)

#if __BIG_ENDIAN-0 < 1
#undef  __BIG_ENDIAN__
#define __BIG_ENDIAN__ 1
#endif

#else /* !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__) */

#if defined(__BYTE_ORDER__)              \
 && (   defined(__ORDER_LITTLE_ENDIAN__) \
     || defined(__ORDER_BIG_ENDIAN__)    \
     || defined(__ORDER_PDP_ENDIAN__)   )
  #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  #define __LITTLE_ENDIAN__ 1
  #elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  #define __BIG_ENDIAN__ 1
  #endif
#elif defined(_LITTLE_ENDIAN) && !defined(_BIG_ENDIAN)
  #define __LITTLE_ENDIAN__ 1
#elif !defined(_LITTLE_ENDIAN) && defined(_BIG_ENDIAN)
  #define __BIG_ENDIAN__ 1
#elif defined(_WIN32) /* little endian on all current MS-supported platforms */
  #define __LITTLE_ENDIAN__ 1
#elif defined(__GLIBC__) || defined(__linux__)
  #include <endian.h>
  #if __BYTE_ORDER == __LITTLE_ENDIAN
  #define __LITTLE_ENDIAN__ 1
  #elif __BYTE_ORDER == __BIG_ENDIAN
  #define __BIG_ENDIAN__ 1
  #endif
#elif defined(__sun) && defined(__SVR4)
  #include <sys/isa_defs.h>
  #if defined(_LITTLE_ENDIAN)
  #define __LITTLE_ENDIAN__ 1
  #elif defined(_BIG_ENDIAN)
  #define __BIG_ENDIAN__ 1
  #endif
#elif defined(_AIX)
  #include <sys/machine.h>
  #if BYTE_ORDER == LITTLE_ENDIAN
  #define __LITTLE_ENDIAN__ 1
  #elif BYTE_ORDER == BIG_ENDIAN
  #define __BIG_ENDIAN__ 1
  #endif
#elif defined(__APPLE__) && defined(__MACH__)
  #include <machine/endian.h>
  #if BYTE_ORDER == LITTLE_ENDIAN
  #define __LITTLE_ENDIAN__ 1
  #elif BYTE_ORDER == BIG_ENDIAN
  #define __BIG_ENDIAN__ 1
  #endif
#elif defined(__FreeBSD__) || defined(__NetBSD__) \
   || defined(__OpenBSD__) || defined(__DragonflyBSD__)
  #include <machine/endian.h>
  #if _BYTE_ORDER == _LITTLE_ENDIAN
  #define __LITTLE_ENDIAN__ 1
  #elif _BYTE_ORDER == _BIG_ENDIAN
  #define __BIG_ENDIAN__ 1
  #endif
#else
  /* Fall back to set endian based on architecture, even though some chips are
   * bi-endian, supporting both little-endian and big-endian.  For example,
   * Itanium is run big endian under HP-UX and is run little endian under Linux
   * and Win64, so check defined(__hpux) instead of defined(__ia64__).  gcc sets
   * __ARMEL__ or __ARMEB__ on ARM for little-endian or big-endian, respectively
   * http://labs.hoffmanlabs.com/node/544 */
  #if defined(__x86_64) || defined(__x86_64__) \
   || defined(__amd64)  || defined(__amd64__)  \
   || defined(__i386)   || defined(__i386__)   \
   || defined(__x86)    || defined(__x86__)    \
   || defined(__ARMEL__)
  #define __LITTLE_ENDIAN__ 1
  #elif defined(__sparc)   || defined(__sparc__) \
     || defined(__ppc__)   || defined(_ARCH_PPC) \
     || defined(_ARCH_PWR) || defined(_ARCH_PWR2) || defined(_POWER) \
     || defined(__ARMEB__) || defined(__hpux)
  #define __BIG_ENDIAN__ 1
  #endif
#endif

#endif /* !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__) */

#if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__)
#error "unsupported byte order (not big or little endian, or not defined)"
#elif defined(__BIG_ENDIAN__) && defined(__LITTLE_ENDIAN__)
#error "both __BIG_ENDIAN__ and __LITTLE_ENDIAN__ defined"
#endif

/* gcc 4.6+ http://gcc.gnu.org/onlinedocs/cpp/Common-Predefined-Macros.html
 * http://publib.boulder.ibm.com/infocenter/macxhelp/v6v81/index.jsp?topic=%2Fcom.ibm.vacpp6m.doc%2Fcompiler%2Fref%2Frumacros.htm
 */

/* http://nadeausoftware.com/articles/2012/01/c_c_tip_how_use_compiler_predefined_macros_detect_operating_system */


/* __GLIBC__ defined in <features.h> in glibc headers on __linux__
 * __USE_STRING_INLINES needs to be defined prior to include <string.h> */
#ifdef PLASMA_FEATURE_ENABLE_GLIBC_STRING_INLINES
#if defined(__linux__) && defined(__GLIBC__) && !defined(__clang__)
#ifndef __USE_STRING_INLINES  /* see comments in glibc /usr/include/string.h */
#define __USE_STRING_INLINES
#endif
#endif
#endif


#endif
