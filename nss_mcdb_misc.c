/* _BSD_SOURCE or _SVID_SOURCE for struct ether_addr on Linux */
#define _BSD_SOURCE

#include "nss_mcdb_misc.h"
#include "nss_mcdb.h"
#include "mcdb.h"
#include "mcdb_uint32.h"
#include "mcdb_attribute.h"

#include <errno.h>
#include <string.h>

#include <aliases.h>
#include <netinet/ether.h>
#include <netinet/in.h>    /* htonl() htons() ntohl() ntohs() */
#include <arpa/inet.h>     /* htonl() htons() ntohl() ntohs() */

/*
 * man aliases(5) getaliasbyname setaliasent getaliasent endaliasent
 *     /etc/aliases
 * man ethers(5)  ether_line ether_hostton ether_ntohost
 *     /etc/ethers
 * man getpublickey (Solaris)
 * man getsecretkey (Solaris)
 */

static enum nss_status
_nss_mcdb_decode_aliasent(struct mcdb * restrict,
                          const struct _nss_kinfo * restrict,
                          const struct _nss_vinfo * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;

static enum nss_status
_nss_mcdb_decode_ether_addr(struct mcdb * restrict,
                            const struct _nss_kinfo * restrict,
                            const struct _nss_vinfo * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;


void _nss_files_setaliasent(void) { _nss_mcdb_setent(NSS_DBTYPE_ALIASES); }
void _nss_files_endaliasent(void) { _nss_mcdb_endent(NSS_DBTYPE_ALIASES); }
void _nss_files_setetherent(void) { _nss_mcdb_setent(NSS_DBTYPE_ETHERS);  }
void _nss_files_endetherent(void) { _nss_mcdb_endent(NSS_DBTYPE_ETHERS);  }


enum nss_status
_nss_files_getaliasent_r(struct aliasent * const restrict aliasbuf,
                         char * const restrict buf, const size_t buflen,
                         struct aliasent ** const restrict aliasbufp)
{
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_aliasent,
                                      .vstruct = aliasbuf,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .vstructp= aliasbufp };
    *aliasbufp = NULL;
    return _nss_mcdb_getent(NSS_DBTYPE_ALIASES, &vinfo);
}

enum nss_status
_nss_files_getaliasbyname_r(const char * const restrict name,
                            struct aliasent * const restrict aliasbuf,
                            char * const restrict buf, const size_t buflen,
                            struct aliasent ** const restrict aliasbufp)
{
    const struct _nss_kinfo kinfo = { .key     = name,
                                      .klen    = strlen(name),
                                      .tagc    = (unsigned char)'=' };
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_aliasent,
                                      .vstruct = aliasbuf,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .vstructp= aliasbufp };
    *aliasbufp = NULL;
    return _nss_mcdb_get_generic(NSS_DBTYPE_ALIASES, &kinfo, &vinfo);
}


enum nss_status
_nss_files_getpublickey(const char * const restrict name,
                        char * const restrict buf, const size_t buflen)
{   /*  man getpublickey() *//* Solaris */
    const struct _nss_kinfo kinfo = { .key     = name,
                                      .klen    = strlen(name),
                                      .tagc    = (unsigned char)'=' };
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_buf,
                                      .vstruct = NULL,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .vstructp= NULL };
    return _nss_mcdb_get_generic(NSS_DBTYPE_PUBLICKEY, &kinfo, &vinfo);
}


enum nss_status
_nss_files_getsecretkey(const char * const restrict name,
                        const char * const restrict decryptkey,
                        char * const restrict buf, const size_t buflen)
{   /*  man getsecretkey() *//* Solaris */
    const struct _nss_kinfo kinfo = { .key     = name,
                                      .klen    = strlen(name),
                                      .tagc    = (unsigned char)'*' };
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_buf,
                                      .vstruct = NULL,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .vstructp= NULL };
    const enum nss_status status =
      _nss_mcdb_get_generic(NSS_DBTYPE_PUBLICKEY, &kinfo, &vinfo);
    /* TODO: decrypt buf using decryptkey */
    return status;
}


enum nss_status
_nss_files_getetherent_r(struct ether_addr * const restrict etherbuf,
                         char * const restrict buf, const size_t buflen,
                         struct ether_addr ** const restrict etherbufp)
{   /*  man ether_line()  */
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_ether_addr,
                                      .vstruct = etherbuf,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .vstructp= etherbufp };
    *etherbufp = NULL;
    return _nss_mcdb_getent(NSS_DBTYPE_ETHERS, &vinfo);
}

enum nss_status
_nss_files_gethostton_r(const char * const restrict name,
                        struct ether_addr * const restrict etherbuf)
{   /*  man ether_hostton()  */
    const struct _nss_kinfo kinfo = { .key     = name,
                                      .klen    = strlen(name),
                                      .tagc    = (unsigned char)'=' };
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_ether_addr,
                                      .vstruct = etherbuf,
                                      .buf     = NULL,
                                      .buflen  = 0,
                                      .vstructp= NULL };
    return _nss_mcdb_get_generic(NSS_DBTYPE_ETHERS, &kinfo, &vinfo);
}

enum nss_status
_nss_files_getntohost_r(struct ether_addr * const restrict ether,
                        char * const restrict buf, const size_t buflen)
{   /*  man ether_ntohost()  */
    char hexstr[12]; /* (6) 8-bit octets = 48-bits */
    const struct _nss_kinfo kinfo = { .key     = hexstr,
                                      .klen     = sizeof(hexstr),
                                      .tagc    = (unsigned char)'x' };
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_buf,
                                      .vstruct = NULL,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .vstructp= NULL };
    /* (assumes hexstr[] on stack is aligned to at least 4-byte boundary) */
    memcpy(hexstr, &(ether->ether_addr_octet[0]), sizeof(hexstr));
    uint32_to_ascii8uphex(ntohl(*((uint32_t *)&hexstr[0])), hexstr);
    uint16_to_ascii4uphex((uint32_t)ntohs(*((uint16_t *)&hexstr[8])), hexstr+8);
    return _nss_mcdb_get_generic(NSS_DBTYPE_ETHERS, &kinfo, &vinfo);
}


static enum nss_status
_nss_mcdb_decode_aliasent(struct mcdb * restrict m,
                          const struct _nss_kinfo * restrict kinfo,
                          const struct _nss_vinfo * restrict vinfo)
{
    /* TODO */
    return NSS_STATUS_SUCCESS;
}


static enum nss_status
_nss_mcdb_decode_ether_addr(struct mcdb * restrict m,
                            const struct _nss_kinfo * restrict kinfo,
                            const struct _nss_vinfo * restrict vinfo)
{
    /* TODO: decode into struct ether_addr and hostname in buf, if not NULL */
    return NSS_STATUS_SUCCESS;
}
