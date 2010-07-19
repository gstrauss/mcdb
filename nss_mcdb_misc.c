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

#ifdef __sun
#include <rpc/des_crypt.h>
int xdecrypt(char *secret, char *passwd);
#endif

/*
 * man aliases(5) getaliasbyname setaliasent getaliasent endaliasent
 *     /etc/aliases
 * man ethers(5)  ether_line ether_hostton ether_ntohost
 *     /etc/ethers
 * man getpublickey (Solaris)
 * man getsecretkey (Solaris)
 * man shells(5)  setusershell getusershell endusershell
 *     /etc/shells  (not implemented here; /etc/shells usually very small file)
 *     (_BSD_SOURCE || ( _XOPEN_SOURCE && _XOPEN_SOURCE < 500)  (note: < 500))
 */

static nss_status_t
nss_mcdb_misc_aliasent_decode(struct mcdb * restrict,
                              const struct nss_mcdb_vinfo * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;

static nss_status_t
nss_mcdb_misc_ether_addr_decode(struct mcdb * restrict,
                                const struct nss_mcdb_vinfo * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;


void _nss_mcdb_setaliasent(void) { nss_mcdb_setent(NSS_DBTYPE_ALIASES); }
void _nss_mcdb_endaliasent(void) { nss_mcdb_endent(NSS_DBTYPE_ALIASES); }
void _nss_mcdb_setetherent(void) { nss_mcdb_setent(NSS_DBTYPE_ETHERS);  }
void _nss_mcdb_endetherent(void) { nss_mcdb_endent(NSS_DBTYPE_ETHERS);  }


nss_status_t
_nss_mcdb_getaliasent_r(struct aliasent * const restrict aliasbuf,
                        char * const restrict buf, const size_t bufsz,
                        int * const restrict errnop)
{
    const struct nss_mcdb_vinfo v = { .decode  = nss_mcdb_misc_aliasent_decode,
                                      .vstruct = aliasbuf,
                                      .buf     = buf,
                                      .bufsz   = bufsz,
                                      .errnop  = errnop };
    return nss_mcdb_getent(NSS_DBTYPE_ALIASES, &v);
}

nss_status_t
_nss_mcdb_getaliasbyname_r(const char * const restrict name,
                           struct aliasent * const restrict aliasbuf,
                           char * const restrict buf, const size_t bufsz,
                           int * const restrict errnop)
{
    const struct nss_mcdb_vinfo v = { .decode  = nss_mcdb_misc_aliasent_decode,
                                      .vstruct = aliasbuf,
                                      .buf     = buf,
                                      .bufsz   = bufsz,
                                      .errnop  = errnop,
                                      .key     = name,
                                      .klen    = strlen(name),
                                      .tagc    = (unsigned char)'=' };
    return nss_mcdb_get_generic(NSS_DBTYPE_ALIASES, &v);
}


nss_status_t
_nss_mcdb_getpublickey(const char * const restrict name,
                       char * const restrict buf, const size_t bufsz,
                       int * const restrict errnop)
{   /*  man getpublickey() *//* Solaris */
    const struct nss_mcdb_vinfo v = { .decode  = nss_mcdb_buf_decode,
                                      .vstruct = NULL,
                                      .buf     = buf,
                                      .bufsz   = bufsz,
                                      .errnop  = errnop,
                                      .key     = name,
                                      .klen    = strlen(name),
                                      .tagc    = (unsigned char)'=' };
    return nss_mcdb_get_generic(NSS_DBTYPE_PUBLICKEY, &v);
}


nss_status_t
_nss_mcdb_getsecretkey(const char * const restrict name,
                       const char * const restrict decryptkey,
                       char * const restrict buf, const size_t bufsz,
                       int * const restrict errnop)
{   /*  man getsecretkey() *//* Solaris */
    const struct nss_mcdb_vinfo v = { .decode  = nss_mcdb_buf_decode,
                                      .vstruct = NULL,
                                      .buf     = buf,
                                      .bufsz   = bufsz,
                                      .errnop  = errnop,
                                      .key     = name,
                                      .klen    = strlen(name),
                                      .tagc    = (unsigned char)'*' };
    const nss_status_t status = nss_mcdb_get_generic(NSS_DBTYPE_PUBLICKEY, &v);
    if (status == NSS_STATUS_SUCCESS) {
      /* todo: decrypt buf using decryptkey */
      #ifdef __sun
        if (!xdecrypt(buf, decryptkey))
            return NSS_STATUS_RETURN;
      #endif
    }
    return status;
}


nss_status_t
_nss_mcdb_getetherent_r(struct ether_addr * const restrict etherbuf,
                        char * const restrict buf, const size_t bufsz,
                        int * const restrict errnop)
{   /*  man ether_line()  */
    const struct nss_mcdb_vinfo v = { .decode  =nss_mcdb_misc_ether_addr_decode,
                                      .vstruct = etherbuf,
                                      .buf     = buf,
                                      .bufsz   = bufsz,
                                      .errnop  = errnop };
    return nss_mcdb_getent(NSS_DBTYPE_ETHERS, &v);
}

nss_status_t
_nss_mcdb_gethostton_r(const char * const restrict name,
                       struct ether_addr * const restrict etherbuf,
                       int * const restrict errnop)
{   /*  man ether_hostton()  */
    const struct nss_mcdb_vinfo v = { .decode  =nss_mcdb_misc_ether_addr_decode,
                                      .vstruct = etherbuf,
                                      .buf     = NULL,
                                      .bufsz   = 0,
                                      .errnop  = errnop,
                                      .key     = name,
                                      .klen    = strlen(name),
                                      .tagc    = (unsigned char)'=' };
    return nss_mcdb_get_generic(NSS_DBTYPE_ETHERS, &v);
}

nss_status_t
_nss_mcdb_getntohost_r(struct ether_addr * const restrict ether,
                       char * const restrict buf, const size_t bufsz,
                       int * const restrict errnop)
{   /*  man ether_ntohost()  */
    const struct nss_mcdb_vinfo v = { .decode  = nss_mcdb_buf_decode,
                                      .vstruct = NULL,
                                      .buf     = buf,
                                      .bufsz   = bufsz,
                                      .errnop  = errnop,
                                      .key     = (char *)ether,
                                      .klen    = sizeof(struct ether_addr),
                                      .tagc    = (unsigned char)'b' };/*binary*/
    return nss_mcdb_get_generic(NSS_DBTYPE_ETHERS, &v);
}


static nss_status_t
nss_mcdb_misc_aliasent_decode(struct mcdb * const restrict m,
                              const struct nss_mcdb_vinfo * const restrict v)
{
    const char * const restrict dptr = (char *)mcdb_dataptr(m);
    struct aliasent * const ae = (struct aliasent *)v->vstruct;
    char *buf = v->buf;
    /* parse fixed format record header to get offsets into strings */
    const uintptr_t idx_ae_mem_str=uint16_from_ascii4uphex(dptr+NSS_AE_MEM_STR);
    const uintptr_t idx_ae_mem    =uint16_from_ascii4uphex(dptr+NSS_AE_MEM);
    const size_t ae_mem_num       =uint16_from_ascii4uphex(dptr+NSS_AE_MEM_NUM);
    char ** const restrict ae_mem =   /* align to 8-byte boundary for 64-bit */
      (char **)(((uintptr_t)(buf+idx_ae_mem+0x7u)) & ~0x7u); /* 8-byte align */
    ae->alias_members_len = ae_mem_num;
    ae->alias_members     = ae_mem;
    ae->alias_local               =uint32_from_ascii8uphex(dptr+NSS_AE_LOCAL);
    /* populate ae string pointers */
    ae->alias_name        = buf;
    /* fill buf, (char **) ae_mem (allow 8-byte ptrs), and terminate strings.
     * scan for ',' instead of precalculating array because names should
     * be short and adding an extra 4 chars per name to store size takes
     * more space and might take just as long to parse as scan for ','
     * (assume data consistent, ae_mem_num correct) */
    if (((char *)ae_mem)-buf+(ae_mem_num<<3) <= v->bufsz) {
        memcpy(buf, dptr+NSS_AE_HDRSZ, mcdb_datalen(m)-NSS_AE_HDRSZ);
        /* terminate strings; replace ':' separator in data with '\0'. */
        buf[idx_ae_mem_str-1] = '\0';        /* terminate ae_name */
        buf[idx_ae_mem-1]     = '\0';        /* terminate final ae_mem string */
        ae_mem[0] = (buf += idx_ae_mem_str); /* begin of ae_mem strings */
        for (size_t i=1; i<ae_mem_num; ++i) {/*(i=1; assigned first str above)*/
            while (*++buf != ',')
                ;
            *buf = '\0';
            ae_mem[i] = ++buf;
        }
        return NSS_STATUS_SUCCESS;
    }
    else {
        *v->errnop = errno = ERANGE;
        return NSS_STATUS_TRYAGAIN;
    }
}


static nss_status_t
nss_mcdb_misc_ether_addr_decode(struct mcdb * const restrict m,
                                const struct nss_mcdb_vinfo * const restrict v)
{
    const char * const restrict dptr = (char *)mcdb_dataptr(m);
    const size_t dlen = ((size_t)mcdb_datalen(m)) - NSS_EA_HDRSZ;
    struct ether_addr * const ether_addr = (struct ether_addr *)v->vstruct;
    char * const buf = v->buf;

    if (ether_addr != NULL) {
        /* (12 hex chars, each encoding (1) 4-bit nibble == 48-bit ether_addr)*/
        ether_addr->ether_addr_octet[0] = (uxton(dptr[0]) <<4)| uxton(dptr[1]);
        ether_addr->ether_addr_octet[1] = (uxton(dptr[2]) <<4)| uxton(dptr[3]);
        ether_addr->ether_addr_octet[2] = (uxton(dptr[4]) <<4)| uxton(dptr[5]);
        ether_addr->ether_addr_octet[3] = (uxton(dptr[6]) <<4)| uxton(dptr[7]);
        ether_addr->ether_addr_octet[4] = (uxton(dptr[8]) <<4)| uxton(dptr[9]);
        ether_addr->ether_addr_octet[5] = (uxton(dptr[10])<<4)| uxton(dptr[11]);
    }

    if (buf != NULL) {
        if (dlen < v->bufsz) {
            memcpy(buf, dptr+NSS_EA_HDRSZ, dlen);
            buf[dlen] = '\0';              /* terminate hostname */
        }
        else {
            *v->errnop = errno = ERANGE;
            return NSS_STATUS_TRYAGAIN;
        }
    }

    return NSS_STATUS_SUCCESS;
}
