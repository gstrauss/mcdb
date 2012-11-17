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

/* inlined functions defined in header
 * (generate external linkage definition in GCC versions earlier than GCC 4.3)
 * (uint32.h does not include other headers defining other inline functions
 *  in header, so simply disable C99INLINE to generate external linkage
 *  definition for all inlined functions seen (i.e. those in uint32.h))
 */
#if defined(NO_C99INLINE)||(defined(__GNUC__) && !defined(__GNUC_STDC_INLINE__))
#define C99INLINE
#undef  NO_C99INLINE
#endif

#include "uint32.h"

/* inlined functions defined in header
 * (generate external linkage definition in C99-compliant compilers)
 * (need to -duplicate- definition from header for non-C99-compliant compiler)
 */
#if !defined(__GNUC__) || defined(__GNUC_STDC_INLINE__)
extern inline
uint32_t uint32_strunpack(const char s[4]);
uint32_t uint32_strunpack(const char s[4]);
extern inline
uint32_t uint32_strunpack_bigendian(const char s[4]);
uint32_t uint32_strunpack_bigendian(const char s[4]);
extern inline
void uint32_strpack(char s[4], uint32_t);
void uint32_strpack(char s[4], uint32_t);
extern inline
void uint32_strpack_bigendian(char s[4], uint32_t);
void uint32_strpack_bigendian(char s[4], uint32_t);

extern inline
uint32_t uint32_hash_djb(uint32_t, const void * restrict, size_t);
uint32_t uint32_hash_djb(uint32_t, const void * restrict, size_t);
extern inline
uint32_t uint32_hash_identity(uint32_t, const void * restrict, size_t);
uint32_t uint32_hash_identity(uint32_t, const void * restrict, size_t);

extern inline
void uint32_to_ascii8uphex(uint32_t, char * restrict);
void uint32_to_ascii8uphex(uint32_t, char * restrict);
extern inline
void uint16_to_ascii4uphex(uint32_t, char * restrict);
void uint16_to_ascii4uphex(uint32_t, char * restrict);
extern inline
uint32_t uint32_from_ascii8uphex(const char * restrict);
uint32_t uint32_from_ascii8uphex(const char * restrict);
extern inline
uint16_t uint16_from_ascii4uphex(const char * restrict);
uint16_t uint16_from_ascii4uphex(const char * restrict);
#endif


/* convert string of 8 ASCII hex chars to unsigned 32-bit value
 * (used to convert architecture-independent string data to numerical data)
 * (use when hex chars are known 0..9 A..F a..f) */
uint32_t
uint32_from_ascii8hex(const char * const restrict buf)
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

    const uint32_t x0_lt_A = (uint32_t)int32_x_lt_y_returns_all_ones(x0,'A');
    const uint32_t x1_lt_A = (uint32_t)int32_x_lt_y_returns_all_ones(x1,'A');
    const uint32_t x2_lt_A = (uint32_t)int32_x_lt_y_returns_all_ones(x2,'A');
    const uint32_t x3_lt_A = (uint32_t)int32_x_lt_y_returns_all_ones(x3,'A');
    const uint32_t x4_lt_A = (uint32_t)int32_x_lt_y_returns_all_ones(x4,'A');
    const uint32_t x5_lt_A = (uint32_t)int32_x_lt_y_returns_all_ones(x5,'A');
    const uint32_t x6_lt_A = (uint32_t)int32_x_lt_y_returns_all_ones(x6,'A');
    const uint32_t x7_lt_A = (uint32_t)int32_x_lt_y_returns_all_ones(x7,'A');

    const uint32_t n0 = xton_select_lt_A(x0, x0_lt_A) << 28;
    const uint32_t n1 = xton_select_lt_A(x1, x1_lt_A) << 24;
    const uint32_t n2 = xton_select_lt_A(x2, x2_lt_A) << 20;
    const uint32_t n3 = xton_select_lt_A(x3, x3_lt_A) << 16;
    const uint32_t n4 = xton_select_lt_A(x4, x4_lt_A) << 12;
    const uint32_t n5 = xton_select_lt_A(x5, x5_lt_A) <<  8;
    const uint32_t n6 = xton_select_lt_A(x6, x6_lt_A) <<  4;
    const uint32_t n7 = xton_select_lt_A(x7, x7_lt_A);

    return (n0 | n1 | n2 | n3 | n4 | n5 | n6 | n7);
}

/* convert string of 4 ASCII hex chars to unsigned 16-bit value
 * (used to convert architecture-independent string data to numerical data)
 * (operate in 32-bit intermediates to use more native sizes on modern CPUs)
 * (use when hex chars are known 0..9 A..F a..f) */
uint16_t
uint16_from_ascii4hex(const char * const restrict buf)
{
    const unsigned char * const b = (const unsigned char *)buf;
    const uint32_t x0 = b[0];
    const uint32_t x1 = b[1];
    const uint32_t x2 = b[2];
    const uint32_t x3 = b[3];

    const uint32_t x0_lt_A = (uint32_t)int32_x_lt_y_returns_all_ones(x0,'A');
    const uint32_t x1_lt_A = (uint32_t)int32_x_lt_y_returns_all_ones(x1,'A');
    const uint32_t x2_lt_A = (uint32_t)int32_x_lt_y_returns_all_ones(x2,'A');
    const uint32_t x3_lt_A = (uint32_t)int32_x_lt_y_returns_all_ones(x3,'A');

    const uint32_t n0 = xton_select_lt_A(x0, x0_lt_A) << 12;
    const uint32_t n1 = xton_select_lt_A(x1, x1_lt_A) <<  8;
    const uint32_t n2 = xton_select_lt_A(x2, x2_lt_A) <<  4;
    const uint32_t n3 = xton_select_lt_A(x3, x3_lt_A);

    return (uint16_t)(n0 | n1 | n2 | n3);
}
