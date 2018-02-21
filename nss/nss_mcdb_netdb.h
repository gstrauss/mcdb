/*
 * nss_mcdb_netdb - query mcdb of hosts,protocols,netgroup,networks,services,rpc
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

#ifndef INCLUDED_NSS_MCDB_NETDB_H
#define INCLUDED_NSS_MCDB_NETDB_H

/* _BSD_SOURCE or _SVID_SOURCE for struct rpcent on Linux */
#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
/* _DARWIN_C_SOURCE for struct rpcent on Darwin */
#ifndef PLASMA_FEATURE_ENABLE_BSD_SOURCE_TO_DARWIN_C_SOURCE
#define PLASMA_FEATURE_ENABLE_BSD_SOURCE_TO_DARWIN_C_SOURCE
#endif

#include "../plasma/plasma_feature.h"
#include "../plasma/plasma_attr.h"
#include "../plasma/plasma_stdtypes.h"
#include "nss_mcdb.h"
PLASMA_ATTR_Pragma_once

#include <netdb.h>
#ifdef __sun
#include <rpc/rpcent.h>
#endif
#ifdef __hpux
struct rpcent {
  char *r_name;
  char **r_aliases;
  int r_number;
};
#endif
#ifdef _AIX
struct rpcent;      /* (forward declaration to avoid _ALL_SOURCE in header) */
struct nwent {
  char *n_name;     /* official name of net *//*(AIX example has field 'name')*/
  char **n_aliases; /* alias list */
  int n_addrtype;   /* net address type */
  void *n_addr;     /* network address */
  int n_length;     /* address length, in bits */
};
#endif

enum {
  NSS_H_ADDRTYPE =  0,
  NSS_H_LENGTH   =  4,
  NSS_HE_MEM_STR =  8,
  NSS_HE_LST_STR = 10,
  NSS_HE_MEM     = 12,
  NSS_HE_MEM_NUM = 14,
  NSS_HE_LST_NUM = 16,
  NSS_HE_HDRSZ   = 20   /*(must be multiple of 4; round up)*/
};

enum {
  NSS_NG_HDRSZ   =  0   /*(must be multiple of 4; round up)*/
};

enum {
  NSS_N_NET      =  0,
  NSS_N_ADDRTYPE =  4,
  NSS_NE_MEM_STR =  8,
  NSS_NE_MEM     = 10,
  NSS_NE_MEM_NUM = 12,
 #ifdef _AIX
  NSS_N_LENGTH   = 14,
 #endif
  NSS_NE_HDRSZ   = 16   /*(must be multiple of 4; round up)*/
};

enum {
  NSS_P_PROTO    =  0,
  NSS_PE_MEM_STR =  4,
  NSS_PE_MEM     =  6,
  NSS_PE_MEM_NUM =  8,
  NSS_PE_HDRSZ   = 12   /*(must be multiple of 4; round up)*/
};

enum {
  NSS_R_NUMBER   =  0,
  NSS_RE_MEM_STR =  4,
  NSS_RE_MEM     =  6,
  NSS_RE_MEM_NUM =  8,
  NSS_RE_HDRSZ   = 12   /*(must be multiple of 4; round up)*/
};

enum {
  NSS_S_PORT     =  0,
  NSS_S_NAME     =  4,
  NSS_SE_MEM_STR =  6,
  NSS_SE_MEM     =  8,
  NSS_SE_MEM_NUM = 10,
  NSS_SE_HDRSZ   = 12   /*(must be multiple of 4)*/
};

EXPORT void _nss_mcdb_sethostent(int);
EXPORT void _nss_mcdb_endhostent(void);
EXPORT void _nss_mcdb_setnetent(int);
EXPORT void _nss_mcdb_endnetent(void);
EXPORT void _nss_mcdb_setprotoent(int);
EXPORT void _nss_mcdb_endprotoent(void);
EXPORT void _nss_mcdb_setrpcent(int);
EXPORT void _nss_mcdb_endrpcent(void);
EXPORT void _nss_mcdb_setservent(int);
EXPORT void _nss_mcdb_endservent(void);

__attribute_nonnull__
__attribute_warn_unused_result__
EXPORT nss_status_t
_nss_mcdb_gethostent_r(struct hostent * restrict, char * restrict, size_t,
                       int * restrict, int * restrict);

__attribute_nonnull__
__attribute_warn_unused_result__
EXPORT nss_status_t
_nss_mcdb_gethostbyname2_r(const char * restrict, int,
                           struct hostent * restrict, char * restrict, size_t,
                           int * restrict, int * restrict);

__attribute_nonnull__
__attribute_warn_unused_result__
EXPORT nss_status_t
_nss_mcdb_gethostbyname_r(const char * restrict,
                          struct hostent * restrict, char * restrict, size_t,
                          int * restrict, int * restrict);

__attribute_nonnull__
__attribute_warn_unused_result__
EXPORT nss_status_t
_nss_mcdb_gethostbyaddr_r(const void * restrict, socklen_t, int,
                          struct hostent * restrict, char * restrict, size_t,
                          int * restrict, int * restrict);

#ifdef __GLIBC__

struct __netgrent; /*(declaration)*/

__attribute_nonnull__
__attribute_warn_unused_result__
EXPORT int _nss_mcdb_setnetgrent(const char * restrict,
                                 struct __netgrent * restrict);
EXPORT void _nss_mcdb_endnetgrent(struct __netgrent * restrict);

__attribute_nonnull__
__attribute_warn_unused_result__
EXPORT nss_status_t
_nss_mcdb_getnetgrent_r(struct __netgrent * restrict, char * restrict, size_t,
                        int * restrict);

#else

__attribute_nonnull__
__attribute_warn_unused_result__
EXPORT int _nss_mcdb_setnetgrent(const char * restrict);
EXPORT void _nss_mcdb_endnetgrent(void);

__attribute_nonnull__
__attribute_warn_unused_result__
EXPORT nss_status_t
_nss_mcdb_getnetgrent_r(char ** restrict, char ** restrict, char ** restrict,
                        char * restrict, size_t, int * restrict);

#endif

__attribute_nonnull__
__attribute_warn_unused_result__
EXPORT nss_status_t
_nss_mcdb_innetgr(const char * restrict, const char * restrict,
                  const char * restrict, const char * restrict,
                  char * restrict, size_t, int * restrict);

__attribute_nonnull__
__attribute_warn_unused_result__
EXPORT nss_status_t
_nss_mcdb_getnetent_r(struct netent * restrict, char * restrict, size_t,
                      int * restrict, int * restrict);

__attribute_nonnull__
__attribute_warn_unused_result__
EXPORT nss_status_t
_nss_mcdb_getnetbyname_r(const char * restrict,
                         struct netent * restrict, char * restrict, size_t,
                         int * restrict, int * restrict);

__attribute_nonnull__
__attribute_warn_unused_result__
EXPORT nss_status_t
_nss_mcdb_getnetbyaddr_r(const uint32_t, int,
                         struct netent * restrict, char * restrict, size_t,
                         int * restrict, int * restrict);

__attribute_nonnull__
__attribute_warn_unused_result__
EXPORT nss_status_t
_nss_mcdb_getprotoent_r(struct protoent * restrict, char * restrict, size_t,
                        int * restrict);

__attribute_nonnull__
__attribute_warn_unused_result__
EXPORT nss_status_t
_nss_mcdb_getprotobyname_r(const char * restrict,
                           struct protoent * restrict, char * restrict, size_t,
                           int * restrict);

__attribute_nonnull__
__attribute_warn_unused_result__
EXPORT nss_status_t
_nss_mcdb_getprotobynumber_r(int,
                             struct protoent * restrict, char * restrict,size_t,
                             int * restrict);

__attribute_nonnull__
__attribute_warn_unused_result__
EXPORT nss_status_t
_nss_mcdb_getrpcent_r(struct rpcent * restrict, char * restrict, size_t,
                      int * restrict);

__attribute_nonnull__
__attribute_warn_unused_result__
EXPORT nss_status_t
_nss_mcdb_getrpcbyname_r(const char * restrict,
                         struct rpcent * restrict, char * restrict, size_t,
                         int * restrict);

__attribute_nonnull__
__attribute_warn_unused_result__
EXPORT nss_status_t
_nss_mcdb_getrpcbynumber_r(int,
                           struct rpcent * restrict, char * restrict, size_t,
                           int * restrict);

__attribute_nonnull__
__attribute_warn_unused_result__
EXPORT nss_status_t
_nss_mcdb_getservent_r(struct servent * restrict, char * restrict, size_t,
                       int * restrict);

__attribute_nonnull_x__((1,3,4,6))
__attribute_warn_unused_result__
EXPORT nss_status_t
_nss_mcdb_getservbyname_r(const char * restrict, const char * restrict,
                          struct servent * restrict, char * restrict, size_t,
                          int * restrict);

__attribute_nonnull_x__((3,4,6))
__attribute_warn_unused_result__
EXPORT nss_status_t
_nss_mcdb_getservbyport_r(int, const char * restrict,
                          struct servent * restrict, char * restrict, size_t,
                          int * restrict);


#endif
