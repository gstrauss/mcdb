/*
 * uint32 - pack and unpack ASCII and hex strings to uint32_t (4-bytes)
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

#ifndef INCLUDED_UINT32_H
#define INCLUDED_UINT32_H

#include "plasma/plasma_feature.h"
#include "plasma/plasma_attr.h"
#include "plasma/plasma_endian.h"
#include "plasma/plasma_stdtypes.h"
PLASMA_ATTR_Pragma_once

#ifndef UINT32_C99INLINE
#define UINT32_C99INLINE C99INLINE
#endif
#ifndef NO_C99INLINE
#ifndef UINT32_C99INLINE_FUNCS
#define UINT32_C99INLINE_FUNCS
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*(use macros only with simple args, or else better to call inline subroutine)*/

#define uint32_strpack_bigendian_macro(s,u) \
        plasma_endian_be32enc_macro((s),(u))
#define uint32_strunpack_bigendian_macro(s) \
        plasma_endian_be32dec_macro(s)

#define uint32_strpack_bigendian_aligned_macro(s,u) \
        (*((uint32_t *)(s)) = plasma_endian_htobe32(u))
#define uint32_strunpack_bigendian_aligned_macro(s) \
        plasma_endian_be32ptoh((uint32_t *)(s))

#define uint64_strpack_bigendian_aligned_macro(s,u) \
        (*((uint64_t *)(s)) = plasma_endian_htobe64(u))
#define uint64_strunpack_bigendian_aligned_macro(s) \
        plasma_endian_be64ptoh((uint64_t *)(s))

/*(non-generic optimization specific to mcdb code usage and only for 32-bit)
 *  *(mcdb limited to 4 GB when compiled 32-bit, so unpack 64-bit nums < 4 GB)*/
#if !defined(_LP64) && !defined(__LP64__)
#undef  uint64_strunpack_bigendian_aligned_macro
#define uint64_strunpack_bigendian_aligned_macro(s) \
        plasma_endian_be32ptoh((uint32_t *)(((char *)(s))+4))
#endif


/* C99 inline functions defined in header */


/* djb cdb hash function: http://cr.yp.to/cdb/cdb.txt
 * modified from the Public Domain cdb-0.75 by Dan Bernstein */

#define UINT32_HASH_DJB_INIT 5381u

#define uint32_hash_djb_uchar(h,c) (((h) + ((h) << 5)) ^ (c))

__attribute_nonnull__()
__attribute_nothrow__
__attribute_pure__
__attribute_warn_unused_result__
UINT32_C99INLINE
uint32_t
uint32_hash_djb(uint32_t, const void * restrict, size_t);
PLASMA_ATTR_Pragma_no_side_effect(uint32_hash_djb)
#ifdef UINT32_C99INLINE_FUNCS
UINT32_C99INLINE
uint32_t
uint32_hash_djb(uint32_t h, const void * const restrict vbuf, const size_t sz)
{
    const unsigned char * restrict buf = (const unsigned char *)vbuf;
    const unsigned char * const e = (const unsigned char *)vbuf + sz;
    for (; __builtin_expect( (buf < e), 1); ++buf)
        h = uint32_hash_djb_uchar(h,*buf);
    return h;
}
#endif

__attribute_nonnull__()
__attribute_nothrow__
__attribute_pure__
__attribute_warn_unused_result__
UINT32_C99INLINE
uint32_t
uint32_hash_identity(uint32_t, const void * restrict, size_t);
PLASMA_ATTR_Pragma_no_side_effect(uint32_hash_identity)
#ifdef UINT32_C99INLINE_FUNCS
UINT32_C99INLINE
uint32_t
uint32_hash_identity(uint32_t h       __attribute_unused__,
                     const void * const restrict vbuf,
                     const size_t sz  __attribute_unused__)
{
    return *(uint32_t *)vbuf;
}
#endif


/* 
 * branchless implementations for comparing two ints and selecting int results
 *
 *     x and y must be in range INT_MIN <= {x,y} <= INT_MAX (signed or unsigned)
 *     a and b must be same bit width as x and y (i.e. 32-bits)
 *
 * branchless implementations applied to int to ASCII hex conversion and reverse
 *
 * References:
 *   http://www.cellperformance.com/
 *     /articles/2006/04/benefits_to_branch_elimination.html
 *   http://graphics.stanford.edu/~seander/bithacks.html#CopyIntegerSign
 *   http://aggregate.org/MAGIC/
 * modified to use unsigned shift; signed right shift is implementation defined
 *
 * 1) cast x and y to int32_t to avoid underflow warnings and then subtract
 *      (x < y yields negative number; x >= y yields positive number or zero)
 * 2) cast result to uint32_t for standards-defined behavior for right bit shift
 *      (isolate most significant bit: 1 for negative number, 0 otherwise)
 * 3) cast result to  int32_t and negate result in two's complement
 *      (-1 is two's complement all bits one, -0 remains all bits zero)
 *      (all 1's if (x < y); 0's if !(x < y) -- for INT_MIN <= {x,y} <= INT_MAX)
 * 4) bitwise-and the all_ones or all_zeros result with a ^ b
 *      (all_ones & (a^b) yields (a^b); all_zeros & (a^b) yields all_zeros)
 * 5) xor result with b
 *      ((a^b) ^ b yields a; all_zeros ^ b yields b)
 */

#define int32_x_lt_y_returns_all_ones(x,y) \
    (-(int32_t)(((uint32_t)(((int32_t)(x))-((int32_t)(y))))>>31))

#define int32_x_lt_y_select_a_else_b(x,y,a,b) \
    (((int32_x_lt_y_returns_all_ones((x),(y))) & ((a)^(b))) ^ (b))

/* convert nibble to ASCII (uppercase) hex char */
#define ntoux(n) ((char)((n) + int32_x_lt_y_select_a_else_b((n),10,'0','A'-10)))

/* convert ASCII (uppercase) hex char to nibble */
#define uxton(x) ((x) - int32_x_lt_y_select_a_else_b((x),'A','0','A'-10))

/* convert nibble to ASCII (lowercase) hex char */
#define ntolx(n) ((char)((n) + int32_x_lt_y_select_a_else_b((n),10,'0','a'-10)))

/* convert ASCII (lowercase) hex char to nibble */
#define lxton(x) ((x) - int32_x_lt_y_select_a_else_b((x),'a','0','a'-10))

/* (using intermediate might be faster when a and/or b require calculation) */
#define int32_intermediate_select_a_else_b(all_ones_or_all_zeros,a,b) \
    (((all_ones_or_all_zeros) & (a)) | (~(all_ones_or_all_zeros) & (b)))
#define xton_select_lt_A(x, x_lt_A_all_ones) \
    int32_intermediate_select_a_else_b((x_lt_A_all_ones),(x)-'0',((x)&7)+9)

/* above macros could be written into isolated basic blocks using intermediates,
 * but not writing macros as { ... } allows compiler more leeway in reordering,
 * especially when doing operations on multiple units where it is more efficient
 * to do all loads, then modifies, then stores to avoid stalling on loads */


/*
 * branchless implementations of int type to hex str and hex str to int type
 */


/* convert unsigned 32-bit value into string of 8 ASCII uppercase hex chars
 * (used to convert numerical data to architecture-independent string data)
 * (call x = (uint32_to_ascii8uphex(n,buf), buf); to have buf returned)
 * (buf must be at least 8 chars in length; not checked)
 * (buf returned contains exactly 8 chars and is not NUL-terminated) */
__attribute_nonnull__()
__attribute_nothrow__
UINT32_C99INLINE
void
uint32_to_ascii8uphex(const uint32_t n, char * const restrict buf);
#ifdef UINT32_C99INLINE_FUNCS
UINT32_C99INLINE
void
uint32_to_ascii8uphex(const uint32_t n, char * const restrict buf)
{
    const uint32_t n0 = (((uint32_t)n) & 0xF0000000u) >> 28;
    const uint32_t n1 = (((uint32_t)n) & 0x0F000000u) >> 24;
    const uint32_t n2 = (((uint32_t)n) & 0x00F00000u) >> 20;
    const uint32_t n3 = (((uint32_t)n) & 0x000F0000u) >> 16;
    const uint32_t n4 = (((uint32_t)n) & 0x0000F000u) >> 12;
    const uint32_t n5 = (((uint32_t)n) & 0x00000F00u) >>  8;
    const uint32_t n6 = (((uint32_t)n) & 0x000000F0u) >>  4;
    const uint32_t n7 = (((uint32_t)n) & 0x0000000Fu);

    buf[0] = ntoux(n0);
    buf[1] = ntoux(n1);
    buf[2] = ntoux(n2);
    buf[3] = ntoux(n3);
    buf[4] = ntoux(n4);
    buf[5] = ntoux(n5);
    buf[6] = ntoux(n6);
    buf[7] = ntoux(n7);
}
#endif

/* convert unsigned 16-bit value into string of 4 ASCII uppercase hex chars
 * (used to convert numerical data to architecture-independent string data)
 * (operate in 32-bit intermediates to use more native sizes on modern CPUs)
 * (caller must verify that n is in range 0 <= n <= USHRT_MAX)
 * (call x = (uint32_to_ascii4uphex(n,buf), buf); to have buf returned)
 * (buf must be at least 8 chars in length; not checked)
 * (buf returned contains exactly 4 chars and is not NUL-terminated) */
__attribute_nonnull__()
__attribute_nothrow__
UINT32_C99INLINE
void
uint16_to_ascii4uphex(const uint32_t n, char * const restrict buf);
#ifdef UINT32_C99INLINE_FUNCS
UINT32_C99INLINE
void
uint16_to_ascii4uphex(const uint32_t n, char * const restrict buf)
{
    const uint32_t n0 = (((uint32_t)n) & 0x0000F000u) >> 12;
    const uint32_t n1 = (((uint32_t)n) & 0x00000F00u) >>  8;
    const uint32_t n2 = (((uint32_t)n) & 0x000000F0u) >>  4;
    const uint32_t n3 = (((uint32_t)n) & 0x0000000Fu);

    buf[0] = ntoux(n0);
    buf[1] = ntoux(n1);
    buf[2] = ntoux(n2);
    buf[3] = ntoux(n3);
}
#endif

/* convert string of 8 ASCII uppercase hex chars to unsigned 32-bit value
 * (used to convert architecture-independent string data to numerical data)
 * (use when hex chars are known 0..9 A..F) */
__attribute_nonnull__()
__attribute_nothrow__
__attribute_pure__
__attribute_warn_unused_result__
UINT32_C99INLINE
uint32_t
uint32_from_ascii8uphex(const char * const restrict buf);
PLASMA_ATTR_Pragma_no_side_effect(uint32_from_ascii8uphex)
#ifdef UINT32_C99INLINE_FUNCS
UINT32_C99INLINE
uint32_t
uint32_from_ascii8uphex(const char * const restrict buf)
{
    const unsigned char * const b = (const unsigned char *)buf;
    const uint32_t x0 = b[0];
    const uint32_t x1 = b[1];
    const uint32_t x2 = b[2];
    const uint32_t x3 = b[3];
    const uint32_t x4 = b[4];
    const uint32_t x5 = b[5];
    const uint32_t x6 = b[6];
    const uint32_t x7 = b[7];

    const uint32_t n0 = uxton(x0) << 28;
    const uint32_t n1 = uxton(x1) << 24;
    const uint32_t n2 = uxton(x2) << 20;
    const uint32_t n3 = uxton(x3) << 16;
    const uint32_t n4 = uxton(x4) << 12;
    const uint32_t n5 = uxton(x5) <<  8;
    const uint32_t n6 = uxton(x6) <<  4;
    const uint32_t n7 = uxton(x7);

    return (n0 | n1 | n2 | n3 | n4 | n5 | n6 | n7);
}
#endif

/* convert string of 4 ASCII uppercase hex chars to unsigned 16-bit value
 * (used to convert architecture-independent string data to numerical data)
 * (operate in 32-bit intermediates to use more native sizes on modern CPUs)
 * (use when hex chars are known 0..9 A..F) */
__attribute_nonnull__()
__attribute_nothrow__
__attribute_pure__
__attribute_warn_unused_result__
UINT32_C99INLINE
uint16_t
uint16_from_ascii4uphex(const char * const restrict buf);
PLASMA_ATTR_Pragma_no_side_effect(uint16_from_ascii4uphex)
#ifdef UINT32_C99INLINE_FUNCS
UINT32_C99INLINE
uint16_t
uint16_from_ascii4uphex(const char * const restrict buf)
{
    const unsigned char * const b = (const unsigned char *)buf;
    const uint32_t x0 = b[0];
    const uint32_t x1 = b[1];
    const uint32_t x2 = b[2];
    const uint32_t x3 = b[3];

    const uint32_t n0 = uxton(x0) << 12;
    const uint32_t n1 = uxton(x1) <<  8;
    const uint32_t n2 = uxton(x2) <<  4;
    const uint32_t n3 = uxton(x3);

    return (uint16_t)(n0 | n1 | n2 | n3);
}
#endif

#define uint32_to_ascii8hex(n,buf) uint32_to_ascii8uphex((n),(buf))

#define uint16_to_ascii4hex(n,buf) uint16_to_ascii4uphex((n),(buf))

/* (not inlined in header) */

/* convert string of 8 ASCII hex chars to unsigned 32-bit value
 * (used to convert architecture-independent string data to numerical data)
 * (use when hex chars are known 0..9 A..F a..f) */
__attribute_nonnull__()
__attribute_nothrow__
__attribute_pure__
__attribute_warn_unused_result__
uint32_t
uint32_from_ascii8hex(const char * const restrict buf);
PLASMA_ATTR_Pragma_no_side_effect(uint32_from_ascii8hex)

/* convert string of 4 ASCII hex chars to unsigned 16-bit value
 * (used to convert architecture-independent string data to numerical data)
 * (use when hex chars are known 0..9 A..F a..f) */
__attribute_nonnull__()
__attribute_nothrow__
__attribute_pure__
__attribute_warn_unused_result__
uint16_t
uint32_from_ascii4hex(const char * const restrict buf);
PLASMA_ATTR_Pragma_no_side_effect(uint32_from_ascii4hex)


/*
 * convert 32-bit unsigned/signed integer to ASCII string of base-10 digits
 * (and unsigned/signed char/short types which promote to int in registers)
 */


/* convert unsigned 32-bit value into string of up to (10) ASCII base-10 digits
 * (helper function for inline function uint32_to_ascii_base10()) (see below) */
__attribute_nonnull__()
__attribute_nothrow__
__attribute_warn_unused_result__
uint32_t
uint32_to_ascii_base10_loop (uint32_t x, char * const restrict buf);

/* convert unsigned 32-bit value into string of up to (10) ASCII base-10 digits
 * (used to append string to a buffer or to assign into an iovec)
 * (avoids call to more flexible, but more expensive snprintf())
 * returns number of characters added to buffer (num from 1 to 10, inclusive)
 * (string is not NUL-terminated)
 * (buf must be at least 10 chars in length; not checked) */
__attribute_nonnull__()
__attribute_nothrow__
__attribute_warn_unused_result__
UINT32_C99INLINE
uint32_t
uint32_to_ascii_base10 (const uint32_t x, char * const restrict buf);
#ifdef UINT32_C99INLINE_FUNCS
UINT32_C99INLINE
uint32_t
uint32_to_ascii_base10 (const uint32_t x, char * const restrict buf)
{
    if (x < 10) {
        buf[0] = (char)(x + '0');
        return 1;
    }
    else if (x < 100) {
        buf[0] = (char)((x / 10) + '0');
        buf[1] = (char)((x % 10) + '0');
        return 2;
    }
    else {
        return uint32_to_ascii_base10_loop(x, buf);
    }
}
#endif

/* convert signed 32-bit value into string of up to (11) ASCII base-10 digits
 * (used to append string to a buffer or to assign into an iovec)
 * (avoids call to more flexible, but more expensive snprintf())
 * returns number of characters added to buffer (num from 1 to 11, inclusive)
 * (string is not NUL-terminated)
 * (buf must be at least 11 chars in length; not checked)
 * (buf size >= 12 chars can: buf[(int32_to_ascii_base_10(x, buf))] = '\0'; ) */
__attribute_nonnull__()
__attribute_nothrow__
__attribute_warn_unused_result__
UINT32_C99INLINE
uint32_t
int32_to_ascii_base10 (int32_t x, char * restrict buf);
#ifdef UINT32_C99INLINE_FUNCS
UINT32_C99INLINE
uint32_t
int32_to_ascii_base10 (int32_t x, char * restrict buf)
{
    const uint32_t pre = (x < 0);
    if (pre) {
        if (__builtin_expect( (x != (int32_t)0x80000000u), 1)) {/*test INT_MIN*/
            x = -x;
            *buf++ = '-';
        }
        else { /* special-case INT_MIN; -INT_MIN overflows and equals INT_MIN */
            PLASMA_ATTR_Pragma_execution_frequency_very_low
            /*memcpy(buf, "-2147483648", 11);*//* would require <string.h> */
            int i; for (i=0; i < 11; ++i) buf[i] = ("-2147483648")[i];
            return 11;
        }
    }
    return pre + uint32_to_ascii_base10((uint32_t)x, buf);
}
#endif


#ifdef __cplusplus
}
#endif

#endif
