#ifndef INCLUDED_NSS_MCDB_NETDB_MAKE_H
#define INCLUDED_NSS_MCDB_NETDB_MAKE_H

#include "code_attributes.h"

#include <netdb.h>

/* buf size 1K + NSS_EA_HDRSZ is probably reasonable */
size_t
cdb_he2str(char * restrict buf, const size_t bufsz,
           const struct hostent * const restrict he)
  __attribute_nonnull__;

/* buf size 1K + NSS_EA_HDRSZ is probably reasonable */
size_t
cdb_ne2str(char * restrict buf, const size_t bufsz,
           const struct netent * const restrict ne)
  __attribute_nonnull__;

/* buf size 1K + NSS_EA_HDRSZ is probably reasonable */
size_t
cdb_pe2str(char * restrict buf, const size_t bufsz,
           const struct protoent * const restrict pe)
  __attribute_nonnull__;

/* buf size 1K + NSS_EA_HDRSZ is probably reasonable */
size_t
cdb_re2str(char * restrict buf, const size_t bufsz,
           const struct rpcent * const restrict re)
  __attribute_nonnull__;
size_t

/* buf size 1K + NSS_EA_HDRSZ is probably reasonable */
cdb_se2str(char * restrict buf, const size_t bufsz,
           const struct servent * const restrict se)
  __attribute_nonnull__;

#endif
