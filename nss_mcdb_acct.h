#ifndef INCLUDED_NSS_MCDB_ACCT_H
#define INCLUDED_NSS_MCDB_ACCT_H

#define NSS_PW_PASSWD   0
#define NSS_PW_GECOS    4
#define NSS_PW_DIR      8
#define NSS_PW_SHELL   12
#define NSS_PW_UID     16  /* prefer 8-byte aligned for cache-line efficiency */
#define NSS_PW_GID     24  /* prefer 8-byte aligned for cache-line efficiency */
#define NSS_PW_HDRSZ   32

#define NSS_GR_PASSWD   0
#define NSS_GR_MEM_STR  4
#define NSS_GR_MEM      8  /* align target addr to 8-byte boundary for 64-bit */
#define NSS_GR_MEM_NUM 12
#define NSS_GR_GID     16  /* prefer 8-byte aligned for cache-line efficiency */
#define NSS_GR_HDRSZ   24

#define NSS_SP_LSTCHG   0  /* prefer 8-byte aligned for cache-line efficiency */
#define NSS_SP_MIN      8  /* prefer 8-byte aligned for cache-line efficiency */
#define NSS_SP_MAX     16  /* prefer 8-byte aligned for cache-line efficiency */
#define NSS_SP_WARN    24  /* prefer 8-byte aligned for cache-line efficiency */
#define NSS_SP_INACT   32  /* prefer 8-byte aligned for cache-line efficiency */
#define NSS_SP_EXPIRE  40  /* prefer 8-byte aligned for cache-line efficiency */
#define NSS_SP_FLAG    48  /* prefer 8-byte aligned for cache-line efficiency */
#define NSS_SP_PWDP    56
#define NSS_SP_HDRSZ   60

#endif
