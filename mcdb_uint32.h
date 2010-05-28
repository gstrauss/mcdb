/* License: GPLv3 */

#ifndef INCLUDED_MCDB_UINT32_H
#define INCLUDED_MCDB_UINT32_H

#include "mcdb_attribute.h"

#include <stdint.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t
mcdb_uint32_unpack(const char s[4])
  __attribute_pure__  __attribute_warn_unused_result__;

uint32_t
mcdb_uint32_unpack_bigendian(const char s[4])
  __attribute_pure__  __attribute_warn_unused_result__;

void
mcdb_uint32_pack(char s[4], uint32_t);

void
mcdb_uint32_pack_bigendian(char s[4], uint32_t);

/*(use macros only with simple args, or else better to call subroutines)*/

/* mcdb_uint32_unpack_macro(): string 's' must be unsigned char */
#define mcdb_uint32_unpack_macro(s) \
  ( (s)[0] | ((s)[1]<<8) | ((s)[2]<<16) | ((s)[3]<<24) )

/* mcdb_uint32_unpack_big_macro(): string 's' must be unsigned char */
#define mcdb_uint32_unpack_bigendian_macro(s) \
  ( ((s)[0]<<24) | ((s)[1]<<16) | ((s)[2]<<8) | (s)[3] )

#define mcdb_uint32_pack_macro(s,u) \
  (s)[0]=u & 0xFF; (s)[1]=(u>>8) & 0xFF; (s)[2]=(u>>16) & 0xFF; (s)[3]=(u>>24)

#define mcdb_uint32_pack_bigendian_macro(s,u) \
  (s)[0]=(u>>24); (s)[1]=(u>>16) & 0xFF; (s)[2]=(u>>8) & 0xFF; (s)[3]=u & 0xFF


#include <arpa/inet.h>  /* htonl(), ntohl() */
#define mcdb_uint32_pack_bigendian_aligned_macro(s,u) \
   (*((uint32_t *)(s)) = htonl(u))
#define mcdb_uint32_unpack_bigendian_aligned_macro(s) \
   ntohl(*((uint32_t *)(s)))


char *
mcdb_uint32_to_ascii8uphex(const uint32_t n, char * restrict buf)
  __attribute_nonnull__;

char *
mcdb_uint16_to_ascii4uphex(const uint32_t n, char * restrict buf)
  __attribute_nonnull__;


#ifdef __cplusplus
}
#endif

#endif
