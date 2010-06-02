#ifndef INCLUDED_NSS_MCDB_NETDB_H
#define INCLUDED_NSS_MCDB_NETDB_H

/* GPS: move that alignment comment into the code */
/* GPS: change nss_mcdb_acct.h to enum's */

enum {
  IDX_H_ADDRTYPE =  0,
  IDX_H_LENGTH   =  8,
  IDX_HE_MEM_STR = 16,
  IDX_HE_LST_STR = 20,
  IDX_HE_MEM     = 24, /* align target addr to 8-byte boundary for 64-bit */
  IDX_HE_MEM_NUM = 28,
  IDX_HE_LST_NUM = 32,
  IDX_HE_HDRSZ   = 36
};

enum {
  IDX_N_NET      =  0,
  IDX_N_ADDRTYPE =  8,
  IDX_NE_MEM_STR = 16,
  IDX_NE_MEM     = 20, /* align target addr to 8-byte boundary for 64-bit */
  IDX_NE_MEM_NUM = 24,
  IDX_NE_HDRSZ   = 28
};

enum {
  IDX_P_PROTO    =  0,
  IDX_PE_MEM_STR =  8,
  IDX_PE_MEM     = 12, /* align target addr to 8-byte boundary for 64-bit */
  IDX_PE_MEM_NUM = 16,
  IDX_PE_HDRSZ   = 20
};

enum {
  IDX_R_NUMBER   =  0,
  IDX_RE_MEM_STR =  8,
  IDX_RE_MEM     = 12, /* align target addr to 8-byte boundary for 64-bit */
  IDX_RE_MEM_NUM = 16,
  IDX_RE_HDRSZ   = 20
};

enum {
  IDX_S_PORT     =  0,
  IDX_S_NAME     =  8,
  IDX_SE_MEM_STR = 12,
  IDX_SE_MEM     = 16, /* align target addr to 8-byte boundary for 64-bit */
  IDX_SE_MEM_NUM = 20,
  IDX_SE_HDRSZ   = 24
};

#endif
