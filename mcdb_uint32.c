/* License: GPLv3 */

/* inlined functions defined in header
 * (generate external linkage definition in GCC versions earlier than GCC 4.3)
 * (mcdb_uint32.h does not include other headers defining other inline functions
 *  in header, so simply disable C99INLINE to generate external linkage
 *  definition for all inlined functions seen (i.e. those in mcdb_uint32.h))
 */
#if defined(__GNUC__) && !defined(__GNUC_STDC_INLINE__)
#define C99INLINE
#endif

#include "mcdb_uint32.h"

/* inlined functions defined in header
 * (generate external linkage definition in C99-compliant compilers)
 * (Would need to repeat definition in header for non-C99-compliant compiler)
 */
#if !defined(__GNUC__) || defined(__GNUC_STDC_INLINE__)
extern inline uint32_t
mcdb_uint32_unpack(const char s[4]);
uint32_t
mcdb_uint32_unpack(const char s[4]);
extern inline uint32_t
mcdb_uint32_unpack_bigendian(const char s[4]);
uint32_t
mcdb_uint32_unpack_bigendian(const char s[4]);
extern inline void
mcdb_uint32_pack(char s[4], uint32_t);
void
mcdb_uint32_pack(char s[4], uint32_t);
extern inline void
mcdb_uint32_pack_bigendian(char s[4], uint32_t);
void
mcdb_uint32_pack_bigendian(char s[4], uint32_t);
#endif


/* GPS: TODO take pipelined versions from my cdbauthz.c */
/* GPS: TODO uint32_from_ascii8uphex, uint16_from_ascii4uphex */
char *
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
char *
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
