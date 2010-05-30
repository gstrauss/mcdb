/* _BSD_SOURCE or _SVID_SOURCE for struct ether_addr on Linux */
#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif

#include "nss_mcdb_misc.h"
#include "nss_mcdb.h"
#include "mcdb.h"
#include "uint32.h"
#include "code_attributes.h"

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
                          const struct _nss_kinfo * restrict
                            __attribute_unused__,
                          const struct _nss_vinfo * restrict)
  __attribute_nonnull_x__((1,3))  __attribute_warn_unused_result__;

static enum nss_status
_nss_mcdb_decode_ether_addr(struct mcdb * restrict,
                            const struct _nss_kinfo * restrict
                              __attribute_unused__,
                            const struct _nss_vinfo * restrict)
  __attribute_nonnull_x__((1,3))  __attribute_warn_unused_result__;


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
_nss_mcdb_decode_aliasent(struct mcdb * const restrict m,
                          const struct _nss_kinfo * const restrict kinfo,
                          const struct _nss_vinfo * const restrict vinfo)
{
    enum {
      IDX_AE_MEM_STR =  0,
      IDX_AE_MEM     =  4, /* align target addr to 8-byte boundary for 64-bit */
      IDX_AE_MEM_NUM =  8,
      IDX_AE_HDRSZ   = 12
    };

    char * const restrict dptr = (char *)mcdb_dataptr(m);
    struct aliasent * const ae = (struct aliasent *)vinfo->vstruct;
    char *buf = vinfo->buf;
    /* parse fixed format record header to get offsets into strings */
    const uintptr_t idx_ae_mem_str=uint16_from_ascii4uphex(dptr+IDX_AE_MEM_STR);
    const uintptr_t idx_ae_mem    =uint16_from_ascii4uphex(dptr+IDX_AE_MEM);
    const size_t ae_mem_num       =uint16_from_ascii4uphex(dptr+IDX_AE_MEM_NUM);
    char ** const restrict ae_mem =
      (char **)(((uintptr_t)(buf+idx_ae_mem+0x7u)) & ~0x7u); /* 8-byte align */
    ae->alias_members_len = ae_mem_num;
    ae->alias_members     = ae_mem;
    ae->alias_local       = 0;          /* ??? */
    /* populate ae string pointers */
    ae->alias_name        = buf;
    /* fill buf, (char **) ae_mem (allow 8-byte ptrs), and terminate strings.
     * scan for ',' instead of precalculating array because names should
     * be short and adding an extra 4 chars per name to store size takes
     * more space and might take just as long to parse as scan for ','
     * (assume data consistent, ae_mem_num correct) */
    if (((char *)ae_mem)-buf+(ae_mem_num<<3) <= vinfo->buflen) {
        *((struct aliasent **)vinfo->vstructp) = ae;
        memcpy(buf, dptr+IDX_AE_HDRSZ, mcdb_datalen(m)-IDX_AE_HDRSZ);
        /* terminate strings; replace ':' separator in data with '\0'. */
        buf[idx_ae_mem_str-1] = '\0';        /* terminate ae_name */
        buf[idx_ae_mem-1]     = '\0';        /* terminate final ae_mem string */
        ae_mem[0] = (buf += idx_ae_mem_str); /* begin of ae_mem strings */
        for (size_t i=1; i<ae_mem_num; ++i) {/*(i=1; assigned first str above)*/
            while (*buf != ',')   /*(*++buf if no 0-len names check in create)*/
                ++buf;
            *buf++ = '\0';
            ae_mem[i] = buf;
        }
        return NSS_STATUS_SUCCESS;
    }
    else {
        *((struct aliasent **)vinfo->vstructp) = NULL;
        errno = ERANGE;
        return NSS_STATUS_TRYAGAIN;
    }
}


static enum nss_status
_nss_mcdb_decode_ether_addr(struct mcdb * const restrict m,
                            const struct _nss_kinfo * const restrict kinfo,
                            const struct _nss_vinfo * const restrict vinfo)
{
    /* (12 hex chars, each encoding (1) 4-bit nibble == 48-bit ether_addr) */
    enum { IDX_ETHER_ADDR_HDRSZ = 12 };

    char * const restrict dptr = (char *)mcdb_dataptr(m);
    const size_t dlen = ((size_t)mcdb_datalen(m)) - IDX_ETHER_ADDR_HDRSZ;
    struct ether_addr * const ether_addr = (struct ether_addr *)vinfo->vstruct;
    char * const buf = vinfo->buf;

    if (ether_addr != NULL) {
        ether_addr->ether_addr_octet[0] = (uxton(dptr[0]) <<4)| uxton(dptr[1]);
        ether_addr->ether_addr_octet[1] = (uxton(dptr[2]) <<4)| uxton(dptr[3]);
        ether_addr->ether_addr_octet[2] = (uxton(dptr[4]) <<4)| uxton(dptr[5]);
        ether_addr->ether_addr_octet[3] = (uxton(dptr[6]) <<4)| uxton(dptr[7]);
        ether_addr->ether_addr_octet[4] = (uxton(dptr[8]) <<4)| uxton(dptr[9]);
        ether_addr->ether_addr_octet[5] = (uxton(dptr[10])<<4)| uxton(dptr[11]);
        if (vinfo->vstructp != NULL)
            *((struct ether_addr **)vinfo->vstructp) = ether_addr;
    }

    if (buf != NULL) {
        if (dlen < vinfo->buflen) {
            memcpy(buf, dptr+IDX_ETHER_ADDR_HDRSZ, dlen);
            buf[dlen] = '\0';              /* terminate hostname */
        }
        else {
            if (vinfo->vstructp != NULL)
                *((struct ether_addr **)vinfo->vstructp) = NULL;
            errno = ERANGE;
            return NSS_STATUS_TRYAGAIN;
        }
    }

    return NSS_STATUS_SUCCESS;
}
