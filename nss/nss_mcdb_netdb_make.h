/*
 * nss_mcdb_netdb_make - mcdb of hosts,protocols,netgroup,networks,services,rpc
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

#ifndef INCLUDED_NSS_MCDB_NETDB_MAKE_H
#define INCLUDED_NSS_MCDB_NETDB_MAKE_H

#include "../plasma/plasma_feature.h"
#include "../plasma/plasma_attr.h"
#include "../plasma/plasma_stdtypes.h"
#include "nss_mcdb_make.h"
PLASMA_ATTR_Pragma_once

#include <netdb.h>
struct rpcent;      /* (forward declaration) */

#if 0
/* buf size 1K + NSS_HE_HDRSZ is probably reasonable */
__attribute_nonnull__
size_t
nss_mcdb_netdb_make_hostent_datastr(char * restrict, const size_t,
				    const struct hostent * const restrict);

/* buf size 1K + NSS_NE_HDRSZ is probably reasonable */
#ifndef _AIX
__attribute_nonnull__
size_t
nss_mcdb_netdb_make_netent_datastr(char * restrict, const size_t,
			           const struct netent * const restrict);
#else
#include "nss_mcdb_netdb.h" /*(for struct nwent on AIX)*/
__attribute_nonnull__
size_t
nss_mcdb_netdb_make_netent_datastr(char * restrict, const size_t,
			           const struct nwent * const restrict);
#endif

/* buf size 1K + NSS_PE_HDRSZ is probably reasonable */
__attribute_nonnull__
size_t
nss_mcdb_netdb_make_protoent_datastr(char * restrict, const size_t,
				     const struct protoent * const restrict);

/* buf size 1K + NSS_RE_HDRSZ is probably reasonable */
__attribute_nonnull__
size_t
nss_mcdb_netdb_make_rpcent_datastr(char * restrict, const size_t,
			           const struct rpcent * const restrict);

/* buf size 1K + NSS_SE_HDRSZ is probably reasonable */
__attribute_nonnull__
size_t
nss_mcdb_netdb_make_servent_datastr(char * restrict, const size_t,
				    const struct servent * const restrict);
#endif


__attribute_nonnull__
bool
nss_mcdb_netdb_make_hostent_encode(
  struct nss_mcdb_make_winfo * const restrict,
  const void * const);

__attribute_nonnull__
bool
nss_mcdb_netdb_make_netent_encode(
  struct nss_mcdb_make_winfo * const restrict,
  const void * const);

__attribute_nonnull_x__((1))
bool
nss_mcdb_netdb_make_netgrent_encode(
  struct nss_mcdb_make_winfo * const restrict,
  const void * const);

__attribute_nonnull__
bool
nss_mcdb_netdb_make_protoent_encode(
  struct nss_mcdb_make_winfo * const restrict,
  const void * const);

__attribute_nonnull__
bool
nss_mcdb_netdb_make_rpcent_encode(
  struct nss_mcdb_make_winfo * const restrict,
  const void * const);

__attribute_nonnull__
bool
nss_mcdb_netdb_make_servent_encode(
  struct nss_mcdb_make_winfo * const restrict,
  const void * const);


__attribute_nonnull__
bool
nss_mcdb_netdb_make_hosts_parse(
  struct nss_mcdb_make_winfo * const restrict, char * restrict, size_t);

__attribute_nonnull__
bool
nss_mcdb_netdb_make_networks_parse(
  struct nss_mcdb_make_winfo * const restrict, char * restrict, size_t);

__attribute_nonnull__
bool
nss_mcdb_netdb_make_netgroup_parse(
  struct nss_mcdb_make_winfo * const restrict, char * restrict, size_t);

__attribute_nonnull__
bool
nss_mcdb_netdb_make_protocols_parse(
  struct nss_mcdb_make_winfo * const restrict, char * restrict, size_t);

__attribute_nonnull__
bool
nss_mcdb_netdb_make_rpc_parse(
  struct nss_mcdb_make_winfo * const restrict, char * restrict, size_t);

__attribute_nonnull__
bool
nss_mcdb_netdb_make_services_parse(
  struct nss_mcdb_make_winfo * const restrict, char * restrict, size_t);


#endif
