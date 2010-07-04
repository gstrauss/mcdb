#ifndef INCLUDED_NSS_MCDB_ACCT_H
#define INCLUDED_NSS_MCDB_ACCT_H

#include "nss_mcdb.h"
#include "code_attributes.h"

#include <sys/types.h>  /* (gid_t) */

enum {
  NSS_PW_PASSWD  =  0,
  NSS_PW_GECOS   =  4,
  NSS_PW_DIR     =  8,
  NSS_PW_SHELL   = 12,
  NSS_PW_UID     = 16,
  NSS_PW_GID     = 24,
  NSS_PW_HDRSZ   = 32
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
  NSS_GL_HDRSZ   =  4
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


int
nss_mcdb_getgrouplist(const char * restrict, gid_t,
                      gid_t * restrict, int * restrict)
  __attribute_nonnull_x__((1,4));  /*((gid_t *)groups can be NULL)*/

nss_status_t
_nss_mcdb_initgroups_dyn(const char * restrict, gid_t,
                         long int * restrict, long int * restrict,
                         gid_t ** restrict, long int, int * restrict)
  __attribute_nonnull__;


#endif
