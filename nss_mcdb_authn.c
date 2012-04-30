/*
 * nss_mcdb_authn - query mcdb of shadow nsswitch.conf database
 *
 * Copyright (c) 2011, Glue Logic LLC. All rights reserved. code()gluelogic.com
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

#include "nss_mcdb_authn.h"
#include "nss_mcdb.h"


void _nss_mcdb_setspent(void) { nss_mcdb_setent(NSS_DBTYPE_SHADOW,0); }
void _nss_mcdb_endspent(void) { nss_mcdb_endent(NSS_DBTYPE_SHADOW);   }


#if !defined(_AIX) && !defined(__CYGWIN__)


#include <errno.h>
#include <string.h>

#include <shadow.h>
#include <arpa/inet.h>  /* ntohl(), ntohs() */

/*
 * man shadow(5) getspnam
 *     /etc/shadow
 */

static nss_status_t
nss_mcdb_authn_spwd_decode(struct mcdb * restrict,
                           const struct nss_mcdb_vinfo * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;


nss_status_t
_nss_mcdb_getspent_r(struct spwd * const restrict spbuf,
                     char * const restrict buf, const size_t bufsz,
                     int * const restrict errnop)
{
    const struct nss_mcdb_vinfo v = { .decode  = nss_mcdb_authn_spwd_decode,
                                      .vstruct = spbuf,
                                      .buf     = buf,
                                      .bufsz   = bufsz,
                                      .errnop  = errnop };
    return nss_mcdb_getent(NSS_DBTYPE_SHADOW, &v);
}

nss_status_t
_nss_mcdb_getspnam_r(const char * const restrict name,
                     struct spwd * const restrict spbuf,
                     char * const restrict buf, const size_t bufsz,
                     int * const restrict errnop)
{
    const struct nss_mcdb_vinfo v = { .decode  = nss_mcdb_authn_spwd_decode,
                                      .vstruct = spbuf,
                                      .buf     = buf,
                                      .bufsz   = bufsz,
                                      .errnop  = errnop,
                                      .key     = name,
                                      .klen    = strlen(name),
                                      .tagc    = (unsigned char)'=' };
    return nss_mcdb_get_generic(NSS_DBTYPE_SHADOW, &v);
}


static nss_status_t
nss_mcdb_authn_spwd_decode(struct mcdb * const restrict m,
                           const struct nss_mcdb_vinfo * const restrict v)
{
    const char * const restrict dptr = (char *)mcdb_dataptr(m);
    const size_t dlen = ((size_t)mcdb_datalen(m)) - NSS_SP_HDRSZ;
    struct spwd * const sp = (struct spwd *)v->vstruct;
    char * const buf = v->buf;
    union { uint32_t u[NSS_SP_HDRSZ>>2]; uint16_t h[NSS_SP_HDRSZ>>1]; } hdr;
    if (dlen < v->bufsz) {
        memcpy(buf, dptr+NSS_SP_HDRSZ, dlen);
        buf[dlen] = '\0';
        memcpy(hdr.u, dptr, NSS_SP_HDRSZ);
    }
    else {
        *v->errnop = errno = ERANGE;
        return NSS_STATUS_TRYAGAIN;
    }
    /* (cast to (int32_t) before casting to (long) to preserve -1) */
    sp->sp_lstchg = (long)(int32_t)ntohl( hdr.u[NSS_SP_LSTCHG>>2] );
    sp->sp_min    = (long)(int32_t)ntohl( hdr.u[NSS_SP_MIN>>2] );
    sp->sp_max    = (long)(int32_t)ntohl( hdr.u[NSS_SP_MAX>>2] );
    sp->sp_warn   = (long)(int32_t)ntohl( hdr.u[NSS_SP_WARN>>2] );
    sp->sp_inact  = (long)(int32_t)ntohl( hdr.u[NSS_SP_INACT>>2] );
    sp->sp_expire = (long)(int32_t)ntohl( hdr.u[NSS_SP_EXPIRE>>2] );
    sp->sp_flag   =                ntohl( hdr.u[NSS_SP_FLAG>>2] );
    sp->sp_namp   = buf;
    sp->sp_pwdp   = buf + ntohs( hdr.h[NSS_SP_PWDP>>1] );
    return NSS_STATUS_SUCCESS;
}


#endif /* !defined(_AIX) && !defined(__CYGWIN__) */
