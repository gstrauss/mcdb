#ifndef INCLUDED_NSS_MCDB_ACCT_H
#define INCLUDED_NSS_MCDB_ACCT_H

#define IDX_PW_PASSWD   0
#define IDX_PW_GECOS    4
#define IDX_PW_DIR      8
#define IDX_PW_SHELL   12
#define IDX_PW_UID     16  /* prefer 8-byte aligned for cache-line efficiency */
#define IDX_PW_GID     24  /* prefer 8-byte aligned for cache-line efficiency */
#define IDX_PW_HDRSZ   32

#define IDX_GR_PASSWD   0
#define IDX_GR_MEM_STR  4
#define IDX_GR_MEM      8  /* align target addr to 8-byte boundary for 64-bit */
#define IDX_GR_MEM_NUM 12
#define IDX_GR_GID     16  /* prefer 8-byte aligned for cache-line efficiency */
#define IDX_GR_HDRSZ   24

#define IDX_SP_NAMP     0
#define IDX_SP_PWDP     4
#define IDX_SP_LSTCHG   8  /* prefer 8-byte aligned for cache-line efficiency */
#define IDX_SP_MIN     16  /* prefer 8-byte aligned for cache-line efficiency */
#define IDX_SP_MAX     24  /* prefer 8-byte aligned for cache-line efficiency */
#define IDX_SP_WARN    32  /* prefer 8-byte aligned for cache-line efficiency */
#define IDX_SP_INACT   40  /* prefer 8-byte aligned for cache-line efficiency */
#define IDX_SP_EXPIRE  48  /* prefer 8-byte aligned for cache-line efficiency */
#define IDX_SP_FLAG    56  /* prefer 8-byte aligned for cache-line efficiency */
#define IDX_SP_HDRSZ   64

#endif
