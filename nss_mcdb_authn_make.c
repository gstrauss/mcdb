/*
 * nss_mcdb_authn_make - mcdb of shadow nsswitch.conf database
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

#include "nss_mcdb_authn_make.h"
#include "nss_mcdb_authn.h"
#include "nss_mcdb.h"
#include "uint32.h"

#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>     /* strtol() strtoul() */
#include <arpa/inet.h>  /* htonl(), htons() */


#ifndef _AIX  /* no /etc/shadow on AIX */


#include <shadow.h>

/*
 * validate data sufficiently for successful serialize/deserialize into mcdb.
 *
 * input is database struct so that code is written to consumes what it produces
 *
 * (future: consider making *_datastr() routines 'static')
 *
 * (buf passed to *_datastr() routines is aligned,
 *  but care is taken to not rely on char *buf alignment)
 */

size_t
nss_mcdb_authn_make_spwd_datastr(char * restrict buf, const size_t bufsz,
				 const struct spwd * const restrict sp)
{
    const size_t sp_namp_len = 1 + strlen(sp->sp_namp);
    const size_t sp_pwdp_len = 1 + strlen(sp->sp_pwdp);
    const uintptr_t sp_pwdp_offset = sp_namp_len;
    const uintptr_t dlen = NSS_SP_HDRSZ + sp_pwdp_offset + sp_pwdp_len;
    union { uint32_t u[NSS_SP_HDRSZ>>2]; uint16_t h[NSS_SP_HDRSZ>>1]; } hdr;
    if (   __builtin_expect(dlen           <= bufsz,     1)
	&& __builtin_expect(sp_pwdp_offset <= USHRT_MAX, 1)) {
	/* store string offsets into aligned header, then copy into buf */
	/* copy strings into buffer, including string terminating '\0' */
	hdr.u[NSS_SP_LSTCHG>>2]   = htonl((uint32_t) sp->sp_lstchg);
	hdr.u[NSS_SP_MIN>>2]      = htonl((uint32_t) sp->sp_min);
	hdr.u[NSS_SP_MAX>>2]      = htonl((uint32_t) sp->sp_max);
	hdr.u[NSS_SP_WARN>>2]     = htonl((uint32_t) sp->sp_warn);
	hdr.u[NSS_SP_INACT>>2]    = htonl((uint32_t) sp->sp_inact);
	hdr.u[NSS_SP_EXPIRE>>2]   = htonl((uint32_t) sp->sp_expire);
	hdr.u[NSS_SP_FLAG>>2]     = htonl((uint32_t) sp->sp_flag);
	hdr.h[NSS_SP_PWDP>>1]     = htons((uint16_t) sp_pwdp_offset);
	hdr.h[(NSS_SP_HDRSZ>>1)-1]= 0;/*(clear last 2 bytes for consistency)*/
	memcpy(buf,                hdr.u,       NSS_SP_HDRSZ);
	memcpy((buf+=NSS_SP_HDRSZ),sp->sp_namp, sp_namp_len);
	memcpy(buf+sp_pwdp_offset, sp->sp_pwdp, sp_pwdp_len);
	return dlen - 1; /* subtract final '\0' */
    }
    else {
	errno = ERANGE;
	return 0;
    }
}



bool
nss_mcdb_authn_make_spwd_encode(
  struct nss_mcdb_make_winfo * const restrict w,
  const void * const restrict entp)
{
    const struct spwd * const restrict sp = entp;
    w->dlen = nss_mcdb_authn_make_spwd_datastr(w->data, w->datasz, sp);
    if (__builtin_expect( w->dlen == 0, 0))
        return false;

    w->tagc = '=';
    w->klen = strlen(sp->sp_namp);
    w->key  = sp->sp_namp;
    if (__builtin_expect( !nss_mcdb_make_mcdbctl_write(w), 0))
        return false;

    return true;
}



static char *
nss_mcdb_authn_make_parse_long_int_colon(char * const restrict b,
                                        #ifdef __sun
                                         int *
                                        #else
                                         long *
                                        #endif
                                                const restrict v)
{
    char *p = b;
    char *e;
    long n = -1;                        /* -1 if field is empty */
    TOKEN_COLONDELIM_END(p);
    if (*p != ':')
        return NULL;                    /* error: invalid line */
    if (b != p) {
        *p = '\0';
        errno = 0;
        n = strtol(b, &e, 10);
        if (*e != '\0' || ((n==LONG_MIN || n==LONG_MAX) && errno == ERANGE))
            return NULL;                /* error: invalid number */
        if (LONG_MAX != INT_MAX && (n < INT_MIN || INT_MAX < n))
            return NULL;                /* error: number out of range */
    }
  #ifdef __sun
    *v = (int)n;
  #else
    *v = n;
  #endif
    return p;
}


bool
nss_mcdb_authn_make_shadow_parse(
  struct nss_mcdb_make_winfo * const restrict w, char * restrict p)
{
    char *b, *e;
    unsigned long u;
    struct spwd sp;

    for (; *p; ++p) {

        /* skip comment lines beginning with '#' (supported by some vendors) */
        if (*p == '#')
            TOKEN_POUNDCOMMENT_SKIP(p);

        /* sp_namp */
        sp.sp_namp = b = p;
        TOKEN_COLONDELIM_END(p);
        if (*b == ':' || *p != ':')
            return false;               /* error: invalid line */
        *p = '\0';

        /* sp_pwdp */
        sp.sp_pwdp = b = ++p;
        TOKEN_COLONDELIM_END(p);
        if (*b == ':' || *p != ':')     /* (?is empty "" sp_pwdp permitted?) */
            return false;               /* error: invalid line */
        *p = '\0';

        /* sp_lstchg */
        p = nss_mcdb_authn_make_parse_long_int_colon(++p, &sp.sp_lstchg);
        if (p == NULL)
            return false;               /* error: invalid line */
        /* sp_min */
        p = nss_mcdb_authn_make_parse_long_int_colon(++p, &sp.sp_min);
        if (p == NULL)
            return false;               /* error: invalid line */
        /* sp_max */
        p = nss_mcdb_authn_make_parse_long_int_colon(++p, &sp.sp_max);
        if (p == NULL)
            return false;               /* error: invalid line */
        /* sp_warn */
        p = nss_mcdb_authn_make_parse_long_int_colon(++p, &sp.sp_warn);
        if (p == NULL)
            return false;               /* error: invalid line */
        /* sp_inact */
        p = nss_mcdb_authn_make_parse_long_int_colon(++p, &sp.sp_inact);
        if (p == NULL)
            return false;               /* error: invalid line */
        /* sp_expire */
        p = nss_mcdb_authn_make_parse_long_int_colon(++p, &sp.sp_expire);
        if (p == NULL)
            return false;               /* error: invalid line */

        /* sp_flag */
        b = ++p;
        TOKEN_COLONDELIM_END(p);
        if (*p != '\n')
            return false;               /* error: invalid line */
        sp.sp_flag = -1;                /* -1 if field is empty */
        if (b != p) {
            *p = '\0';
            errno = 0;
            u = strtoul(b, &e, 10);
            if (*e != '\0' || (u == ULONG_MAX && errno == ERANGE))
                return false;           /* error: invalid number */
            if (ULONG_MAX != UINT_MAX && UINT_MAX < u)
                return false;           /* error: number out of range */
            sp.sp_flag = u;
        }

        /* find newline (to prep for beginning of next line) (checked above) */

        /* process struct spwd */
        if (!w->encode(w, &sp))
            return false;
    }

    return true;
}


#else /* _AIX */


bool
nss_mcdb_authn_make_shadow_parse(
  struct nss_mcdb_make_winfo * const restrict w  __attribute_unused__,
  char * restrict p  __attribute_unused__)
{
    return false;
}

bool
nss_mcdb_authn_make_spwd_encode(
  struct nss_mcdb_make_winfo * const restrict w  __attribute_unused__,
  const void * const restrict entp  __attribute_unused__)
{
    return false;
}


#endif /* _AIX */
