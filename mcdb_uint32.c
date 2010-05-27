/* License: GPLv3 */

#include "mcdb_uint32.h"

uint32_t
mcdb_uint32_unpack(const char s[4])
{
    const unsigned char * const restrict n = (const unsigned char *)s;
    return mcdb_uint32_unpack_macro(n);
}

uint32_t
mcdb_uint32_unpack_bigendian(const char s[4])
{
    const unsigned char * const restrict n = (const unsigned char *)s;
    return mcdb_uint32_unpack_bigendian_macro(n);
}

void
mcdb_uint32_pack(char s[4], const uint32_t u)
{
    mcdb_uint32_pack_macro(s,u);
}

void
mcdb_uint32_pack_bigendian(char s[4], const uint32_t u)
{
    mcdb_uint32_pack_bigendian_macro(s,u);
}


/* GPS: TODO take pipelined versions from my cdbauthz.c */
/* GPS: TODO uint32_from_ascii8uphex, uint16_from_ascii4uphex */
/* experiment with C99 static inline in header */
char *  __attribute_noinline__
uint32_to_ascii8uphex(const uint32_t n, char * restrict buf)
{
    uint32_t nibble;
    uint32_t i = 0;
    do {
        nibble = (n << (i<<2)) >> 28; /* isolate 4 bits */
        *buf++ = nibble + (nibble < 10 ? '0': 'A'-10);
    } while (++i < 8);
    return buf;
}
char *  __attribute_noinline__
uint16_to_ascii4uphex(const uint32_t n, char * restrict buf)
{
    uint32_t nibble;
    uint32_t i = 4;
    do {
        nibble = (n << (i<<2)) >> 28; /* isolate 4 bits */
        *buf++ = nibble + (nibble < 10 ? '0': 'A'-10);
    } while (++i < 8);
    return buf;
}
