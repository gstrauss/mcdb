#ifndef INCLUDED_NSS_MCDB_MISC_MAKE_H
#define INCLUDED_NSS_MCDB_MISC_MAKE_H

/* _BSD_SOURCE or _SVID_SOURCE for struct ether_addr on Linux */
/* (future: would prefer not to have to define _BSD_SOURCE in header) */
#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif

#include "code_attributes.h"

#include <aliases.h>
#include <netinet/ether.h>

/* buf size 1K + NSS_AE_HDRSZ is probably reasonable */
size_t
cdb_ae2str(char * restrict buf, const size_t bufsz,
           const struct aliasent * const restrict ae)
  __attribute_nonnull__;

/* buf size 1K + NSS_EA_HDRSZ is probably reasonable */
size_t
cdb_ea2str(char * restrict buf, const size_t bufsz,
           const struct ether_addr * const restrict ea,
           const char * const restrict hostname)
  __attribute_nonnull__;

#endif
