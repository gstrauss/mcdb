/*
 * nss_mcdb - common routines to query mcdb of nsswitch.conf databases
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

#ifndef INCLUDED_NSS_MCDB_H
#define INCLUDED_NSS_MCDB_H

#include "../plasma/plasma_feature.h"
#include "../plasma/plasma_attr.h"
#include "../plasma/plasma_stdtypes.h"
#include "../mcdb.h"
PLASMA_ATTR_Pragma_once

#if defined(__linux__)
  #include <nss.h>   /* NSS_STATUS_{TRYAGAIN,UNAVAIL,NOTFOUND,SUCCESS,RETURN} */
  typedef enum nss_status nss_status_t;
#elif defined(__sun)
  #include <nss_common.h> /* NSS_{TRYAGAIN,UNAVAIL,NOTFOUND,SUCCESS,RETURN} */
  #include <nss_dbdefs.h>
  #define NSS_STATUS_SUCCESS  NSS_SUCCESS
  #define NSS_STATUS_NOTFOUND NSS_NOTFOUND
  #define NSS_STATUS_TRYAGAIN NSS_TRYAGAIN
  #define NSS_STATUS_UNAVAIL  NSS_UNAVAIL
  #define NSS_STATUS_RETURN   NSS_NOTFOUND
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
  #include <nsswitch.h>
  typedef enum nss_status {
    NSS_STATUS_TRYAGAIN = NS_TRYAGAIN,
    NSS_STATUS_UNAVAIL  = NS_UNAVAIL,
    NSS_STATUS_NOTFOUND = NS_NOTFOUND,
    NSS_STATUS_SUCCESS  = NS_SUCCESS,
    NSS_STATUS_RETURN   = NS_RETURN
  } nss_status_t;
  /* TODO: FreeBSD: create wrapper interfaces that catch return value
   * and translate NSS_STATUS_TRYAGAIN to NS_RETURN when errno == ERANGE
   * See FreeBSD /usr/include/nss.h: __nss_compat_result(rv, err)
   * Register the wrapper interfaces with FreeBSD nss_module_register(). */
#elif defined(__hpux)
  //#include <nsswitch/hp_nss_common.h>
  //#include <nsswitch/hp_nss_dbdefs.h>
  #include <nsswitch.h>
  typedef enum nss_status {
    NSS_STATUS_TRYAGAIN = __NSW_TRYAGAIN,
    NSS_STATUS_UNAVAIL  = __NSW_UNAVAIL,
    NSS_STATUS_NOTFOUND = __NSW_NOTFOUND,
    NSS_STATUS_SUCCESS  = __NSW_SUCCESS,
    NSS_STATUS_RETURN   = __NSW_NOTFOUND
  } nss_status_t;
#else
  typedef enum nss_status {
    NSS_STATUS_TRYAGAIN = -2,
    NSS_STATUS_UNAVAIL  = -1,
    NSS_STATUS_NOTFOUND =  0,
    NSS_STATUS_SUCCESS  =  1,
    NSS_STATUS_RETURN   =  0
  } nss_status_t;
#endif

/* enum nss_dbtype index must match path in nss_mcdb.c char *_nss_dbnames[] */
enum nss_dbtype {
    NSS_DBTYPE_ALIASES = 0,  /* first enum element here must always be 0 */
    NSS_DBTYPE_ETHERS,
    NSS_DBTYPE_GROUP,
    NSS_DBTYPE_HOSTS,
    NSS_DBTYPE_NETGROUP,
    NSS_DBTYPE_NETWORKS,
    NSS_DBTYPE_PASSWD,
    NSS_DBTYPE_PROTOCOLS,
    NSS_DBTYPE_PUBLICKEY,
    NSS_DBTYPE_RPC,
    NSS_DBTYPE_SERVICES,
    NSS_DBTYPE_SHADOW,
    NSS_DBTYPE_SENTINEL      /* sentinel must always be final enum element */
};

/* To maintain a consistent view of a database, set*ent() opens a session to
 * the database and it is held open until end*ent() is called.  This session
 * is maintained in thread-local storage.  get*() queries other than get*ent()
 * obtain and release a session to the database upon every call.  To avoid the
 * locking overhead, thread can call set*ent() and then can make (many) get*()
 * calls in the same session.  When done, end*ent() should be called to release
 * the session.  If the set*ent() ... end*ent() routines are used in a threaded
 * program, care should be taken to ensure other threads are *not* in process
 * of accessing the mcdb when end*ent() is called, or else crashes are likely
 * if the mcdb is updated. */

struct nss_mcdb_vinfo {
  /* fail with errno = ERANGE if insufficient buf space supplied */
  nss_status_t (* const decode)(struct mcdb * restrict,
                                const struct nss_mcdb_vinfo * restrict);
  void * const restrict vstruct;
  char * const restrict buf;
  const size_t bufsz;
  int  * const restrict errnop;
  const char * const restrict key;
  const size_t klen;
  const unsigned char tagc;
};


INTERNAL nss_status_t
nss_mcdb_setent(enum nss_dbtype, int);

INTERNAL nss_status_t
nss_mcdb_endent(enum nss_dbtype);

/* _nss_mcdb_get*ent() walks db returning successive keys with '=' tag char */
__attribute_nonnull__
__attribute_warn_unused_result__
INTERNAL nss_status_t
nss_mcdb_getent(enum nss_dbtype,
                const struct nss_mcdb_vinfo * restrict);

/* similar to mcdb_findstart and mcdb_findnext; used for netdb netgroups */
__attribute_nonnull__
__attribute_warn_unused_result__
INTERNAL nss_status_t
nss_mcdb_getentstart(enum nss_dbtype,
                     const struct nss_mcdb_vinfo * restrict);

__attribute_nonnull__
__attribute_warn_unused_result__
INTERNAL nss_status_t
nss_mcdb_getnetgrentnext(const struct nss_mcdb_vinfo * const restrict v);

#if 0  /* implemented, but not enabling by default; often used only with NIS+ */
__attribute_nonnull__
__attribute_warn_unused_result__
INTERNAL nss_status_t
nss_mcdb_getentnext(enum nss_dbtype,
                    const struct nss_mcdb_vinfo * restrict);
#endif

/* _nss_mcdb_get*by*() generic queries */
__attribute_nonnull__
__attribute_warn_unused_result__
INTERNAL nss_status_t
nss_mcdb_get_generic(enum nss_dbtype,
                     const struct nss_mcdb_vinfo * restrict);

__attribute_nonnull__
__attribute_warn_unused_result__
INTERNAL nss_status_t
nss_mcdb_buf_decode(struct mcdb * restrict,
                    const struct nss_mcdb_vinfo * restrict);


EXPORT bool
nss_mcdb_refresh_check(enum nss_dbtype);


#endif
