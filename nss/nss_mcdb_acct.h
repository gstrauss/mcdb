/*
 * nss_mcdb_acct - query mcdb of passwd, group nsswitch.conf databases
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

#ifndef INCLUDED_NSS_MCDB_ACCT_H
#define INCLUDED_NSS_MCDB_ACCT_H

#include "../plasma/plasma_feature.h"
#include "../plasma/plasma_attr.h"
#include "../plasma/plasma_stdtypes.h"
#include "nss_mcdb.h"
PLASMA_ATTR_Pragma_once

#include <pwd.h>        /* (struct passwd) */
#include <grp.h>        /* (struct group) */

enum {
  NSS_PW_PASSWD  =  0,
  NSS_PW_GECOS   =  2,
  NSS_PW_DIR     =  4,
  NSS_PW_SHELL   =  6,
  NSS_PW_UID     =  8,
  NSS_PW_GID     = 12,
 #if defined(__sun)
  NSS_PW_AGE     = 16,
  NSS_PW_COMMENT = 18,
  NSS_PW_HDRSZ   = 20   /*(must be multiple of 4)*/
 #elif defined(__FreeBSD__)
  NSS_PW_CHANGE  = 16,
  NSS_PW_EXPIRE  = 24,
  NSS_PW_FIELDS  = 32,
  NSS_PW_CLASS   = 36,
  NSS_PW_HDRSZ   = 40   /*(must be multiple of 4; round up)*/
 #else
  NSS_PW_HDRSZ   = 16   /*(must be multiple of 4)*/
 #endif
};

enum {
  NSS_GR_PASSWD  =  0,
  NSS_GR_MEM_STR =  2,
  NSS_GR_MEM     =  4,
  NSS_GR_MEM_NUM =  6,
  NSS_GR_GID     =  8,
  NSS_GR_HDRSZ   = 12   /*(must be multiple of 4)*/
};

enum {
  NSS_GL_NGROUPS =  0,
  NSS_GL_HDRSZ   =  4   /*(must be multiple of 4)*/
};

/* ngroups_max reasonable value used in sizing some data structures
 * (256) 4-byte or 4-char entries fills 1 KB and must fit in, e.g. struct group
 * On Linux _SC_GETPW_R_SIZE_MAX and _SC_GETGR_R_SIZE_MAX are 1 KB
 */
#define NSS_MCDB_NGROUPS_MAX 256

EXPORT void _nss_mcdb_setpwent(void);
EXPORT void _nss_mcdb_endpwent(void);
EXPORT void _nss_mcdb_setgrent(void);
EXPORT void _nss_mcdb_endgrent(void);
EXPORT void _nss_mcdb_setspent(void);
EXPORT void _nss_mcdb_endspent(void);

__attribute_nonnull__
__attribute_warn_unused_result__
EXPORT nss_status_t
_nss_mcdb_getpwent_r(struct passwd * restrict, char * restrict, size_t,
                     int * restrict);

__attribute_nonnull__
__attribute_warn_unused_result__
EXPORT nss_status_t
_nss_mcdb_getpwnam_r(const char * restrict,
                     struct passwd * restrict, char * restrict, size_t,
                     int * restrict);

__attribute_nonnull__
__attribute_warn_unused_result__
EXPORT nss_status_t
_nss_mcdb_getpwuid_r(uid_t,
                     struct passwd * restrict, char * restrict, size_t,
                     int * restrict);

__attribute_nonnull__
__attribute_warn_unused_result__
EXPORT nss_status_t
_nss_mcdb_getgrent_r(struct group * restrict, char * restrict, size_t,
                     int * restrict);

__attribute_nonnull__
__attribute_warn_unused_result__
EXPORT nss_status_t
_nss_mcdb_getgrnam_r(const char * restrict,
                     struct group * restrict, char * restrict, size_t,
                     int * restrict);

__attribute_nonnull__
__attribute_warn_unused_result__
EXPORT nss_status_t
_nss_mcdb_getgrgid_r(const gid_t gid,
                     struct group * restrict, char * restrict, size_t,
                     int * restrict);

__attribute_nonnull__
__attribute_warn_unused_result__
EXPORT nss_status_t
_nss_mcdb_initgroups_dyn(const char * restrict, gid_t,
                         long int * restrict, long int * restrict,
                         gid_t ** restrict, long int, int * restrict);

__attribute_nonnull_x__((1,4))
__attribute_warn_unused_result__
EXPORT int
nss_mcdb_getgrouplist(const char * restrict, gid_t,
                      gid_t * restrict, int * restrict);
                      /*((gid_t *)groups can be NULL)*/


#endif
