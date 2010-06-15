#ifndef INCLUDED_NSS_MCDB_NETDB_MAKE_H
#define INCLUDED_NSS_MCDB_NETDB_MAKE_H

#include "code_attributes.h"

#include <netdb.h>

/* buf size 1K + NSS_HE_HDRSZ is probably reasonable */
size_t
nss_mcdb_netdb_make_hostent_datastr(char * restrict buf, const size_t bufsz,
				    const struct hostent * const restrict he)
  __attribute_nonnull__;

/* buf size 1K + NSS_NE_HDRSZ is probably reasonable */
size_t
nss_mcdb_netdb_make_netent_datastr(char * restrict buf, const size_t bufsz,
			           const struct netent * const restrict ne)
  __attribute_nonnull__;

/* buf size 1K + NSS_PE_HDRSZ is probably reasonable */
size_t
nss_mcdb_netdb_make_protoent_datastr(char * restrict buf, const size_t bufsz,
				     const struct protoent * const restrict pe)
  __attribute_nonnull__;

/* buf size 1K + NSS_RE_HDRSZ is probably reasonable */
size_t
nss_mcdb_netdb_make_rpcent_datastr(char * restrict buf, const size_t bufsz,
			           const struct rpcent * const restrict re)
  __attribute_nonnull__;
size_t

/* buf size 1K + NSS_SE_HDRSZ is probably reasonable */
nss_mcdb_netdb_make_servent_datastr(char * restrict buf, const size_t bufsz,
				    const struct servent * const restrict se)
  __attribute_nonnull__;

#endif
