/*
 * nss_mcdb_misc_make - mcdb of aliases, ethers, publickey, secretkey
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

#ifndef INCLUDED_NSS_MCDB_MISC_MAKE_H
#define INCLUDED_NSS_MCDB_MISC_MAKE_H

#include "nss_mcdb_make.h"
#include "code_attributes.h"

/*#include <aliases.h>*/    /* not portable; see nss_mcdb_misc.h */
#include "nss_mcdb_misc.h"  /* (centralize misc headers between platforms) */

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
