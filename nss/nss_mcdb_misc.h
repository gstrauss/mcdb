/*
 * nss_mcdb_misc - query mcdb of aliases, ethers, publickey, secretkey
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

#ifndef INCLUDED_NSS_MCDB_MISC_H
#define INCLUDED_NSS_MCDB_MISC_H

/* _BSD_SOURCE or _SVID_SOURCE for struct ether_addr on Linux */
#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include "../plasma/plasma_feature.h"
#include "../plasma/plasma_attr.h"
#include "../plasma/plasma_stdtypes.h"
#include "nss_mcdb.h"
PLASMA_ATTR_Pragma_once

/* TODO: implemented only on Linux; other platforms incomplete/missing */
#if defined(__linux__)
#include <aliases.h>        /* (struct aliasent) */
#include <netinet/ether.h>  /* (struct ether_addr) */
#elif defined(__sun)
#include <sys/ethernet.h>   /* (struct ether_addr) */
#endif

enum {
  NSS_AE_LOCAL   =  0,
  NSS_AE_MEM_STR =  4,
  NSS_AE_MEM     =  6,
  NSS_AE_MEM_NUM =  8,
  NSS_AE_HDRSZ   = 12   /*(must be multiple of 4; round up)*/
};

enum {
  NSS_EA_HDRSZ   =  6
};

EXPORT void _nss_mcdb_setaliasent(void);
EXPORT void _nss_mcdb_endaliasent(void);
EXPORT void _nss_mcdb_setetherent(void);
EXPORT void _nss_mcdb_endetherent(void);

__attribute_nonnull__
__attribute_warn_unused_result__
EXPORT nss_status_t
_nss_mcdb_getaliasent_r(struct aliasent * restrict,
                        char * restrict, size_t, int * restrict);

__attribute_nonnull__
__attribute_warn_unused_result__
EXPORT nss_status_t
_nss_mcdb_getaliasbyname_r(const char * restrict, struct aliasent * restrict,
                           char * restrict, size_t, int * restrict);

__attribute_nonnull__
__attribute_warn_unused_result__
EXPORT nss_status_t
_nss_mcdb_getpublickey(const char * restrict,
                       char * restrict, size_t, int * restrict);

__attribute_nonnull__
__attribute_warn_unused_result__
EXPORT nss_status_t
_nss_mcdb_getsecretkey(const char * restrict, const char * restrict,
                       char * restrict, size_t, int * restrict);

__attribute_nonnull__
__attribute_warn_unused_result__
EXPORT nss_status_t
_nss_mcdb_getetherent_r(struct ether_addr * restrict, char * restrict, size_t,
                        int * restrict);

__attribute_nonnull__
__attribute_warn_unused_result__
EXPORT nss_status_t
_nss_mcdb_gethostton_r(const char * restrict, struct ether_addr * restrict,
                       int * restrict);

__attribute_nonnull__
__attribute_warn_unused_result__
EXPORT nss_status_t
_nss_mcdb_getntohost_r(struct ether_addr * restrict, char * restrict, size_t,
                       int * restrict);


#endif
