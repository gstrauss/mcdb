/*
 * plasma_endian - portability macros for byteorder conversion
 *
 *   inline assembly and compiler intrinsics for byteorder conversion
 *   (little endian <=> big endian)
 *
 *
 * Copyright (c) 2012, Glue Logic LLC. All rights reserved. code()gluelogic.com
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

#define PLASMA_ENDIAN_C99INLINE_FUNCS

/* inlined functions defined in header
 * (generate external linkage definition in GCC versions earlier than GCC 4.3)*/
#if defined(NO_C99INLINE) \
 || defined(__clang__) || (defined(__GNUC__) && !defined(__GNUC_STDC_INLINE__))
#define PLASMA_ENDIAN_C99INLINE
#endif

#include "plasma_endian.h"

/* inlined functions defined in header
 * (generate external linkage definition in C99-compliant compilers)
 * (need to -duplicate- definition from header for non-C99-compliant compiler)
 */
#if !defined(__GNUC__) || defined(__GNUC_STDC_INLINE__)
#if defined(plasma_endian_swap16_func) \
 &&!defined(plasma_endian_swap16_funcmacro)
extern inline
uint16_t plasma_endian_swap16_func (uint16_t);
uint16_t plasma_endian_swap16_func (uint16_t);
#endif
#if defined(plasma_endian_swap16p_func) \
 &&!defined(plasma_endian_swap16p_funcmacro)
extern inline
uint16_t plasma_endian_swap16p_func (const uint16_t * restrict);
uint16_t plasma_endian_swap16p_func (const uint16_t * restrict);
#endif
#if defined(plasma_endian_swap32_func) \
 &&!defined(plasma_endian_swap32_funcmacro)
extern inline
uint32_t plasma_endian_swap32_func (uint32_t);
uint32_t plasma_endian_swap32_func (uint32_t);
#endif
#if defined(plasma_endian_swap32p_func) \
 &&!defined(plasma_endian_swap32p_funcmacro)
extern inline
uint32_t plasma_endian_swap32p_func (const uint32_t * restrict);
uint32_t plasma_endian_swap32p_func (const uint32_t * restrict);
#endif
#if defined(plasma_endian_swap64_func) \
 &&!defined(plasma_endian_swap64_funcmacro)
extern inline
uint64_t plasma_endian_swap64_func (uint64_t);
uint64_t plasma_endian_swap64_func (uint64_t);
#endif
#if defined(plasma_endian_swap64p_func) \
 &&!defined(plasma_endian_swap64p_funcmacro)
extern inline
uint64_t plasma_endian_swap64p_func (const uint64_t * restrict);
uint64_t plasma_endian_swap64p_func (const uint64_t * restrict);
#endif
#endif


#ifdef __sparc
#if (defined(__SUNPRO_C)  && __SUNPRO_C  < 0x5100) \
 || (defined(__SUNPRO_CC) && __SUNPRO_CC < 0x5100) /* Sun Studio < 12.1 */

void
plasma_endian_SPARC_asm (void)
{
    /* SPARC Assembly Language Reference Manual
     * http://docs.oracle.com/cd/E26502_01/html/E28387/gmadm.html */

    __asm__(".global plasma_endian_swap32_SPARC\n"
            ".type plasma_endian_swap32_SPARC,#function\n"
            "plasma_endian_swap32_SPARC:\n"
            "retl\n"
            "lduwa [%o0] 0x88, %o0\n");

    __asm__(".global plasma_endian_swap64_SPARC\n"
            ".type plasma_endian_swap64_SPARC,#function\n"
            "plasma_endian_swap64_SPARC:\n"
            "retl\n"
            "ldxa  [%o0] 0x88, %o0\n");
}
PLASMA_ATTR_Pragma_rarely_called(plasma_endian_SPARC_asm)

#endif
#endif
