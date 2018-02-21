/*
 * nss_mcdb_authn - query mcdb of shadow nsswitch.conf database
 *
 * Copyright (c) 2011, Glue Logic LLC. All rights reserved. code()gluelogic.com
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

#ifndef INCLUDED_NSS_MCDB_AUTHN_H
#define INCLUDED_NSS_MCDB_AUTHN_H

#include "../plasma/plasma_feature.h"
#include "../plasma/plasma_attr.h"
#include "../plasma/plasma_stdtypes.h"
#include "nss_mcdb.h"
PLASMA_ATTR_Pragma_once


EXPORT void _nss_mcdb_setspent(void);
EXPORT void _nss_mcdb_endspent(void);

enum {
  NSS_SP_LSTCHG  =  0,
  NSS_SP_MIN     =  4,
  NSS_SP_MAX     =  8,
  NSS_SP_WARN    = 12,
  NSS_SP_INACT   = 16,
  NSS_SP_EXPIRE  = 20,
  NSS_SP_FLAG    = 24,
  NSS_SP_PWDP    = 28,
  NSS_SP_HDRSZ   = 32   /*(must be multiple of 4; round up)*/
};


#if !defined(_AIX) && !defined(__CYGWIN__) \
 && !(defined(__APPLE__) && defined(__MACH__))

#include <shadow.h>     /* (struct spwd) */

__attribute_nonnull__
__attribute_warn_unused_result__
EXPORT nss_status_t
_nss_mcdb_getspent_r(struct spwd * restrict, char * restrict, size_t,
                     int * restrict);

__attribute_nonnull__
__attribute_warn_unused_result__
EXPORT nss_status_t
_nss_mcdb_getspnam_r(const char * restrict,
                     struct spwd * restrict, char * restrict, size_t,
                     int * restrict);

#endif /* !defined(_AIX) && !defined(__CYGWIN__) */


#endif
