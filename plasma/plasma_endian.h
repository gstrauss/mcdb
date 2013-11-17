/*
 * plasma_endian - portability macros for byteorder conversion
 *
 *   inline assembly and compiler intrinsics for byteorder conversion
 *   (little endian <=> big endian)
 *
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

#ifndef PLASMA_ENDIAN_H
#define PLASMA_ENDIAN_H

#include "plasma_feature.h"
#include "plasma_attr.h"
#include "plasma_stdtypes.h"
PLASMA_ATTR_Pragma_once

/*
 * byte order - __BIG_ENDIAN__ or __LITTLE_ENDIAN__
 *
 * gcc and clang on Mac OSX define __BIG_ENDIAN__ or __LITTLE_ENDIAN__ to 1.
 * Provide similar define for other compilers and platforms.
 */

#if defined(__LITTLE_ENDIAN__)

#if __LITTLE_ENDIAN__-0 < 1
#undef  __LITTLE_ENDIAN__
#define __LITTLE_ENDIAN__ 1
#endif

#elif defined(__BIG_ENDIAN__)

#if __BIG_ENDIAN__-0 < 1
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


/*
 * prefer OS-supplied macros from headers, which might employ compiler builtins,
 * and might provide code that can be constant-folded by optimizing compiler
 * when the argument value is constant and known at compile time, and might
 * otherwise emit optimized assembly.
 */

#if defined(__linux__) || defined(__GLIBC__)

#include <byteswap.h>
#define plasma_endian_swap16(x)   __bswap_16(x)
#define plasma_endian_swap32(x)   __bswap_32(x)
#define plasma_endian_swap64(x)   __bswap_64(x)
#define plasma_endian_swap16p(x)  __bswap_16(*(x))
#define plasma_endian_swap32p(x)  __bswap_32(*(x))
#define plasma_endian_swap64p(x)  __bswap_64(*(x))

#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonflyBSD__)

#include <machine/endian.h>
/* http://fxr.watson.org/fxr/source/sys/endian.h */
#define plasma_endian_swap16(x)   __bswap16(x)
#define plasma_endian_swap32(x)   __bswap32(x)
#define plasma_endian_swap64(x)   __bswap64(x)
#define plasma_endian_swap16p(x)  __bswap16(*(x))
#define plasma_endian_swap32p(x)  __bswap32(*(x))
#define plasma_endian_swap64p(x)  __bswap64(*(x))

#elif defined(__OpenBSD__)

/* http://fxr.watson.org/fxr/source/arch/i386/include/endian.h?v=OPENBSD */
#include <machine/endian.h>
#define plasma_endian_swap16(x)   __swap16(x)
#define plasma_endian_swap32(x)   __swap32(x)
#define plasma_endian_swap64(x)   __swap64(x)
#define plasma_endian_swap16p(x)  __swap16(*(x))
#define plasma_endian_swap32p(x)  __swap32(*(x))
#define plasma_endian_swap64p(x)  __swap64(*(x))

#elif defined(__APPLE__) && defined(__MACH__)

#include <libkern/OSByteOrder.h>
#define plasma_endian_swap16(x)   __DARWIN_OSSwapInt16(x)
#define plasma_endian_swap32(x)   __DARWIN_OSSwapInt32(x)
#define plasma_endian_swap64(x)   __DARWIN_OSSwapInt64(x)
#define plasma_endian_swap16p(x)  __DARWIN_OSSwapInt16(*(x))
#define plasma_endian_swap32p(x)  __DARWIN_OSSwapInt32(*(x))
#define plasma_endian_swap64p(x)  __DARWIN_OSSwapInt64(*(x))

#elif defined(_MSC_VER)

/* http://msdn.microsoft.com/en-us/library/5704bbxw%28v=vs.80%29.aspx */
/* http://msdn.microsoft.com/en-us/library/a3140177%28v=vs.80%29.aspx */
/*?#include <stdlib.h>?*/
#include <intrin.h>
#pragma intrinsic(_byteswap_ushort)
#pragma intrinsic(_byteswap_ulong)
#pragma intrinsic(_byteswap_uint64)
#define plasma_endian_swap16(x)   _byteswap_ushort(x)
#define plasma_endian_swap32(x)   _byteswap_ulong(x)
#define plasma_endian_swap64(x)   _byteswap_uint64(x)
#define plasma_endian_swap16p(x)  _byteswap_ushort(*(x))
#define plasma_endian_swap32p(x)  _byteswap_ulong(*(x))
#define plasma_endian_swap64p(x)  _byteswap_uint64(*(x))

#else  /* (no OS std lib endian support) */


#ifndef PLASMA_ENDIAN_C99INLINE
#define PLASMA_ENDIAN_C99INLINE C99INLINE
#endif
#ifndef NO_C99INLINE
#ifndef PLASMA_ENDIAN_C99INLINE_FUNCS
#define PLASMA_ENDIAN_C99INLINE_FUNCS
#endif
#endif

/* (expect 64-bit CPU; not providing support for 32-bit SPARC or POWER CPU)
 * (might need to pass compiler flags to indicate minimum compiler target arch
 *  when compiling 32-bit, e.g. xlc -xarch=pwr5)
 * (SPARC and POWER additionally have special instructions for byteorder swap
 *  and store, but an interface is not provided here to take value, store ptr)
 */

#if defined(__sparc)

/* Sun Studio 12 update 1 (5.10) supports extended inline asm syntax (like gcc).
 * Earlier versions support separate .il inline templates but limited inline asm
 * Solaris <sys/byteorder.h> on Solaris 10 provides unimpressive implementation;
 * even the non-assembly implementation in the inline funcs below is better
 * https://blogs.oracle.com/DanX/entry/optimizing_byte_swapping_for_fun
 * http://stackoverflow.com/questions/105252/how-do-i-convert-between-big-endian-and-little-endian-values-in-c
 * http://cryptojedi.org/programming/qhasm-doc/SPARC.html
 * https://blogs.oracle.com/danmcd/entry/endian_independence_not_just_for
 * SPARC has an alternate space identifier load instruction. A predefine alternate space (0x88) is the little-endian space, which means if you utter "lduwa [address-reg] 0x88, [dst-reg]" the word pointed to by [address-reg] will be swapped into [dst-reg]. The sun4u version of MD5 exploits this instruction to overcome MD5's little-endian bias, for example.
 * <v9/sys/asi.h> defines ASI_PL 0x88 (used as a literal in assembly below)
 *   for address space identifier (ASI) primary little (PL) endian
 * lduha, lduwa, ldxa require address in register and do not support all the
 * addressing modes allowed by the "m" constraint.  Therefore, the assembly
 * uses "r"(ptr).  However, since the memory is accessed, not just the literal
 * address, an additional constraint is added: "m"(*ptr).  Sun Studio compiler:
 *   warning: parameter in inline asm statement unused: %2 
 * so add comment in assembly (beginning with '!') to fool Sun Studio compiler.
 * (Alternatively, can use Sun Studio compiler with -erroff=E_ASM_UNUSED_PARAM)
 */

  #if defined(__GNUC__) \
   || (defined(__SUNPRO_C)  && __SUNPRO_C  >= 0x5100) \
   || (defined(__SUNPRO_CC) && __SUNPRO_CC >= 0x5100) /* Sun Studio 12.1 */
    #define plasma_endian_swap16p_funcbody(ptr)                              \
      register uint16_t u;                                                   \
      __asm__ ("lduha [%1] 0x88, %0   !%2" : "=r"(u) : "r"(ptr), "m"(*ptr)); \
      return u
    #define plasma_endian_swap32p_funcbody(ptr)                              \
      register uint32_t u;                                                   \
      __asm__ ("lduwa [%1] 0x88, %0   !%2" : "=r"(u) : "r"(ptr), "m"(*ptr)); \
      return u
    #define plasma_endian_swap64p_funcbody(ptr)                              \
      register uint64_t u;                                                   \
      __asm__ ("ldxa  [%1] 0x88, %0   !%2" : "=r"(u) : "r"(ptr), "m"(*ptr)); \
      return u
    #define plasma_endian_swap16_funcbody(x) \
      return plasma_endian_swap16p(&(x))
    #define plasma_endian_swap32_funcbody(x) \
      return plasma_endian_swap32p(&(x))
    #define plasma_endian_swap64_funcbody(x) \
      return plasma_endian_swap64p(&(x))
  #else
    /*(out-of-line functions due to limited asm support in earlier Sun Studio)
     *(use C for 16-bit byte swap which is faster than function call overhead)*/
    __attribute_pure__
    uint32_t plasma_endian_swap32_SPARC (const uint32_t * restrict);
    __attribute_pure__
    uint32_t plasma_endian_swap64_SPARC (const uint64_t * restrict);
    #pragma no_side_effect(plasma_endian_swap32_SPARC)
    #pragma no_side_effect(plasma_endian_swap64_SPARC)
    #define plasma_endian_swap32_funcbody(x) \
      return plasma_endian_swap32_SPARC(&(x))
    #define plasma_endian_swap64_funcbody(x) \
      return plasma_endian_swap64_SPARC(&(x))
    #define plasma_endian_swap16p_funcmacro(x)  plasma_endian_swap16(*(x))
    #define plasma_endian_swap32p_funcmacro(x)  plasma_endian_swap32_SPARC(x)
    #define plasma_endian_swap64p_funcmacro(x)  plasma_endian_swap64_SPARC(x)
  #endif

#elif defined(__IBMC__) || defined(__IBMCPP__)

/* AIX xlc byte swapping intrinsics take pointer arguments; use in inline func
 * https://publib.boulder.ibm.com/infocenter/comphelp/v8v101/index.jsp?topic=%2Fcom.ibm.xlcpp8a.doc%2Fcompiler%2Fref%2Fbif_int.htm
 * (assembly not provided here for GCC < 4.3 (without __builtin_bswap_*()))
 * http://cr.illumos.org/~webrev/skiselkov/edonr/usr/src/common/crypto/edonr/edonr_byteorder.h.html */

  #define plasma_endian_swap16p_funcmacro(x)  __load2r(x)
  #define plasma_endian_swap32p_funcmacro(x)  __load4r(x)
  #define plasma_endian_swap16_funcbody(x) \
    return __load2r((unsigned short *)&(x))
  #define plasma_endian_swap32_funcbody(x) \
    return __load4r((unsigned int *)&(x))

  /* (xlc intrinsic __load8r() available only with
   *  -arch=pwr7 (_ARCH_PWR7) and -q64 (__64BIT__)) */
  #if defined(_ARCH_PWR7) && defined(__64BIT__)
    #define plasma_endian_swap64p_funcmacro(x)  __load8r(x)
    #define plasma_endian_swap64_funcbody(x) \
      return __load8r((unsigned long long *)&(x))
  #elif __BIG_ENDIAN__
    #define plasma_endian_swap64p_funcbody(ptr)                  \
      return __rldimi((uint64_t)__load4r(((uint32_t *)(ptr))+1), \
                      (uint64_t)__load4r(((uint32_t *)(ptr))),   \
                      32u, 0xFFFFFFFF00000000uLL)
    #define plasma_endian_swap64_funcbody(x) \
      return plasma_endian_swap64p(&(x))
  #elif __LITTLE_ENDIAN__
    #define plasma_endian_swap64p_funcbody(ptr)                  \
      return __rldimi((uint64_t)__load4r(((uint32_t *)(ptr))),   \
                      (uint64_t)__load4r(((uint32_t *)(ptr))+1), \
                      32u, 0xFFFFFFFF00000000uLL)
    #define plasma_endian_swap64_funcbody(x) \
      return plasma_endian_swap64p(&(x))
  #endif

#elif defined(__HP_cc) || defined(__HP_aCC)

/* http://h21007.www2.hp.com/portal/download/files/unprot/Itanium/kb/HPKB-003Inline_IPF_assembly.pdf
 * Using the mux instruction to switch byte order
 * uint64_t new_val = _Asm_mux1(_MBTYPE_REV, old_val); 
 * http://h21007.www2.hp.com/portal/download/files/unprot/Itanium/inline_assem_ERS.pdf
 * _Asm_shl  shift left */

  #include <machine/sys/inline.h>

  #define plasma_endian_swap16_funcbody(x)         \
    register uint64_t u = (x);                     \
    return _Asm_mux1(_MBTYPE_REV, _Asm_shl(u, 48))
  #define plasma_endian_swap32_funcbody(x)         \
    register uint64_t u = (x);                     \
    return _Asm_mux1(_MBTYPE_REV, _Asm_shl(u, 32))
  #define plasma_endian_swap64_funcbody(x)         \
    return _Asm_mux1(_MBTYPE_REV, (x))
  #define plasma_endian_swap16p_funcmacro(x)  plasma_endian_swap16(*(x))
  #define plasma_endian_swap32p_funcmacro(x)  plasma_endian_swap32(*(x))
  #define plasma_endian_swap64p_funcmacro(x)  plasma_endian_swap64(*(x))

#endif


#if __GNUC_PREREQ(4,3)

  #if __GNUC_PREREQ(4,8)
  #undef  plasma_endian_swap16_funcmacro
  #define plasma_endian_swap16_funcmacro(x)   __builtin_bswap16(x)
  #endif

  #undef  plasma_endian_swap32_funcmacro
  #define plasma_endian_swap32_funcmacro(x)   __builtin_bswap32(x)
  #undef  plasma_endian_swap64_funcmacro
  #define plasma_endian_swap64_funcmacro(x)   __builtin_bswap64(x)

#endif


/* (plasma_endian_*_const macros require x to be unsigned type of proper size)*/

#define plasma_endian_swap16_const(x) \
        ( ((x)>>8) \
        | ((x)<<8) )

#define plasma_endian_swap32_const(x) \
        ( (((x)>>24))                 \
        | (((x)<<8) & 0x00FF0000)     \
        | (((x)>>8) & 0x0000FF00)     \
        | (((x)<<24)) )

#define plasma_endian_swap64_const(x)          \
        ( (((x)>>56))                          \
        | (((x)<<40) & 0x00FF000000000000LL)   \
        | (((x)<<24) & 0x0000FF0000000000LL)   \
        | (((x)<<8)  & 0x000000FF00000000LL)   \
        | (((x)>>8)  & 0x00000000FF000000LL)   \
        | (((x)>>24) & 0x0000000000FF0000LL)   \
        | (((x)>>40) & 0x000000000000FF00LL)   \
        | (((x)<<56)) )


#ifdef plasma_endian_swap16_funcmacro
#define plasma_endian_swap16_func(x) plasma_endian_swap16_funcmacro(x)
#endif
#ifdef plasma_endian_swap16p_funcmacro
#define plasma_endian_swap16p_func(x) plasma_endian_swap16p_funcmacro(x)
#endif
#ifdef plasma_endian_swap32_funcmacro
#define plasma_endian_swap32_func(x) plasma_endian_swap32_funcmacro(x)
#endif
#ifdef plasma_endian_swap32p_funcmacro
#define plasma_endian_swap32p_func(x) plasma_endian_swap32p_funcmacro(x)
#endif
#ifdef plasma_endian_swap64_funcmacro
#define plasma_endian_swap64_func(x) plasma_endian_swap64_funcmacro(x)
#endif
#ifdef plasma_endian_swap64p_funcmacro
#define plasma_endian_swap64p_func(x) plasma_endian_swap64p_funcmacro(x)
#endif


#ifdef plasma_attr_has_builtin_constant_p
#define plasma_endian_swap16(x)     \
  (__builtin_constant_p(x)          \
    ? plasma_endian_swap16_const(x) \
    : plasma_endian_swap16_func(x))
#define plasma_endian_swap32(x)     \
  (__builtin_constant_p(x)          \
    ? plasma_endian_swap32_const(x) \
    : plasma_endian_swap32_func(x))
#define plasma_endian_swap64(x)     \
  (__builtin_constant_p(x)          \
    ? plasma_endian_swap64_const(x) \
    : plasma_endian_swap64_func(x))
#else
#define plasma_endian_swap16(x) plasma_endian_swap16_func(x)
#define plasma_endian_swap32(x) plasma_endian_swap32_func(x)
#define plasma_endian_swap64(x) plasma_endian_swap64_func(x)
#endif

#define plasma_endian_swap16p(x) plasma_endian_swap16p_func(x)
#define plasma_endian_swap32p(x) plasma_endian_swap32p_func(x)
#define plasma_endian_swap64p(x) plasma_endian_swap64p_func(x)


#ifdef __cplusplus
extern "C" {
#endif


#ifndef plasma_endian_swap16_funcmacro
#define plasma_endian_swap16_func plasma_endian_swap16_func
__attribute_const__
PLASMA_ENDIAN_C99INLINE
uint16_t
plasma_endian_swap16_func (const uint16_t x);
#endif

#ifndef plasma_endian_swap16p_funcmacro
#define plasma_endian_swap16p_func plasma_endian_swap16p_func
__attribute_pure__
PLASMA_ENDIAN_C99INLINE
uint16_t
plasma_endian_swap16p_func (const uint16_t * const restrict x)
  __attribute_nonnull__  __attribute_nothrow__;
PLASMA_ATTR_Pragma_no_side_effect(plasma_endian_swap16p_func)
#endif

#ifndef plasma_endian_swap16_funcmacro
#ifdef PLASMA_ENDIAN_C99INLINE_FUNCS
__attribute_const__
PLASMA_ENDIAN_C99INLINE
uint16_t
plasma_endian_swap16_func (const uint16_t x)
{
  #if defined(plasma_endian_swap16_funcbody)
    plasma_endian_swap16_funcbody(x);
  #else
    return plasma_endian_swap16_const(x);
  #endif
}
#endif
#endif

#ifndef plasma_endian_swap16p_funcmacro
#ifdef PLASMA_ENDIAN_C99INLINE_FUNCS
__attribute_pure__
PLASMA_ENDIAN_C99INLINE
uint16_t
plasma_endian_swap16p_func (const uint16_t * const restrict x)
  __attribute_nothrow__
{
  #if defined(plasma_endian_swap16p_funcbody)
    plasma_endian_swap16p_funcbody(x);
  #else
    return plasma_endian_swap16_const(*x);
  #endif
}
#endif
#endif


#ifndef plasma_endian_swap32_funcmacro
#define plasma_endian_swap32_func plasma_endian_swap32_func
__attribute_const__
PLASMA_ENDIAN_C99INLINE
uint32_t
plasma_endian_swap32_func (const uint32_t x);
#endif

#ifndef plasma_endian_swap32p_funcmacro
#define plasma_endian_swap32p_func plasma_endian_swap32p_func
__attribute_pure__
PLASMA_ENDIAN_C99INLINE
uint32_t
plasma_endian_swap32p_func (const uint32_t * const restrict x)
  __attribute_nonnull__  __attribute_nothrow__;
PLASMA_ATTR_Pragma_no_side_effect(plasma_endian_swap32p_func)
#endif

#ifndef plasma_endian_swap32_funcmacro
#ifdef PLASMA_ENDIAN_C99INLINE_FUNCS
__attribute_const__
PLASMA_ENDIAN_C99INLINE
uint32_t
plasma_endian_swap32_func (const uint32_t x)
{
  #if defined(plasma_endian_swap32_funcbody)
    plasma_endian_swap32_funcbody(x);
  #else
    return plasma_endian_swap32_const(x);
  #endif
}
#endif
#endif

#ifndef plasma_endian_swap32p_funcmacro
#ifdef PLASMA_ENDIAN_C99INLINE_FUNCS
__attribute_pure__
PLASMA_ENDIAN_C99INLINE
uint32_t
plasma_endian_swap32p_func (const uint32_t * const restrict x)
  __attribute_nothrow__
{
  #if defined(plasma_endian_swap32p_funcbody)
    plasma_endian_swap32p_funcbody(x);
  #else
    return plasma_endian_swap32(*x);
  #endif
}
#endif
#endif


#ifndef plasma_endian_swap64_funcmacro
#define plasma_endian_swap64_func plasma_endian_swap64_func
__attribute_const__
PLASMA_ENDIAN_C99INLINE
uint64_t
plasma_endian_swap64_func (const uint64_t x);
#endif

#ifndef plasma_endian_swap64p_funcmacro
#define plasma_endian_swap64p_func plasma_endian_swap64p_func
__attribute_pure__
PLASMA_ENDIAN_C99INLINE
uint64_t
plasma_endian_swap64p_func (const uint64_t * const restrict x)
  __attribute_nonnull__  __attribute_nothrow__;
PLASMA_ATTR_Pragma_no_side_effect(plasma_endian_swap64p_func)
#endif

#ifndef plasma_endian_swap64_funcmacro
#ifdef PLASMA_ENDIAN_C99INLINE_FUNCS
__attribute_const__
PLASMA_ENDIAN_C99INLINE
uint64_t
plasma_endian_swap64_func (const uint64_t x)
{
  #if defined(plasma_endian_swap64_funcbody)
    plasma_endian_swap64_funcbody(x);
  #else
    return plasma_endian_swap64_const(x);
  #endif
}
#endif
#endif

#ifndef plasma_endian_swap64p_funcmacro
#ifdef PLASMA_ENDIAN_C99INLINE_FUNCS
__attribute_pure__
PLASMA_ENDIAN_C99INLINE
uint64_t
plasma_endian_swap64p_func (const uint64_t * const restrict x)
  __attribute_nothrow__
{
  #if defined(plasma_endian_swap64p_funcbody)
    plasma_endian_swap64p_funcbody(x);
  #else
    return plasma_endian_swap64(*x);
  #endif
}
#endif
#endif


#ifdef __cplusplus
}
#endif


#endif /* (no OS std lib endian support) */



/*
 * BSD-style byteorder conversion macros
 * (additionally, provide macros to read from pointers to aligned data)
 * (aside: SPARC and POWER have assembly instructions (and xlC has intrinsics)
 *  to perform byteorder conversion as part of writing to a memory location,
 *  but a plasma_endian interface to leverage these is not currently provided)
 * (see plasma_feature.h for __LITTLE_ENDIAN__ and __BIG_ENDIAN__)
 */

#if __LITTLE_ENDIAN__

#define plasma_endian_htobe16(x) plasma_endian_swap16(x)
#define plasma_endian_htobe32(x) plasma_endian_swap32(x)
#define plasma_endian_htobe64(x) plasma_endian_swap64(x)

#define plasma_endian_be16toh(x) plasma_endian_swap16(x)
#define plasma_endian_be32toh(x) plasma_endian_swap32(x)
#define plasma_endian_be64toh(x) plasma_endian_swap64(x)

#define plasma_endian_htole16(x) (x)
#define plasma_endian_htole32(x) (x)
#define plasma_endian_htole64(x) (x)

#define plasma_endian_le16toh(x) (x)
#define plasma_endian_le32toh(x) (x)
#define plasma_endian_le64toh(x) (x)

#define plasma_endian_hptobe16(x) plasma_endian_swap16p(x)
#define plasma_endian_hptobe32(x) plasma_endian_swap32p(x)
#define plasma_endian_hptobe64(x) plasma_endian_swap64p(x)

#define plasma_endian_be16ptoh(x) plasma_endian_swap16p(x)
#define plasma_endian_be32ptoh(x) plasma_endian_swap32p(x)
#define plasma_endian_be64ptoh(x) plasma_endian_swap64p(x)

#define plasma_endian_hptole16(x) (*(x))
#define plasma_endian_hptole32(x) (*(x))
#define plasma_endian_hptole64(x) (*(x))

#define plasma_endian_le16ptoh(x) (*(x))
#define plasma_endian_le32ptoh(x) (*(x))
#define plasma_endian_le64ptoh(x) (*(x))

#elif __BIG_ENDIAN__

#define plasma_endian_htole16(x) plasma_endian_swap16(x)
#define plasma_endian_htole32(x) plasma_endian_swap32(x)
#define plasma_endian_htole64(x) plasma_endian_swap64(x)

#define plasma_endian_le16toh(x) plasma_endian_swap16(x)
#define plasma_endian_le32toh(x) plasma_endian_swap32(x)
#define plasma_endian_le64toh(x) plasma_endian_swap64(x)

#define plasma_endian_htobe16(x) (x)
#define plasma_endian_htobe32(x) (x)
#define plasma_endian_htobe64(x) (x)

#define plasma_endian_be16toh(x) (x)
#define plasma_endian_be32toh(x) (x)
#define plasma_endian_be64toh(x) (x)

#define plasma_endian_hptobe16(x) (*(x))
#define plasma_endian_hptobe32(x) (*(x))
#define plasma_endian_hptobe64(x) (*(x))

#define plasma_endian_be16ptoh(x) (*(x))
#define plasma_endian_be32ptoh(x) (*(x))
#define plasma_endian_be64ptoh(x) (*(x))

#define plasma_endian_hptole16(x) plasma_endian_swap16p(x)
#define plasma_endian_hptole32(x) plasma_endian_swap32p(x)
#define plasma_endian_hptole64(x) plasma_endian_swap64p(x)

#define plasma_endian_le16ptoh(x) plasma_endian_swap16p(x)
#define plasma_endian_le32ptoh(x) plasma_endian_swap32p(x)
#define plasma_endian_le64ptoh(x) plasma_endian_swap64p(x)

#endif


#define plasma_endian_htonl(x) plasma_endian_htobe32(x)
#define plasma_endian_htons(x) plasma_endian_htobe16(x)
#define plasma_endian_ntohl(x) plasma_endian_be32toh(x)
#define plasma_endian_ntohs(x) plasma_endian_be16toh(x)


/* byteorder conversion to/from pointers to data with unknown alignment
 * (code below originated in mcdb/uint32.h)
 * (mimic FreeBSD naming http://fxr.watson.org/fxr/source/sys/endian.h)
 *
 * NB: macro args are repeated and so should be simple with no side-effects
 *     (or else caller might choose to create an inline function to wrap macro)
 *     (inline functions could be provided below, though not provided for now)
 */

#define plasma_endian_le16dec_macro(s)    ( (uint16_t) \
  ( (((uint_fast16_t)(((unsigned char *)(s))[0])))     \
  | (((uint_fast16_t)(((unsigned char *)(s))[1]))<<8) ))

#define plasma_endian_be16dec_macro(s)    ( (uint16_t) \
  ( (((uint_fast16_t)(((unsigned char *)(s))[1])))     \
  | (((uint_fast16_t)(((unsigned char *)(s))[0]))<<8) ))

#define plasma_endian_le16enc_macro(s,u) \
  (s)[0]=(__typeof__(*(s)))((u));        \
  (s)[1]=(__typeof__(*(s)))((u)>>8)

#define plasma_endian_be16enc_macro(s,u) \
  (s)[0]=(__typeof__(*(s)))((u)>>8);     \
  (s)[1]=(__typeof__(*(s)))((u))

#define plasma_endian_le32dec_macro(s)    ( (uint32_t) \
  ( (((uint_fast32_t)(((unsigned char *)(s))[0])))     \
  | (((uint_fast32_t)(((unsigned char *)(s))[1]))<<8)  \
  | (((uint_fast32_t)(((unsigned char *)(s))[2]))<<16) \
  | (((uint_fast32_t)(((unsigned char *)(s))[3]))<<24) ) )

#define plasma_endian_be32dec_macro(s)    ( (uint32_t) \
  ( (((uint_fast32_t)(((unsigned char *)(s))[3])))     \
  | (((uint_fast32_t)(((unsigned char *)(s))[2]))<<8)  \
  | (((uint_fast32_t)(((unsigned char *)(s))[1]))<<16) \
  | (((uint_fast32_t)(((unsigned char *)(s))[0]))<<24) ) )

#define plasma_endian_le32enc_macro(s,u) \
  (s)[0]=(__typeof__(*(s)))((u));        \
  (s)[1]=(__typeof__(*(s)))((u)>>8);     \
  (s)[2]=(__typeof__(*(s)))((u)>>16);    \
  (s)[3]=(__typeof__(*(s)))((u)>>24)

#define plasma_endian_be32enc_macro(s,u) \
  (s)[0]=(__typeof__(*(s)))((u)>>24);    \
  (s)[1]=(__typeof__(*(s)))((u)>>16);    \
  (s)[2]=(__typeof__(*(s)))((u)>>8);     \
  (s)[3]=(__typeof__(*(s)))((u))

#define plasma_endian_le64dec_macro(s)    ( (uint64_t) \
  ( (((uint_fast64_t)(((unsigned char *)(s))[0])))     \
  | (((uint_fast64_t)(((unsigned char *)(s))[1]))<<8)  \
  | (((uint_fast64_t)(((unsigned char *)(s))[2]))<<16) \
  | (((uint_fast64_t)(((unsigned char *)(s))[3]))<<24) \
  | (((uint_fast64_t)(((unsigned char *)(s))[4]))<<32) \
  | (((uint_fast64_t)(((unsigned char *)(s))[5]))<<40) \
  | (((uint_fast64_t)(((unsigned char *)(s))[6]))<<48) \
  | (((uint_fast64_t)(((unsigned char *)(s))[7]))<<56) ) )

#define plasma_endian_be64dec_macro(s)    ( (uint64_t) \
  ( (((uint_fast64_t)(((unsigned char *)(s))[7])))     \
  | (((uint_fast64_t)(((unsigned char *)(s))[6]))<<8)  \
  | (((uint_fast64_t)(((unsigned char *)(s))[5]))<<16) \
  | (((uint_fast64_t)(((unsigned char *)(s))[4]))<<24) \
  | (((uint_fast64_t)(((unsigned char *)(s))[3]))<<32) \
  | (((uint_fast64_t)(((unsigned char *)(s))[2]))<<40) \
  | (((uint_fast64_t)(((unsigned char *)(s))[1]))<<48) \
  | (((uint_fast64_t)(((unsigned char *)(s))[0]))<<56) ) )

#define plasma_endian_le64enc_macro(s,u) \
  (s)[0]=(__typeof__(*(s)))((u));        \
  (s)[1]=(__typeof__(*(s)))((u)>>8);     \
  (s)[2]=(__typeof__(*(s)))((u)>>16);    \
  (s)[3]=(__typeof__(*(s)))((u)>>24);    \
  (s)[4]=(__typeof__(*(s)))((u)>>32);    \
  (s)[5]=(__typeof__(*(s)))((u)>>40);    \
  (s)[6]=(__typeof__(*(s)))((u)>>48);    \
  (s)[7]=(__typeof__(*(s)))((u)>>56)

#define plasma_endian_be64enc_macro(s,u) \
  (s)[0]=(__typeof__(*(s)))((u)>>56);    \
  (s)[1]=(__typeof__(*(s)))((u)>>48);    \
  (s)[2]=(__typeof__(*(s)))((u)>>40);    \
  (s)[3]=(__typeof__(*(s)))((u)>>32);    \
  (s)[4]=(__typeof__(*(s)))((u)>>24);    \
  (s)[5]=(__typeof__(*(s)))((u)>>16);    \
  (s)[6]=(__typeof__(*(s)))((u)>>8);     \
  (s)[7]=(__typeof__(*(s)))((u))


#endif
