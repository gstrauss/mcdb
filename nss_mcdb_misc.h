#ifndef INCLUDED_NSS_MCDB_MISC_H
#define INCLUDED_NSS_MCDB_MISC_H

enum {
  NSS_AE_LOCAL   =  0,
  NSS_AE_MEM_STR =  4,
  NSS_AE_MEM     =  8, /* align target addr to 8-byte boundary for 64-bit */
  NSS_AE_MEM_NUM = 12,
  NSS_AE_HDRSZ   = 20
};

enum {
  NSS_EA_HDRSZ   = 12
};

#endif
