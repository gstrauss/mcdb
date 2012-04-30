/*
 * nss_mcdb_authn_make - mcdb of shadow nsswitch.conf database
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

#ifndef INCLUDED_NSS_MCDB_AUTHN_MAKE_H
#define INCLUDED_NSS_MCDB_AUTHN_MAKE_H

#include "nss_mcdb_make.h"
#include "code_attributes.h"


#if !defined(_AIX) && !defined(__CYGWIN__)

#include <shadow.h>

/* buf size passed should be _SC_GETPW_R_SIZE_MAX + NSS_SP_HDRSZ (not checked)*/
size_t
nss_mcdb_authn_make_spwd_datastr(char * restrict buf, const size_t bufsz,
				 const struct spwd * const restrict sp)
  __attribute_nonnull__;

#endif /* !defined(_AIX) && !defined(__CYGWIN__) */


bool
nss_mcdb_authn_make_spwd_encode(
  struct nss_mcdb_make_winfo * const restrict w,
  const void * const restrict entp)
  __attribute_nonnull__;


bool
nss_mcdb_authn_make_shadow_parse(
  struct nss_mcdb_make_winfo * const restrict w, char * restrict p)
  __attribute_nonnull__;


#endif
