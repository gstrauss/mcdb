/*
 * code_attributes - portability macros for compiler-specific code attributes
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

#ifndef INCLUDED_CODE_ATTRIBUTES_H
#define INCLUDED_CODE_ATTRIBUTES_H


/*
 * C inline functions defined in header
 * Discussion of nuances of "extern inline" and inline functions in C headers:
 *   http://www.greenend.org.uk/rjk/2003/03/inline.html
 *   http://gcc.gnu.org/onlinedocs/gcc/Inline.html#Inline
 */
#ifdef NO_C99INLINE
#define C99INLINE
#else
#ifndef C99INLINE
#if !defined(__GNUC__) || defined(__GNUC_STDC_INLINE__)
#define C99INLINE inline
#else /* (GCC extern inline was prior to C99 standard; gcc 4.3+ supports C99) */
#define C99INLINE extern inline
#endif
#endif
#endif


/*
 * http://gcc.gnu.org/onlinedocs/gcc/Function-Attributes.html
 * /usr/include/sys/cdefs.h (GNU/Linux systems)
 *
 * Note: below not an exhaustive list of attributes
 * Attributes exist at above URL that are not listed below
 */

#ifdef __linux
#include <features.h>
#endif

#ifndef __GNUC_PREREQ
#  ifdef __GNUC_PREREQ__
#    define __GNUC_PREREQ __GNUC_PREREQ__
#  elif defined __GNUC__ && defined __GNUC_MINOR__
#    define __GNUC_PREREQ(maj, min) \
       ((__GNUC__ << 16) + __GNUC_MINOR__ >= ((maj) << 16) + (min))
#  else
#    define __GNUC_PREREQ(maj, min) 0
#  endif
#endif

#if defined(__GNUC__) && !__GNUC_PREREQ(2,7)
#define __attribute__(x)
#endif

#if (defined(__GNUC__) && __GNUC_PREREQ(3,1)) \
 || defined(__xlc__) || defined(__xlC__) /* IBM AIX xlC */
#ifndef __attribute_noinline__
#define __attribute_noinline__  __attribute__((noinline))
#endif
#endif
#ifndef __attribute_noinline__
#define __attribute_noinline__
#endif

#if (defined(__GNUC__) && __GNUC_PREREQ(2,5)) \
 || defined(__xlc__) || defined(__xlC__) /* IBM AIX xlC */
#ifndef __attribute_noreturn__
#define __attribute_noreturn__  __attribute__((noreturn))
#endif
#endif
#ifndef __attribute_noreturn__
#define __attribute_noreturn__
#endif

#if defined(__GNUC__) && __GNUC_PREREQ(3,3)
#ifndef __attribute_nonnull__
#define __attribute_nonnull__  __attribute__((nonnull))
#endif
#endif
#ifndef __attribute_nonnull__
#define __attribute_nonnull__
#endif

#if defined(__GNUC__) && __GNUC_PREREQ(3,3)
#ifndef __attribute_nonnull_x__
#define __attribute_nonnull_x__(x)  __attribute__((nonnull x))
#endif
#endif
#ifndef __attribute_nonnull_x__
#define __attribute_nonnull_x__(x)
#endif

#if defined(__GNUC__) && __GNUC_PREREQ(2,96)
#ifndef __attribute_malloc__
#define __attribute_malloc__  __attribute__((malloc))
#endif
#endif
#ifndef __attribute_malloc__
#define __attribute_malloc__
#endif

#if (defined(__GNUC__) && __GNUC_PREREQ(2,96)) \
 || defined(__xlc__) || defined(__xlC__) /* IBM AIX xlC */
#ifndef __attribute_pure__
#define __attribute_pure__  __attribute__((pure))
#endif
#endif
#ifndef __attribute_pure__
#define __attribute_pure__
#endif

#if defined(__GNUC__) && __GNUC_PREREQ(4,3)
#ifndef __attribute_hot__
#define __attribute_hot__  __attribute__((hot))
#endif
#endif
#ifndef __attribute_hot__
#define __attribute_hot__
#endif

#if defined(__GNUC__) && __GNUC_PREREQ(4,3)
#ifndef __attribute_cold__
#define __attribute_cold__  __attribute__((cold))
#endif
#endif
#ifndef __attribute_cold__
#define __attribute_cold__
#endif

#if defined(__GNUC__) && __GNUC_PREREQ(2,96)
#ifndef __attribute_unused__
#define __attribute_unused__  __attribute__((unused))
#endif
#endif
#ifndef __attribute_unused__
#define __attribute_unused__
#endif

#if defined(__GNUC__) && __GNUC_PREREQ(3,4)
#ifndef __attribute_warn_unused_result__
#define __attribute_warn_unused_result__  __attribute__((warn_unused_result))
#endif
#endif
#ifndef __attribute_warn_unused_result__
#define __attribute_warn_unused_result__
#endif


/*
 * Symbol visibility attributes:
 * http://docs.sun.com/source/820-4155/c.html
 *   (Oracle/Sun Studio 12 C compiler supports __attribute__
 *    This is not applied in code below; todo)
 * http://wikis.sun.com/display/SunStudio/Using+Sun+Studio+for+open+source+apps
 * Note that GCC allows these at the end of function declarations,
 * but Sun Studio requires them at the beginning, such as:
 * EXPORT void foo(int bar);
 */

#if defined(__GNUC__) && (__GNUC__ >= 4)
# define EXPORT		__attribute__((visibility("default")))
# define HIDDEN		__attribute__((visibility("hidden")))
# define INTERNAL	__attribute__((visibility("internal")))
#elif defined(__SUNPRO_C) && (__SUNPRO_C >= 0x550)
# define EXPORT		__global
# define HIDDEN		__hidden
# define INTERNAL	__hidden
#else /* not gcc >= 4 and not Sun Studio >= 8 */
# define EXPORT
# define HIDDEN
# define INTERNAL
#endif


/* GCC __builtin_expect() is used below to hint to compiler expected results
 * of commands executed.  Successful execution is expected and should be
 * optimally scheduled and predicted as branch taken.  Error conditions are
 * not expected and should be scheduled as the less likely branch taken.
 * (__builtin_expect() is recognized by IBM xlC compiler) */

#if !(defined(__GNUC__) && __GNUC_PREREQ(2,96)) \
 && !(defined(__xlc__) || defined(__xlC__))
#ifndef __builtin_expect
#define __builtin_expect(x,y) (x)
#endif
#endif

/* GCC __builtin_prefetch() http://gcc.gnu.org/projects/prefetch.html */
#if !defined(__GNUC__)
/* xlC __dcbt() http://publib.boulder.ibm.com/infocenter/comphelp/v111v131/
 * (on Power7, might check locality and use __dcbtt() or __dcbstt())*/
#if defined(__xlc__) || defined(__xlC__)
#define __builtin_prefetch(addr,rw,locality) \
        ((rw) ? __dcbst(addr) : __dcbt(addr))
#endif
/* Sun Studio
 * http://blogs.oracle.com/solarisdev/entry/new_article_prefetching
 * (other prefetch options are available, but not mapped to locality here) */
#if defined(__SUNPRO_C) || defined(__SUNPRO_CC)
#include <sun_prefetch.h>
#define __builtin_prefetch(addr,rw,locality) \
        ((rw) ? sun_prefetch_write_once(addr) : sun_prefetch_read_once(addr))
#endif
/* HP aCC for IA-64 (Itanium)
 * Search internet "Inline assembly for Itanium-based HP-UX"
 * _Asm_lfhint competers appear to be reverse those of gcc __builtin_prefetch */
#if defined(__ia64) && (defined(__HP_cc) || defined(__HP_aCC))
#include <fenv.h>
#include <machine/sys/inline.h>
#define __builtin_prefetch(addr,rw,locality) \
        ((rw) ? _Asm_lfetch_excl(_LFTYPE_NONE, -(locality)+3, (addr)) \
              : _Asm_lfetch(_LFTYPE_NONE, -(locality)+3, (addr)))
#endif
/* default (do-nothing macros) */
#ifndef __builtin_prefetch
#define __builtin_prefetch(addr,rw,locality)
#endif
#endif


#define retry_eintr_while(x) \
  /*@-whileempty@*/                 /* caller must #include <errno.h> */   \
  while (__builtin_expect((x),0) && __builtin_expect(errno == EINTR, 1)) ; \
  /*@=whileempty@*/

#define retry_eintr_do_while(x,c)         /* caller must #include <errno.h> */ \
  do {(x);} while (__builtin_expect((c),0) && __builtin_expect(errno==EINTR, 1))


#endif
