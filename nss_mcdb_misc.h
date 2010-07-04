#ifndef INCLUDED_NSS_MCDB_MISC_H
#define INCLUDED_NSS_MCDB_MISC_H

enum {
  NSS_AE_LOCAL   =  0,
  NSS_AE_MEM_STR =  4,
  NSS_AE_MEM     =  8,
  NSS_AE_MEM_NUM = 12,
  NSS_AE_HDRSZ   = 20
};

enum {
  NSS_EA_HDRSZ   = 12
};

/* _BSD_SOURCE or _SVID_SOURCE for struct ether_addr on Linux */
#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif

#include "code_attributes.h"

#include <aliases.h>        /* (struct aliasent) */
#include <netinet/ether.h>  /* (struct ether_addr) */

void _nss_mcdb_setaliasent(void);
void _nss_mcdb_endaliasent(void);
void _nss_mcdb_setetherent(void);
void _nss_mcdb_endetherent(void);

nss_status_t
_nss_mcdb_getaliasent_r(struct aliasent * restrict,
                        char * restrict, size_t, int * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;

nss_status_t
_nss_mcdb_getaliasbyname_r(const char * restrict, struct aliasent * restrict,
                           char * restrict, size_t, int * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;

nss_status_t
_nss_mcdb_getpublickey(const char * restrict,
                       char * restrict, size_t, int * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;

nss_status_t
_nss_mcdb_getsecretkey(const char * restrict, const char * restrict,
                       char * restrict, size_t, int * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;

nss_status_t
_nss_mcdb_getetherent_r(struct ether_addr * restrict, char * restrict, size_t,
                        int * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;

nss_status_t
_nss_mcdb_gethostton_r(const char * restrict, struct ether_addr * restrict,
                       int * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;

nss_status_t
_nss_mcdb_getntohost_r(struct ether_addr * restrict, char * restrict, size_t,
                       int * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;


#endif
