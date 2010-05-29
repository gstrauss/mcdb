#ifndef INCLUDED_CODE_ATTRIBUTES_H
#define INCLUDED_CODE_ATTRIBUTES_H


/*
 * C inline functions defined in header
 * Discussion of nuances of "extern inline" and inline functions in C headers:
 *   http://www.greenend.org.uk/rjk/2003/03/inline.html
 *   http://gcc.gnu.org/onlinedocs/gcc/Inline.html#Inline
 */
#ifndef C99INLINE
#if !defined(__GNUC__) || defined(__GNUC_STDC_INLINE__)
#define C99INLINE inline
#else /* (GCC extern inline was prior to C99 standard; gcc 4.3+ supports C99) */
#define C99INLINE extern inline
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
#define __attribute_nonnull_x__(x)  __attribute__((nonnull(x)))
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
 * not expected and should be scheduled as the less likely branch taken. */

#if !defined(__GNUC__) || !__GNUC_PREREQ(2,96)
#ifndef __builtin_expect
#define __builtin_expect(x,y) (x)
#endif
#endif

#define retry_eintr(x) \
  /*@-whileempty@*/ \
  while (__builtin_expect((x),0) && __builtin_expect(errno == EINTR, 1)) ; \
  /*@=whileempty@*/


#endif
