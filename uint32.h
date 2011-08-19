/* License: GPLv3 */

#ifndef INCLUDED_UINT32_H
#define INCLUDED_UINT32_H

#include "code_attributes.h"

#include <sys/types.h>
#include <stdint.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

/*(use macros only with simple args, or else better to call inline subroutine)*/

/* uint32_strunpack_macro(): string 's' must be unsigned char */
#define uint32_strunpack_macro(s) \
  ( (s)[0] | ((s)[1]<<8) | ((s)[2]<<16) | ((s)[3]<<24) )

/* uint32_strunpack_bigendian_macro(): string 's' must be unsigned char */
#define uint32_strunpack_bigendian_macro(s) \
  ( ((s)[0]<<24) | ((s)[1]<<16) | ((s)[2]<<8) | (s)[3] )

#define uint32_strpack_macro(s,u) \
  (s)[0]=u & 0xFF; (s)[1]=(u>>8) & 0xFF; (s)[2]=(u>>16) & 0xFF; (s)[3]=(u>>24)

#define uint32_strpack_bigendian_macro(s,u) \
  (s)[0]=(u>>24); (s)[1]=(u>>16) & 0xFF; (s)[2]=(u>>8) & 0xFF; (s)[3]=u & 0xFF


#include <arpa/inet.h>  /* htonl(), ntohl() */
#define uint32_strpack_bigendian_aligned_macro(s,u) \
   (*((uint32_t *)(s)) = htonl(u))
#define uint32_strunpack_bigendian_aligned_macro(s) \
   ntohl(*((uint32_t *)(s)))


/*(uint64 macros built with uint32 macros)*/
#define uint64_strpack_bigendian_aligned_macro(s,u) \
   uint32_strpack_bigendian_aligned_macro((s), (uint32_t)((u)>>32)), \
   uint32_strpack_bigendian_aligned_macro((s)+4,(uint32_t)(u))
#define uint64_strunpack_bigendian_aligned_macro(s) \
   ( (((uint64_t)uint32_strunpack_bigendian_aligned_macro((s)))<<32) \
    |           (uint32_strunpack_bigendian_aligned_macro((s)+4)) )
/*(non-generic optimization specific to mcdb code usage and only for 32-bit)*/
#if !defined(_LP64) && !defined(__LP64__)
#undef  uint64_strunpack_bigendian_aligned_macro
#define uint64_strunpack_bigendian_aligned_macro(s) \
                 uint32_strunpack_bigendian_aligned_macro((s)+4)
#endif


/* C99 inline functions defined in header */

uint32_t  C99INLINE  __attribute_pure__
uint32_strunpack(const char s[4])
  __attribute_warn_unused_result__;
#if !defined(NO_C99INLINE)
uint32_t  C99INLINE
uint32_strunpack(const char s[4])
{
    const unsigned char * const restrict n = (const unsigned char *)s;
    return uint32_strunpack_macro(n);
}
#endif

uint32_t  C99INLINE  __attribute_pure__
uint32_strunpack_bigendian(const char s[4])
  __attribute_warn_unused_result__;
#if !defined(NO_C99INLINE)
uint32_t  C99INLINE
uint32_strunpack_bigendian(const char s[4])
{
    const unsigned char * const restrict n = (const unsigned char *)s;
    return uint32_strunpack_bigendian_macro(n);
}
#endif

void  C99INLINE
uint32_strpack(char s[4], const uint32_t u);
#if !defined(NO_C99INLINE)
void  C99INLINE
uint32_strpack(char s[4], const uint32_t u)
{
    uint32_strpack_macro(s,u);
}
#endif

void  C99INLINE
uint32_strpack_bigendian(char s[4], const uint32_t u);
#if !defined(NO_C99INLINE)
void  C99INLINE
uint32_strpack_bigendian(char s[4], const uint32_t u)
{
    uint32_strpack_bigendian_macro(s,u);
}
#endif


/* djb cdb hash function: http://cr.yp.to/cdb/cdb.txt */

#define UINT32_HASH_DJB_INIT 5381

#define uint32_hash_djb_uchar(h,c) (((h) + ((h) << 5)) ^ (c))

uint32_t  C99INLINE  __attribute_pure__
uint32_hash_djb(uint32_t, const void *, size_t)
  __attribute_nonnull__  __attribute_warn_unused_result__;
#if !defined(NO_C99INLINE)
uint32_t  C99INLINE
uint32_hash_djb(uint32_t h, const void * const vbuf, const size_t sz)
{
    /* TODO: test if better code is generated with:
     *    while (sz--) h = (h + (h << 5)) ^ *buf++; */
    /* Note: if size_t is signed type, then overflow is undefined behavior */
    const unsigned char * const restrict buf = (const unsigned char *)vbuf;
    size_t i = SIZE_MAX;  /* (size_t)-1; will wrap around to 0 with first ++i */
    while (++i < sz)
        h = (h + (h << 5)) ^ buf[i];
    return h;
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
 * modified to use unsigned shift since signed right shift implentation defined
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
void  C99INLINE
uint32_to_ascii8uphex(const uint32_t n, char * const restrict buf)
  __attribute_nonnull__;
#if !defined(NO_C99INLINE)
void  C99INLINE
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
void  C99INLINE
uint16_to_ascii4uphex(const uint32_t n, char * const restrict buf)
  __attribute_nonnull__;
#if !defined(NO_C99INLINE)
void  C99INLINE
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
uint32_t  C99INLINE  __attribute_pure__
uint32_from_ascii8uphex(const char * const restrict buf)
  __attribute_nonnull__  __attribute_warn_unused_result__;
#if !defined(NO_C99INLINE)
uint32_t  C99INLINE
uint32_from_ascii8uphex(const char * const restrict buf)
{
    const uint32_t x0 = buf[0];
    const uint32_t x1 = buf[1];
    const uint32_t x2 = buf[2];
    const uint32_t x3 = buf[3];
    const uint32_t x4 = buf[4];
    const uint32_t x5 = buf[5];
    const uint32_t x6 = buf[6];
    const uint32_t x7 = buf[7];

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
uint16_t  C99INLINE  __attribute_pure__
uint16_from_ascii4uphex(const char * const restrict buf)
  __attribute_nonnull__  __attribute_warn_unused_result__;
#if !defined(NO_C99INLINE)
uint16_t  C99INLINE
uint16_from_ascii4uphex(const char * const restrict buf)
{
    const uint32_t x0 = buf[0];
    const uint32_t x1 = buf[1];
    const uint32_t x2 = buf[2];
    const uint32_t x3 = buf[3];

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
uint32_t  __attribute_pure__
uint32_from_ascii8hex(const char * const restrict buf)
  __attribute_nonnull__  __attribute_warn_unused_result__;

/* convert string of 4 ASCII hex chars to unsigned 16-bit value
 * (used to convert architecture-independent string data to numerical data)
 * (use when hex chars are known 0..9 A..F a..f) */
uint16_t  __attribute_pure__
uint32_from_ascii4hex(const char * const restrict buf)
  __attribute_nonnull__  __attribute_warn_unused_result__;


#ifdef __cplusplus
}
#endif

#endif
