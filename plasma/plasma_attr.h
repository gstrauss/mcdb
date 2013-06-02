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

#if __STDC_VERSION__-0 < 199901L \
 && !defined(__STDC_C99) && !defined(__C99_RESTRICT)
/*(__C99_RESTRICT defined in xlc/xlC -qkeyword=restrict or some -qlanglevel= )*/
/* msvc: http://msdn.microsoft.com/en-us/library/5ft82fed.aspx */
#ifndef restrict
#if defined(__GNUC__) || defined(__clang__) || defined(_MSC_VER)
#define restrict __restrict
#elif defined(__SUNPRO_C)
#define restrict _Restrict
#else
#define restrict
#endif
#endif
#endif

#if defined(__SUNPRO_C) || defined(__SUNPRO_CC)
#ifndef __asm__
#define __asm__ __asm
#endif
#ifndef __volatile__
#define __volatile__ volatile
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

#if !__GNUC_PREREQ(2,8) && !defined(__clang__)
#define __extension__
#endif

#if __GNUC_PREREQ(3,1) \
 || defined(__xlc__) || defined(__xlC__) /* IBM AIX xlC */ \
 || __has_attribute(noinline)
#ifndef __attribute_noinline__
#define __attribute_noinline__  __attribute__((noinline))
#endif
#endif
#ifndef __attribute_noinline__
#define __attribute_noinline__
#endif

#if __GNUC_PREREQ(3,1) \
 || defined(__xlc__) || defined(__xlC__) /* IBM AIX xlC */ \
 || __has_attribute(always_inline)
#ifndef __attribute_always_inline__
#define __attribute_always_inline__  __attribute__((always_inline))
#endif
#elif defined(_MSC_VER)
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


#if defined(__sun) && defined(__SVR4)
#include <sys/isa_defs.h>  /* needed on Solaris for _LP64 or _ILP32 define */
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

#if defined(__clang__) || defined(INTEL_COMPILER) || defined(_MSC_VER) \
 ||(__GNUC_PREREQ(4,7) && defined(__SSE__))
#include <xmmintrin.h>
#else
enum _mm_hint
{
  _MM_HINT_T0 =  3,
  _MM_HINT_T1 =  2,
  _MM_HINT_T2 =  1,
  _MM_HINT_NTA = 0
};
#endif

/* GCC __builtin_prefetch() http://gcc.gnu.org/projects/prefetch.html */
#if !defined(__GNUC__) && !__has_builtin(__builtin_prefetch)
/* http://software.intel.com/sites/products/documentation/studio/composer/en-us/2011Update/compiler_c/intref_cls/common/intref_sse_cacheability.htm
 * http://msdn.microsoft.com/en-us/library/84szxsww%28v=vs.90%29.aspx
 * On non-x86 systems, MSVC provides PreFetchCacheLine() (not used here)
 *   void PreFetchCacheLine(int Level, VOID CONST *Address);
 *   #define __builtin_prefetch(addr,rw,locality) \
 *           PreFetchCacheLine((locality),(addr))
 * http://msdn.microsoft.com/en-us/library/windows/desktop/ms684826%28v=vs.85%29.aspx */
#if defined(__INTEL_COMPILER) || defined(_MSC_VER)
#ifdef _MSC_VER
#pragma intrinsic(_mm_prefetch)
#endif
#define __builtin_prefetch(addr,rw,locality) \
        _mm_prefetch((addr),(locality))
#endif
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
                                 /*(locality) == 3*/ _LFHINT_NONE,  \
                                 (void *)(addr))                    \
              : _Asm_lfetch(     _LFTYPE_NONE,                      \
                                   (locality) == 0 ? _LFHINT_NTA  : \
                                   (locality) == 1 ? _LFHINT_NT2  : \
                                   (locality) == 2 ? _LFHINT_NT1  : \
                                 /*(locality) == 3*/ _LFHINT_NONE,  \
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


/* __typeof__() is non-standard, though available on many modern compilers
 * http://en.wikipedia.org/wiki/Typeof
 *
 * gcc and clang support __typeof__()
 *
 * Sun Studio 12 C compiler supports __typeof__ extension
 * http://www.oracle.com/technetwork/systems/cccompare-137792.html
 * http://www.oracle.com/technetwork/server-storage/solaris10/c-type-137127.html
 * http://docs.oracle.com/cd/E19205-01/820-4155/c++.html
 * https://blogs.oracle.com/sga/entry/typeof_and_alignof
 * Sun Studio C++:
 *   Keyword typeof is available under -features=gcc command line option.
 *
 * xlC supports __typeof__()
 * The typeof and __typeof__ keywords are supported as follows:
 *     C only The __typeof__ keyword is recognized under compilation with the xlc invocation command or the -qlanglvl=extc89, -qlanglvl=extc99, or -qlanglvl=extended options. The typeof keyword is only recognized under compilation with -qkeyword=typeof.
 *     C++ The typeof and __typeof__ keywords are recognized by default.
 * http://publib.boulder.ibm.com/infocenter/compbgpl/v9v111/index.jsp?topic=/com.ibm.xlcpp9.bg.doc/language_ref/typeof_operator.htm
 *
 * HP-UX cc supports __typeof__ when compiled with -Agcc
 * http://h30499.www3.hp.com/t5/Languages-and-Scripting/HP-UX-IA64-and-HP-C-Is-quot-typeof-quot-keyword-supported/td-p/4529218
 *
 * MS Visual Studio 2010 decltype() can be used to implement __typeof__()
 * http://msdn.microsoft.com/en-us/library/dd537655.aspx
 * http://stackoverflow.com/questions/70013/how-to-detect-if-im-compiling-code-with-visual-studio-2008
 */
#if defined(_MSC_VER) && _MSC_VER >= 1600
#define __typeof__(x)  decltype(x)
#endif


#ifdef __cplusplus
}
#endif

#endif
