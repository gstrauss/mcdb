#ifndef INCLUDED_NSS_MCDB_NETDB_H
#define INCLUDED_NSS_MCDB_NETDB_H

enum {
  NSS_H_ADDRTYPE =  0,
  NSS_H_LENGTH   =  8,
  NSS_HE_MEM_STR = 16,
  NSS_HE_LST_STR = 20,
  NSS_HE_MEM     = 24,
  NSS_HE_MEM_NUM = 28,
  NSS_HE_LST_NUM = 32,
  NSS_HE_HDRSZ   = 36
};

enum {
  NSS_N_NET      =  0,
  NSS_N_ADDRTYPE =  8,
  NSS_NE_MEM_STR = 16,
  NSS_NE_MEM     = 20,
  NSS_NE_MEM_NUM = 24,
  NSS_NE_HDRSZ   = 28
};

enum {
  NSS_P_PROTO    =  0,
  NSS_PE_MEM_STR =  8,
  NSS_PE_MEM     = 12,
  NSS_PE_MEM_NUM = 16,
  NSS_PE_HDRSZ   = 20
};

enum {
  NSS_R_NUMBER   =  0,
  NSS_RE_MEM_STR =  8,
  NSS_RE_MEM     = 12,
  NSS_RE_MEM_NUM = 16,
  NSS_RE_HDRSZ   = 20
};

enum {
  NSS_S_PORT     =  0,
  NSS_S_NAME     =  8,
  NSS_SE_MEM_STR = 12,
  NSS_SE_MEM     = 16,
  NSS_SE_MEM_NUM = 20,
  NSS_SE_HDRSZ   = 24
};

#endif
