#ifndef INCLUDED_NSS_MCDB_ACCT_MAKE_H
#define INCLUDED_NSS_MCDB_ACCT_MAKE_H

#include "code_attributes.h"

#include <pwd.h>
#include <grp.h>
#include <shadow.h>

/* buf size passed should be _SC_GETPW_R_SIZE_MAX + NSS_PW_HDRSZ (not checked)*/
size_t
cdb_pw2str(char * restrict buf, const size_t bufsz,
           const struct passwd * const restrict pw)
  __attribute_nonnull__;

/* buf size passed should be _SC_GETGR_R_SIZE_MAX + NSS_GR_HDRSZ (not checked)*/
size_t
cdb_gr2str(char * restrict buf, const size_t bufsz,
           const struct group * const restrict gr)
  __attribute_nonnull__;

/* buf size passed should be _SC_GETPW_R_SIZE_MAX + NSS_SP_HDRSZ (not checked)*/
size_t
cdb_spwd2str(char * restrict buf, const size_t bufsz,
             const struct spwd * const restrict sp)
  __attribute_nonnull__;

#endif
