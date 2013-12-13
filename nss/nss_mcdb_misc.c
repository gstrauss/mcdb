/*
 * nss_mcdb_misc - query mcdb of aliases, ethers, publickey, secretkey
 *
 * Copyright (c) 2010, Glue Logic LLC. All rights reserved. code()gluelogic.com
 *
 *  This file is part of mcdb.
 *
 *  mcdb is free software: you can redistribute it and/or modify it under
 *  the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 2.1 of the License, or
 *  (at your option) any later version.
 *
 *  mcdb is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with mcdb.  If not, see <http://www.gnu.org/licenses/>.
 */

/* _BSD_SOURCE or _SVID_SOURCE for struct ether_addr on Linux */
#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif

#include "nss_mcdb_misc.h"
#include "nss_mcdb.h"

#include <errno.h>
#include <string.h>

/*#include <aliases.h>*/       /* not portable; see nss_mcdb_misc.h */
/*#include <netinet/ether.h>*/ /* not portable; see nss_mcdb_misc.h */

#include <netinet/in.h>    /* htonl() htons() ntohl() ntohs() */
#include <arpa/inet.h>     /* htonl() htons() ntohl() ntohs() */

#ifdef __sun
#include <rpc/des_crypt.h>
int xdecrypt(char *secret, char *passwd);
#endif

PLASMA_ATTR_Pragma_no_side_effect(strlen)

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

__attribute_nonnull__
__attribute_warn_unused_result__
static nss_status_t
nss_mcdb_misc_aliasent_decode(struct mcdb * restrict,
                              const struct nss_mcdb_vinfo * restrict);

__attribute_nonnull__
__attribute_warn_unused_result__
static nss_status_t
nss_mcdb_misc_ether_addr_decode(struct mcdb * restrict,
                                const struct nss_mcdb_vinfo * restrict);


void _nss_mcdb_setaliasent(void) { nss_mcdb_setent(NSS_DBTYPE_ALIASES,0); }
void _nss_mcdb_endaliasent(void) { nss_mcdb_endent(NSS_DBTYPE_ALIASES);   }
void _nss_mcdb_setetherent(void) { nss_mcdb_setent(NSS_DBTYPE_ETHERS,0);  }
void _nss_mcdb_endetherent(void) { nss_mcdb_endent(NSS_DBTYPE_ETHERS);    }


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
    size_t ae_mem_num;
    union { uint32_t u[NSS_AE_HDRSZ>>2]; uint16_t h[NSS_AE_HDRSZ>>1]; } hdr;
    memcpy(hdr.u, dptr, NSS_AE_HDRSZ);
    ae->alias_name        = buf;
    ae->alias_local       = (int)ntohl( hdr.u[NSS_AE_LOCAL>>2] );
    ae->alias_members_len = ae_mem_num =(size_t)ntohs(hdr.h[NSS_AE_MEM_NUM>>1]);
    ae->alias_members     = /* align to 8-byte boundary for 64-bit */
      (char **)(((uintptr_t)(buf+ntohs(hdr.h[NSS_AE_MEM>>1])+0x7u)) & ~0x7u);
    /* fill buf, (char **) ae_mem (allow 8-byte ptrs), and terminate strings.
     * scan for '\0' instead of precalculating array because names should
     * be short and adding an extra 4 chars per name to store size takes
     * more space and might take just as long to parse as scan for '\0'
     * (assume data consistent, ae_mem_num correct) */
    if (((char *)ae->alias_members)-buf+(ae_mem_num<<3) <= v->bufsz) {
        char ** const restrict ae_mem = ae->alias_members;
        memcpy(buf, dptr+NSS_AE_HDRSZ, mcdb_datalen(m)-NSS_AE_HDRSZ);
        ae_mem[0] = (buf += ntohs(hdr.h[NSS_AE_MEM_STR>>1])); /*ae_mem strings*/
        for (size_t i=1; i<ae_mem_num; ++i) {/*(i=1; assigned first str above)*/
            while (*++buf != '\0')
                ;
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

    if (ether_addr != NULL) /* (48-bit ether_addr == 6 bytes == NSS_EA_HDRSZ) */
	memcpy(&ether_addr->ether_addr_octet[0], dptr, 6);

    if (buf != NULL) {
        if (dlen < v->bufsz) {
            memcpy(buf, dptr+NSS_EA_HDRSZ, dlen);
            buf[dlen] = '\0';
        }
        else {
            *v->errnop = errno = ERANGE;
            return NSS_STATUS_TRYAGAIN;
        }
    }

    return NSS_STATUS_SUCCESS;
}
