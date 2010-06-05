#ifndef INCLUDED_NSS_MCDB_MISC_H
#define INCLUDED_NSS_MCDB_MISC_H

enum {
  IDX_AE_LOCAL   =  0,
  IDX_AE_MEM_STR =  4,
  IDX_AE_MEM     =  8, /* align target addr to 8-byte boundary for 64-bit */
  IDX_AE_MEM_NUM = 12,
  IDX_AE_HDRSZ   = 20
};

enum {
  IDX_EA_HDRSZ   = 12
};

#endif
