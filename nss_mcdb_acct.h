#ifndef INCLUDED_NSS_MCDB_ACCT_H
#define INCLUDED_NSS_MCDB_ACCT_H

#include "nss_mcdb.h"
#include "code_attributes.h"

#include <sys/types.h>  /* (gid_t) */
#include <pwd.h>        /* (struct passwd) */
#include <grp.h>        /* (struct group) */
#include <shadow.h>     /* (struct spwd) */

enum {
  NSS_PW_PASSWD  =  0,
  NSS_PW_GECOS   =  4,
  NSS_PW_DIR     =  8,
  NSS_PW_SHELL   = 12,
  NSS_PW_UID     = 16,
  NSS_PW_GID     = 24,
 #if defined(__sun)
  NSS_PW_AGE     = 32,
  NSS_PW_COMMENT = 36,
  NSS_PW_HDRSZ   = 40
 #elif defined(__FreeBSD__)
  NSS_PW_CHANGE  = 32,
  NSS_PW_EXPIRE  = 48,
  NSS_PW_FIELDS  = 64,
  NSS_PW_CLASS   = 72,
  NSS_PW_HDRSZ   = 76
 #else
  NSS_PW_HDRSZ   = 32
 #endif
};

enum {
  NSS_GR_PASSWD  =  0,
  NSS_GR_MEM_STR =  4,
  NSS_GR_MEM     =  8,
  NSS_GR_MEM_NUM = 12,
  NSS_GR_GID     = 16,
  NSS_GR_HDRSZ   = 24
};

enum {
  NSS_GL_NGROUPS =  0,
  NSS_GL_HDRSZ   =  8
};

enum {
  NSS_SP_LSTCHG  =  0,
  NSS_SP_MIN     =  8,
  NSS_SP_MAX     = 16,
  NSS_SP_WARN    = 24,
  NSS_SP_INACT   = 32,
  NSS_SP_EXPIRE  = 40,
  NSS_SP_FLAG    = 48,
  NSS_SP_PWDP    = 56,
  NSS_SP_HDRSZ   = 60
};

/* ngroups_max reasonable value used in sizing some data structures
 * (256) 4-byte or 4-char entries fills 1 KB and must fit in, e.g. struct group
 * On Linux _SC_GETPW_R_SIZE_MAX and _SC_GETGR_R_SIZE_MAX are 1 KB
 */
#define NSS_MCDB_NGROUPS_MAX 256

void _nss_mcdb_setpwent(void);
void _nss_mcdb_endpwent(void);
void _nss_mcdb_setgrent(void);
void _nss_mcdb_endgrent(void);
void _nss_mcdb_setspent(void);
void _nss_mcdb_endspent(void);

nss_status_t
_nss_mcdb_getpwent_r(struct passwd * restrict, char * restrict, size_t,
                     int * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;

nss_status_t
_nss_mcdb_getpwnam_r(const char * restrict,
                     struct passwd * restrict, char * restrict, size_t,
                     int * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;

nss_status_t
_nss_mcdb_getpwuid_r(uid_t,
                     struct passwd * restrict, char * restrict, size_t,
                     int * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;

nss_status_t
_nss_mcdb_getgrent_r(struct group * restrict, char * restrict, size_t,
                     int * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;

nss_status_t
_nss_mcdb_getgrnam_r(const char * restrict,
                     struct group * restrict, char * restrict, size_t,
                     int * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;

nss_status_t
_nss_mcdb_getgrgid_r(const gid_t gid,
                     struct group * restrict, char * restrict, size_t,
                     int * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;

nss_status_t
_nss_mcdb_getspent_r(struct spwd * restrict, char * restrict, size_t,
                     int * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;

nss_status_t
_nss_mcdb_getspnam_r(const char * restrict,
                     struct spwd * restrict, char * restrict, size_t,
                     int * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;

nss_status_t
_nss_mcdb_initgroups_dyn(const char * restrict, gid_t,
                         long int * restrict, long int * restrict,
                         gid_t ** restrict, long int, int * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;

int
nss_mcdb_getgrouplist(const char * restrict, gid_t,
                      gid_t * restrict, int * restrict)
  __attribute_nonnull_x__((1,4))  __attribute_warn_unused_result__;
  /*((gid_t *)groups can be NULL)*/


#endif
