#ifndef INCLUDED_NSS_MCDB_ACCT_H
#define INCLUDED_NSS_MCDB_ACCT_H

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

#endif
