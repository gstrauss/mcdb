#ifndef INCLUDED_NSS_MCDB_MISC_MAKE_H
#define INCLUDED_NSS_MCDB_MISC_MAKE_H

#include "nss_mcdb_make.h"
#include "code_attributes.h"

#include <aliases.h>

/* buf size 1K + NSS_AE_HDRSZ is probably reasonable */
size_t
nss_mcdb_misc_make_aliasent_datastr(char * restrict buf, const size_t bufsz,
				    const struct aliasent * const restrict ae)
  __attribute_nonnull__;

/* buf size 1K + NSS_EA_HDRSZ is probably reasonable */
size_t
nss_mcdb_misc_make_ether_addr_datastr(char * restrict buf, const size_t bufsz,
				      const void * const restrict entp,
				      const char * const restrict hostname)
  __attribute_nonnull__;


bool
nss_mcdb_misc_make_aliasent_encode(
  struct nss_mcdb_make_winfo * const restrict w,
  const void * const restrict entp)
  __attribute_nonnull__;

bool
nss_mcdb_misc_make_ether_addr_encode(
  struct nss_mcdb_make_winfo * const restrict w,
  const void * const restrict entp)
  __attribute_nonnull__;


bool
nss_mcdb_misc_make_ethers_parse(
  struct nss_mcdb_make_winfo * const restrict w, char * restrict p)
  __attribute_nonnull__;

bool
nss_mcdb_misc_make_aliases_parse(
  struct nss_mcdb_make_winfo * const restrict w, char * restrict p)
  __attribute_nonnull__;


#endif
