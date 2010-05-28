/* TODO: portability to non-GCC compilers and earlier versions of GCC */

#ifndef __GNUC__
#define __builtin_expect(x,y) (x)
#endif

#ifdef __linux
#include <features.h>
#endif

/*
 * Discussion of nuances of "extern inline" functions in C:
 *   http://www.greenend.org.uk/rjk/2003/03/inline.html
 *   http://gcc.gnu.org/onlinedocs/gcc/Inline.html#Inline
 */

/*
 http://gcc.gnu.org/onlinedocs/gcc/Function-Attributes.html#Function-Attributes
 */

#if defined(__GNUC__) \
 || defined(__xlc__) || defined(__xlC__) /* IBM AIX xlC */
#ifndef __attribute_noinline__
#define __attribute_noinline__  __attribute__((noinline))
#endif
#endif
#ifndef __attribute_noinline__
#define __attribute_noinline__
#endif

#ifdef  __GNUC__
#ifndef __attribute_nonnull__
#define __attribute_nonnull__  __attribute__((nonnull))
#endif
#endif
#ifndef __attribute_nonnull__
#define __attribute_nonnull__
#endif

#ifdef  __GNUC__
#ifndef __attribute_nonnull_x__
#define __attribute_nonnull_x__(x)  __attribute__((nonnull(x)))
#endif
#endif
#ifndef __attribute_nonnull_x__
#define __attribute_nonnull_x__(x)
#endif

#ifdef  __GNUC__
#ifndef __attribute_malloc__
#define __attribute_malloc__  __attribute__((malloc))
#endif
#endif
#ifndef __attribute_malloc__
#define __attribute_malloc__
#endif

#if (defined(__GNUC_PREREQ) && __GNUC_PREREQ(2,96)) \
 || defined(__xlc__) || defined(__xlC__) /* IBM AIX xlC */
#ifndef __attribute_pure__
#define __attribute_pure__  __attribute__((pure))
#endif
#endif
#ifndef __attribute_pure__
#define __attribute_pure__
#endif

#if defined(__GNUC_PREREQ) && __GNUC_PREREQ(4,3)
#ifndef __attribute_cold__
#define __attribute_cold__  __attribute__((cold))
#endif
#endif
#ifndef __attribute_cold__
#define __attribute_cold__
#endif

#ifdef  __GNUC__
#ifndef __attribute_unused__
#define __attribute_unused__  __attribute__((unused))
#endif
#endif
#ifndef __attribute_unused__
#define __attribute_unused__
#endif

#ifdef  __GNUC__
#ifndef __attribute_warn_unused_result__
#define __attribute_warn_unused_result__  __attribute__((warn_unused_result))
#endif
#endif
#ifndef __attribute_warn_unused_result__
#define __attribute_warn_unused_result__
#endif
