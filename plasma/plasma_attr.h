/*
 * plasma_attr - portability macros for compiler-specific code attributes
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

#ifndef INCLUDED_PLASMA_ATTR_H
#define INCLUDED_PLASMA_ATTR_H

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(__STDC_C99) && !defined(__C99_RESTRICT)
/*(__C99_RESTRICT defined in xlc/xlC -qkeyword=restrict or some -qlanglevel= )*/
/* msvc: http://msdn.microsoft.com/en-us/library/5ft82fed.aspx */
#ifndef restrict
#if defined(__GNUC__) || defined(__clang__) || defined(_MSC_VER)
#define restrict __restrict
#else
#define restrict
#endif
#endif
#endif

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
#if !defined(__GNUC__) || defined(__GNUC_STDC_INLINE__) || defined(__clang__)
#  ifdef _MSC_VER
#  define C99INLINE __inline
#  else
#  define C99INLINE inline
#  endif
#else /* (GCC extern inline was prior to C99 standard; gcc 4.3+ supports C99) */
#  define C99INLINE extern inline
#endif
#endif
#endif


/*
 * http://gcc.gnu.org/onlinedocs/gcc/Function-Attributes.html
 * http://gcc.gnu.org/onlinedocs/gcc/Attribute-Syntax.html
 * /usr/include/sys/cdefs.h (GNU/Linux systems)
 * http://ohse.de/uwe/articles/gcc-attributes.html
 *
 * Note: below is not an exhaustive list of attributes
 * Attributes exist at above URLs that are not listed below
 *
 * http://clang.llvm.org/docs/LanguageExtensions.html
 */

#ifdef __linux__
#include <features.h>
#endif

#ifndef __has_attribute       /* clang */
#define __has_attribute(x) 0
#endif

#ifndef __has_builtin         /* clang */
#define __has_builtin(x) 0
#endif

#ifndef __has_feature         /* clang */
#define __has_feature(x) 0
#endif
#ifndef __has_extension       /* clang */
#define __has_extension(x)  __has_feature(x)
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
 || defined(__xlc__) || defined(__xlC__) /* IBM AIX xlC */ \
 || __has_attribute(noinline)
#ifndef __attribute_noinline__
#define __attribute_noinline__  __attribute__((noinline))
#endif
#endif
#ifndef __attribute_noinline__
#define __attribute_noinline__
#endif

#if (defined(__GNUC__) && __GNUC_PREREQ(3,1)) \
 || defined(__xlc__) || defined(__xlC__) /* IBM AIX xlC */ \
 || __has_attribute(always_inline)
#ifndef __attribute_always_inline__
#define __attribute_always_inline__  __attribute__((always_inline))
#endif
#elif defined(MSC_VER)
/* http://msdn.microsoft.com/en-us/library/z8y1yy88.aspx */
#ifndef __attribute_always_inline__
#define __attribute_always_inline__  __forceinline
#endif
#endif
#ifndef __attribute_always_inline__
#define __attribute_always_inline__  C99INLINE
#endif

#if __GNUC_PREREQ(2,5) \
 || defined(__xlc__) || defined(__xlC__) /* IBM AIX xlC */ \
 || defined(__HP_cc) || defined(__HP_aCC) \
 || __has_attribute(noreturn)
#ifndef __attribute_noreturn__
#define __attribute_noreturn__  __attribute__((noreturn))
#endif
#endif
#ifndef __attribute_noreturn__
#define __attribute_noreturn__
#endif

#if __GNUC_PREREQ(3,3) \
 || __has_attribute(nonnull)
#ifndef __attribute_nonnull__
#define __attribute_nonnull__  __attribute__((nonnull))
#endif
#endif
#ifndef __attribute_nonnull__
#define __attribute_nonnull__
#endif

#if __GNUC_PREREQ(3,3) \
 || __has_attribute(nonnull)
#ifndef __attribute_nonnull_x__
#define __attribute_nonnull_x__(x)  __attribute__((nonnull x))
#endif
#endif
#ifndef __attribute_nonnull_x__
#define __attribute_nonnull_x__(x)
#endif

#if __GNUC_PREREQ(2,96) \
 || defined(__HP_cc)|| defined(__HP_aCC) \
 || __has_attribute(malloc)
#ifndef __attribute_malloc__
#define __attribute_malloc__  __attribute__((malloc))
#endif
#endif
#ifndef __attribute_malloc__
#define __attribute_malloc__
#endif

#if __GNUC_PREREQ(2,96) \
 || defined(__xlc__) || defined(__xlC__) /* IBM AIX xlC */ \
 || __has_attribute(pure)
#ifndef __attribute_pure__
#define __attribute_pure__  __attribute__((pure))
#endif
#elif defined(__HP_cc)|| defined(__HP_aCC)
#ifndef __attribute_pure__
#define __attribute_pure__  __attribute__((non_exposing))
#endif
#endif
#ifndef __attribute_pure__
#define __attribute_pure__
#endif

#if __GNUC_PREREQ(4,3) \
 || __has_attribute(hot)
#ifndef __attribute_hot__
#define __attribute_hot__  __attribute__((hot))
#endif
#endif
#ifndef __attribute_hot__
#define __attribute_hot__
#endif

#if __GNUC_PREREQ(4,3) \
 || __has_attribute(cold)
#ifndef __attribute_cold__
#define __attribute_cold__  __attribute__((cold))
#endif
#endif
#ifndef __attribute_cold__
#define __attribute_cold__
#endif

#if __GNUC_PREREQ(2,96) \
 || __has_attribute(unused)
#ifndef __attribute_unused__
#define __attribute_unused__  __attribute__((unused))
#endif
#endif
#ifndef __attribute_unused__
#define __attribute_unused__
#endif

#if __GNUC_PREREQ(3,3) \
 || __has_attribute(nothrow)
#ifndef __attribute_nothrow__
#define __attribute_nothrow__  __attribute__((nothrow))
#endif
#endif
#ifndef __attribute_nothrow__
#define __attribute_nothrow__
#endif

#if __GNUC_PREREQ(3,4) \
 || defined(__HP_cc)|| defined(__HP_aCC) \
 || __has_attribute(warn_unused_result)
#ifndef __attribute_warn_unused_result__
#define __attribute_warn_unused_result__  __attribute__((warn_unused_result))
#endif
#endif
#ifndef __attribute_warn_unused_result__
#define __attribute_warn_unused_result__
#endif

#if (defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))) \
 || __has_attribute(regparm)
#ifndef __attribute_regparm__
#define __attribute_regparm__(x)  __attribute__((regparm x))
#endif
#endif
#ifndef __attribute_regparm__
#define __attribute_regparm__(x)
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

#if __GNUC_PREREQ(4,0) || __has_attribute(visibility)
# define EXPORT            __attribute__((visibility("default")))
# define HIDDEN            __attribute__((visibility("hidden")))
# define INTERNAL          __attribute__((visibility("internal")))
#elif defined(__SUNPRO_C) && (__SUNPRO_C >= 0x550)
# define EXPORT            __global
# define HIDDEN            __hidden
# define INTERNAL          __hidden
#elif defined(__HP_cc) || defined(__HP_aCC)
# define EXPORT            __attribute__((visibility("default")))
# define HIDDEN            __attribute__((visibility("hidden")))
# define INTERNAL          __attribute__((visibility("protected")))
#else /* not gcc >= 4 and not Sun Studio >= 8 and not HP aCC */
# define EXPORT
# define HIDDEN
# define INTERNAL
#endif

#if defined(__GNUC__) \
 &&(defined(__sun) || defined(_AIX) || defined(__hpux) || defined(__CYGWIN__))
# undef  EXPORT
# undef  HIDDEN
# undef  INTERNAL
# define EXPORT
# define HIDDEN
# define INTERNAL
#endif


/* GCC __builtin_expect() is used below to hint to compiler expected results
 * of commands executed.  Successful execution is expected and should be
 * optimally scheduled and predicted as branch taken.  Error conditions are
 * not expected and should be scheduled as the less likely branch taken.
 * (__builtin_expect() is recognized by IBM xlC compiler) */

#if !__GNUC_PREREQ(2,96) \
 && !(defined(__xlc__) || defined(__xlC__)) \
 && !__has_builtin(__builtin_expect)
#ifndef __builtin_expect
#define __builtin_expect(x,y) (x)
#endif
#endif

/* GCC __builtin_prefetch() http://gcc.gnu.org/projects/prefetch.html */
#if !defined(__GNUC__) && !__has_builtin(__builtin_prefetch)
/* xlC __dcbt() http://publib.boulder.ibm.com/infocenter/comphelp/v111v131/
 * (on Power7, might check locality and use __dcbtt() or __dcbstt())*/
#if defined(__xlc__) || defined(__xlC__)
#define __builtin_prefetch(addr,rw,locality) \
        ((rw) ? __dcbst((void *)(addr)) : __dcbt((void *)(addr)))
#endif
/* Sun Studio
 * http://blogs.oracle.com/solarisdev/entry/new_article_prefetching
 * (other prefetch options are available, but not mapped to locality here) */
#if defined(__SUNPRO_C) || defined(__SUNPRO_CC)
#include <sun_prefetch.h>
#define __builtin_prefetch(addr,rw,locality) \
        ((rw) ? sun_prefetch_write_once((void *)(addr)) \
              : sun_prefetch_read_once((void *)(addr)))
#endif
/* HP aCC for IA-64 (Itanium)
 * Search internet "Inline assembly for Itanium-based HP-UX"
 * (HP compiler expects to see string _LFHINT_* and not integral constants)*/
#if defined(__ia64) && (defined(__HP_cc) || defined(__HP_aCC))
#include <fenv.h>
#include <machine/sys/inline.h>
#define __builtin_prefetch(addr,rw,locality)                        \
        ((rw) ? _Asm_lfetch_excl(_LFTYPE_NONE,                      \
                                   (locality) == 0 ? _LFHINT_NTA  : \
                                   (locality) == 1 ? _LFHINT_NT2  : \
                                   (locality) == 2 ? _LFHINT_NT1  : \
                                                     _LFHINT_NONE,  \
                                 (void *)(addr))                    \
              : _Asm_lfetch(     _LFTYPE_NONE,                      \
                                   (locality) == 0 ? _LFHINT_NTA  : \
                                   (locality) == 1 ? _LFHINT_NT2  : \
                                   (locality) == 2 ? _LFHINT_NT1  : \
                                                     _LFHINT_NONE,  \
                                 (void *)(addr)))
#endif
/* default (do-nothing macros) */
#ifndef __builtin_prefetch
#define __builtin_prefetch(addr,rw,locality)
#endif
#endif


#define retry_eintr_while(x) \
  /*@-whileempty@*/                 /* caller must #include <errno.h> */   \
  do {} while (__builtin_expect((x),0) && __builtin_expect(errno == EINTR, 1)) \
  /*@=whileempty@*/

#define retry_eintr_do_while(x,c)   /* caller must #include <errno.h> */   \
  do {(x);} while (__builtin_expect((c),0) && __builtin_expect(errno==EINTR, 1))

/* Interestingly, GNU Linux systems might provide similar macro
 * TEMP_FAILURE_RETRY(expression) in unistd.h when defined(_GNU_SOURCE) */


#ifdef __cplusplus
}
#endif

#endif
