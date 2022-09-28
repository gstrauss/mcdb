/*
 * nss_mcdb_netdb_make - mcdb of hosts,protocols,netgroup,networks,services,rpc
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

/* _ALL_SOURCE for struct rpcent on AIX */
#ifdef _AIX  /*mmap constants and basic networking on AIX require non-standard*/
#ifndef _ALL_SOURCE
#define _ALL_SOURCE
#endif
#endif
/* _BSD_SOURCE or _SVID_SOURCE for struct rpcent on Linux */
#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
/* _DARWIN_C_SOURCE for struct rpcent on Darwin */
#define PLASMA_FEATURE_ENABLE_BSD_SOURCE_TO_DARWIN_C_SOURCE

#include "nss_mcdb_netdb_make.h"
#include "nss_mcdb_netdb.h"
#include "nss_mcdb_make.h"

#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>     /* strtol() */
#include <netdb.h>
#include <sys/socket.h> /* AF_INET */
#include <arpa/inet.h>  /* inet_pton() ntohl() ntohs() htons() */

PLASMA_ATTR_Pragma_no_side_effect(strlen)

/* (similar to code nss_mcdb_acct_make.c:nss_mcdb_acct_make_group_datastr()) */
__attribute_nonnull__()
static size_t
nss_mcdb_netdb_make_list2str(char * const restrict buf, const size_t bufsz,
                             char * const * const restrict list,
                             size_t * const restrict dlenp)
{
    size_t dlen = *dlenp;
    size_t len;
    size_t num = 0;
    const char * restrict str;

    while ((str = list[num]) != NULL
	   && __builtin_expect( (len = 1 + strlen(str)) <= bufsz-dlen, 1)) {
	memcpy(buf+dlen, str, len);
	dlen += len;
	++num;
    }

    if (str == NULL) {
	*dlenp = dlen;
	return num;
    }
    else {
	errno = ERANGE;
	return ~(size_t)0; /* -1 */
    }
}

__attribute_nonnull__()
static size_t
nss_mcdb_netdb_make_hostent_datastr(char * restrict buf, const size_t bufsz,
				    const struct hostent * const restrict he)
{
    const size_t he_name_len       = 1 + strlen(he->h_name);
    const size_t he_mem_str_offset = he_name_len;
    size_t he_lst_str_offset;
    char ** const restrict he_lst  = he->h_addr_list;
    const size_t len               = (size_t)(unsigned int)he->h_length;
    size_t he_mem_num;
    size_t he_lst_num = 0;
    size_t dlen = NSS_HE_HDRSZ + he_mem_str_offset;
    union { uint32_t u[NSS_HE_HDRSZ>>2]; uint16_t h[NSS_HE_HDRSZ>>1]; } hdr;
    if (   __builtin_expect(he_name_len <= USHRT_MAX, 1)
	&& __builtin_expect(dlen        <  bufsz,     1)) {
	he_mem_num =nss_mcdb_netdb_make_list2str(buf,bufsz,he->h_aliases,&dlen);
	if (__builtin_expect( he_mem_num == ~(size_t)0, 0))
	    return 0;
	he_lst_str_offset = dlen - NSS_HE_HDRSZ;
	while (he_lst[he_lst_num] != NULL
	       && __builtin_expect(len < bufsz-dlen, 1)) {
	    memcpy(buf+dlen, he_lst[he_lst_num], len); /*binary address*/
	    dlen += len;
	    ++he_lst_num;
	} /* check for he_lst[he_lst_num] == NULL for sufficient buf space */
	if (   __builtin_expect(he_mem_num <= USHRT_MAX, 1)
	    && __builtin_expect(he_lst_num <= USHRT_MAX, 1)
	    && __builtin_expect(he_lst[he_lst_num] == NULL,  1)
	    && __builtin_expect(((he_mem_num+he_lst_num)<<3)+8u+8u+7u
                                <= bufsz-dlen, 1)) {
	    /* verify space in string for (2) 8-aligned char ** array + NULL
	     * (not strictly necessary, but best to catch excessively long
	     *  entries at mcdb create time rather than in query at runtime) */

	    /* store string offsets into aligned header, then copy into buf */
	    /* copy strings into buffer, including string terminating '\0' */
	    hdr.u[NSS_H_ADDRTYPE>>2]  = htonl((uint32_t) he->h_addrtype);
	    hdr.u[NSS_H_LENGTH>>2]    = htonl((uint32_t) he->h_length);
	    hdr.h[NSS_HE_MEM>>1]      = htons((uint16_t)(dlen - NSS_HE_HDRSZ));
	    hdr.h[NSS_HE_MEM_STR>>1]  = htons((uint16_t) he_mem_str_offset);
	    hdr.h[NSS_HE_LST_STR>>1]  = htons((uint16_t) he_lst_str_offset);
	    hdr.h[NSS_HE_MEM_NUM>>1]  = htons((uint16_t) he_mem_num);
	    hdr.h[NSS_HE_LST_NUM>>1]  = htons((uint16_t) he_lst_num);
	    hdr.h[(NSS_HE_HDRSZ>>1)-1]= 0;/*(clear last 2 bytes (consistency))*/
	    memcpy(buf,              hdr.u,      NSS_HE_HDRSZ);
	    memcpy(buf+NSS_HE_HDRSZ, he->h_name, he_name_len);
	    return dlen;
	}
    }

    errno = ERANGE;
    return 0;
}


#ifndef _AIX
__attribute_nonnull__()
static size_t
nss_mcdb_netdb_make_netent_datastr(char * restrict buf, const size_t bufsz,
				   const struct netent * const restrict ne)
#else
__attribute_nonnull__()
static size_t
nss_mcdb_netdb_make_netent_datastr(char * restrict buf, const size_t bufsz,
				   const struct nwent * const restrict ne)
#endif
{
    const size_t    ne_name_len       = 1 + strlen(ne->n_name);
    const uintptr_t ne_mem_str_offset = ne_name_len;
    size_t ne_mem_num;
    size_t dlen = NSS_NE_HDRSZ + ne_mem_str_offset;
    union { uint32_t u[NSS_NE_HDRSZ>>2]; uint16_t h[NSS_NE_HDRSZ>>1]; } hdr;
    if (   __builtin_expect(ne_name_len <= USHRT_MAX, 1)
	&& __builtin_expect(dlen        <  bufsz,     1)) {
	ne_mem_num =nss_mcdb_netdb_make_list2str(buf,bufsz,ne->n_aliases,&dlen);
	if (__builtin_expect( ne_mem_num == ~(size_t)0, 0))
	    return 0;
	if (   __builtin_expect(ne_mem_num <= USHRT_MAX, 1)
	    && __builtin_expect((ne_mem_num<<3)+8u+7u <= bufsz-dlen, 1)) {
	    /* verify space in string for 8-aligned char ** ne_mem array + NULL
	     * (not strictly necessary, but best to catch excessively long
	     *  entries at mcdb create time rather than in query at runtime) */

	    /* store string offsets into aligned header, then copy into buf */
	    /* copy strings into buffer, including string terminating '\0' */
	    hdr.u[NSS_N_ADDRTYPE>>2]  = htonl((uint32_t) ne->n_addrtype);
          #ifndef _AIX
	    hdr.u[NSS_N_NET>>2]       = htonl((uint32_t) ne->n_net);
          #else
	    hdr.u[NSS_N_NET>>2]       = *(uint32_t *)ne->n_addr;
          #endif
	    hdr.h[NSS_NE_MEM>>1]      = htons((uint16_t)(dlen - NSS_NE_HDRSZ));
	    hdr.h[NSS_NE_MEM_STR>>1]  = htons((uint16_t) ne_mem_str_offset);
	    hdr.h[NSS_NE_MEM_NUM>>1]  = htons((uint16_t) ne_mem_num);
          #ifdef _AIX
	    hdr.h[(NSS_N_LENGTH>>1)]  = htons((uint16_t) ne->n_length);/*_AIX*/
          #else
	    hdr.h[(NSS_NE_HDRSZ>>1)-1]= 0;/*(clear last 2 bytes (consistency))*/
          #endif
	    memcpy(buf,              hdr.u,      NSS_NE_HDRSZ);
	    memcpy(buf+NSS_NE_HDRSZ, ne->n_name, ne_name_len);
	    return dlen;
	}
    }

    errno = ERANGE;
    return 0;
}


__attribute_nonnull__()
static size_t
nss_mcdb_netdb_make_protoent_datastr(char * restrict buf, const size_t bufsz,
				     const struct protoent * const restrict pe)
{
    const size_t    pe_name_len       = 1 + strlen(pe->p_name);
    const uintptr_t pe_mem_str_offset = pe_name_len;
    size_t pe_mem_num;
    size_t dlen = NSS_PE_HDRSZ + pe_mem_str_offset;
    union { uint32_t u[NSS_PE_HDRSZ>>2]; uint16_t h[NSS_PE_HDRSZ>>1]; } hdr;
    if (   __builtin_expect(pe_name_len <= USHRT_MAX, 1)
	&& __builtin_expect(dlen        <  bufsz,     1)) {
	pe_mem_num =nss_mcdb_netdb_make_list2str(buf,bufsz,pe->p_aliases,&dlen);
	if (__builtin_expect( pe_mem_num == ~(size_t)0, 0))
	    return 0;
	if (   __builtin_expect(pe_mem_num <= USHRT_MAX, 1)
	    && __builtin_expect((pe_mem_num<<3)+8u+7u <= bufsz-dlen, 1)) {
	    /* verify space in string for 8-aligned char ** pe_mem array + NULL
	     * (not strictly necessary, but best to catch excessively long
	     *  entries at mcdb create time rather than in query at runtime) */

	    /* store string offsets into aligned header, then copy into buf */
	    /* copy strings into buffer, including string terminating '\0' */
	    hdr.u[NSS_P_PROTO>>2]     = htonl((uint32_t) pe->p_proto);
	    hdr.h[NSS_PE_MEM>>1]      = htons((uint16_t)(dlen - NSS_PE_HDRSZ));
	    hdr.h[NSS_PE_MEM_STR>>1]  = htons((uint16_t) pe_mem_str_offset);
	    hdr.h[NSS_PE_MEM_NUM>>1]  = htons((uint16_t) pe_mem_num);
	    hdr.h[(NSS_PE_HDRSZ>>1)-1]= 0;/*(clear last 2 bytes (consistency))*/
	    memcpy(buf,              hdr.u,      NSS_PE_HDRSZ);
	    memcpy(buf+NSS_PE_HDRSZ, pe->p_name, pe_name_len);
	    return dlen;
	}
    }

    errno = ERANGE;
    return 0;
}


__attribute_nonnull__()
static size_t
nss_mcdb_netdb_make_rpcent_datastr(char * restrict buf, const size_t bufsz,
				   const struct rpcent * const restrict re)
{
    const size_t    re_name_len       = 1 + strlen(re->r_name);
    const uintptr_t re_mem_str_offset = re_name_len;
    size_t re_mem_num;
    size_t dlen = NSS_RE_HDRSZ + re_mem_str_offset;
    union { uint32_t u[NSS_RE_HDRSZ>>2]; uint16_t h[NSS_RE_HDRSZ>>1]; } hdr;
    if (   __builtin_expect(re_name_len <= USHRT_MAX, 1)
	&& __builtin_expect(dlen        <  bufsz,     1)) {
	re_mem_num =nss_mcdb_netdb_make_list2str(buf,bufsz,re->r_aliases,&dlen);
	if (__builtin_expect( re_mem_num == ~(size_t)0, 0))
	    return 0;
	if (   __builtin_expect(re_mem_num <= USHRT_MAX, 1)
	    && __builtin_expect((re_mem_num<<3)+8u+7u <= bufsz-dlen, 1)) {
	    /* verify space in string for 8-aligned char ** re_mem array + NULL
	     * (not strictly necessary, but best to catch excessively long
	     *  entries at mcdb create time rather than in query at runtime) */

	    /* store string offsets into aligned header, then copy into buf */
	    /* copy strings into buffer, including string terminating '\0' */
	    hdr.u[NSS_R_NUMBER>>2]    = htonl((uint32_t) re->r_number);
	    hdr.h[NSS_RE_MEM>>1]      = htons((uint16_t)(dlen - NSS_RE_HDRSZ));
	    hdr.h[NSS_RE_MEM_STR>>1]  = htons((uint16_t) re_mem_str_offset);
	    hdr.h[NSS_RE_MEM_NUM>>1]  = htons((uint16_t) re_mem_num);
	    hdr.h[(NSS_RE_HDRSZ>>1)-1]= 0;/*(clear last 2 bytes (consistency))*/
	    memcpy(buf,              hdr.u,      NSS_RE_HDRSZ);
	    memcpy(buf+NSS_RE_HDRSZ, re->r_name, re_name_len);
	    return dlen;
	}
    }

    errno = ERANGE;
    return 0;
}


__attribute_nonnull__()
static size_t
nss_mcdb_netdb_make_servent_datastr(char * restrict buf, const size_t bufsz,
				    const struct servent * const restrict se)
{
    /*(proto is first element instead of name for use by query code)*/
    const size_t    se_name_len       = 1 + strlen(se->s_name);
    const size_t    se_proto_len      = 1 + strlen(se->s_proto);
    const uintptr_t se_name_offset    = se_proto_len;
    const uintptr_t se_mem_str_offset = se_name_offset + se_name_len;
    size_t se_mem_num;
    size_t dlen = NSS_SE_HDRSZ + se_mem_str_offset;
    union { uint32_t u[NSS_SE_HDRSZ>>2]; uint16_t h[NSS_SE_HDRSZ>>1]; } hdr;
    if (   __builtin_expect(se_name_len <= USHRT_MAX, 1)
	&& __builtin_expect(dlen        <  bufsz,     1)) {
	se_mem_num =nss_mcdb_netdb_make_list2str(buf,bufsz,se->s_aliases,&dlen);
	if (__builtin_expect( se_mem_num == ~(size_t)0, 0))
	    return 0;
	if (   __builtin_expect(se_mem_num <= USHRT_MAX, 1)
	    && __builtin_expect((se_mem_num<<3)+8u+7u <= bufsz-dlen, 1)) {
	    /* verify space in string for 8-aligned char ** se_mem array + NULL
	     * (not strictly necessary, but best to catch excessively long
	     *  entries at mcdb create time rather than in query at runtime) */

	    /* store string offsets into aligned header, then copy into buf */
	    /* copy strings into buffer, including string terminating '\0' */
	    hdr.u[NSS_S_PORT>>2]      = (uint32_t) se->s_port;/*net byte order*/
	    hdr.h[NSS_S_NAME>>1]      = htons((uint16_t) se_name_offset);
	    hdr.h[NSS_SE_MEM>>1]      = htons((uint16_t)(dlen - NSS_SE_HDRSZ));
	    hdr.h[NSS_SE_MEM_STR>>1]  = htons((uint16_t) se_mem_str_offset);
	    hdr.h[NSS_SE_MEM_NUM>>1]  = htons((uint16_t) se_mem_num);
	    memcpy(buf,                 hdr.u,       NSS_SE_HDRSZ);
	    memcpy((buf+=NSS_SE_HDRSZ), se->s_proto, se_proto_len);
	    memcpy(buf+se_name_offset,  se->s_name,  se_name_len);
	    return dlen;
	}
    }

    errno = ERANGE;
    return 0;
}



bool
nss_mcdb_netdb_make_hostent_encode(
  struct nss_mcdb_make_winfo * const restrict w,
  const void * const entp)
{
    const struct hostent * const restrict he = entp;
    uintptr_t i;

    w->dlen = nss_mcdb_netdb_make_hostent_datastr(w->data, w->datasz, he);
    if (__builtin_expect( w->dlen == 0, 0))
        return false;

    w->tagc = '=';
    w->klen = strlen(he->h_name);
    w->key  = he->h_name;
    if (__builtin_expect( !nss_mcdb_make_mcdbctl_write(w), 0))
        return false;

    w->tagc = '~';
    if (__builtin_expect( !nss_mcdb_make_mcdbctl_write(w), 0))
        return false;

    for (i = 0; he->h_aliases[i] != NULL; ++i) {
        w->tagc = '~';
        w->klen = strlen(he->h_aliases[i]);
        w->key  = he->h_aliases[i];
        if (__builtin_expect( !nss_mcdb_make_mcdbctl_write(w), 0))
            return false;
    }

    /* one address per line in /etc/hosts, so support encoding only one addr */
    w->tagc = 'b';  /* binary */
    w->klen = (uint32_t)he->h_length;
    w->key  = he->h_addr_list[0];
    if (__builtin_expect( !nss_mcdb_make_mcdbctl_write(w), 0))
        return false;

    return true;
}


bool
nss_mcdb_netdb_make_netent_encode(
  struct nss_mcdb_make_winfo * const restrict w,
  const void * const entp)
{
  #ifndef _AIX
    const struct netent * const restrict ne = entp;
  #else
    const struct nwent * const restrict ne = entp;
  #endif
    uintptr_t i;
    uint32_t n[2];

    w->dlen = nss_mcdb_netdb_make_netent_datastr(w->data, w->datasz, ne);
    if (__builtin_expect( w->dlen == 0, 0))
        return false;

    w->tagc = '=';
    w->klen = strlen(ne->n_name);
    w->key  = ne->n_name;
    if (__builtin_expect( !nss_mcdb_make_mcdbctl_write(w), 0))
        return false;

    w->tagc = '~';
    if (__builtin_expect( !nss_mcdb_make_mcdbctl_write(w), 0))
        return false;

    for (i = 0; ne->n_aliases[i] != NULL; ++i) {
        w->tagc = '~';
        w->klen = strlen(ne->n_aliases[i]);
        w->key  = ne->n_aliases[i];
        if (__builtin_expect( !nss_mcdb_make_mcdbctl_write(w), 0))
            return false;
    }

    w->tagc = 'x';
    w->klen = sizeof(n);
    w->key  = (const char *)n;
  #ifndef _AIX
    n[0] = htonl((uint32_t) ne->n_net);
  #else
    n[0] = *(uint32_t *)ne->n_addr;
  #endif
    n[1] = htonl((uint32_t) ne->n_addrtype);
    if (__builtin_expect( !nss_mcdb_make_mcdbctl_write(w), 0))
        return false;

    return true;
}


bool
nss_mcdb_netdb_make_protoent_encode(
  struct nss_mcdb_make_winfo * const restrict w,
  const void * const entp)
{
    const struct protoent * const restrict pe = entp;
    uintptr_t i;
    const uint32_t n = htonl((uint32_t) pe->p_proto);

    w->dlen = nss_mcdb_netdb_make_protoent_datastr(w->data, w->datasz, pe);
    if (__builtin_expect( w->dlen == 0, 0))
        return false;

    w->tagc = '=';
    w->klen = strlen(pe->p_name);
    w->key  = pe->p_name;
    if (__builtin_expect( !nss_mcdb_make_mcdbctl_write(w), 0))
        return false;

    w->tagc = '~';
    if (__builtin_expect( !nss_mcdb_make_mcdbctl_write(w), 0))
        return false;

    for (i = 0; pe->p_aliases[i] != NULL; ++i) {
        w->tagc = '~';
        w->klen = strlen(pe->p_aliases[i]);
        w->key  = pe->p_aliases[i];
        if (__builtin_expect( !nss_mcdb_make_mcdbctl_write(w), 0))
            return false;
    }

    w->tagc = 'x';
    w->klen = sizeof(uint32_t);
    w->key  = (const char *)&n;
    if (__builtin_expect( !nss_mcdb_make_mcdbctl_write(w), 0))
        return false;

    return true;
}


bool
nss_mcdb_netdb_make_rpcent_encode(
  struct nss_mcdb_make_winfo * const restrict w,
  const void * const entp)
{
    const struct rpcent * const restrict re = entp;
    uintptr_t i;
    const uint32_t n = htonl((uint32_t) re->r_number);

    w->dlen = nss_mcdb_netdb_make_rpcent_datastr(w->data, w->datasz, re);
    if (__builtin_expect( w->dlen == 0, 0))
        return false;

    w->tagc = '=';
    w->klen = strlen(re->r_name);
    w->key  = re->r_name;
    if (__builtin_expect( !nss_mcdb_make_mcdbctl_write(w), 0))
        return false;

    w->tagc = '~';
    if (__builtin_expect( !nss_mcdb_make_mcdbctl_write(w), 0))
        return false;

    for (i = 0; re->r_aliases[i] != NULL; ++i) {
        w->tagc = '~';
        w->klen = strlen(re->r_aliases[i]);
        w->key  = re->r_aliases[i];
        if (__builtin_expect( !nss_mcdb_make_mcdbctl_write(w), 0))
            return false;
    }

    w->tagc = 'x';
    w->klen = sizeof(uint32_t);
    w->key  = (const char *)&n;
    if (__builtin_expect( !nss_mcdb_make_mcdbctl_write(w), 0))
        return false;

    return true;
}


bool
nss_mcdb_netdb_make_servent_encode(
  struct nss_mcdb_make_winfo * const restrict w,
  const void * const entp)
{
    const struct servent * const restrict se = entp;
    uintptr_t i;
    const uint32_t n = (uint32_t) se->s_port;/*(already in network btye order)*/

    w->dlen = nss_mcdb_netdb_make_servent_datastr(w->data, w->datasz, se);
    if (__builtin_expect( w->dlen == 0, 0))
        return false;

    w->tagc = '=';
    w->klen = strlen(se->s_name);
    w->key  = se->s_name;
    if (__builtin_expect( !nss_mcdb_make_mcdbctl_write(w), 0))
        return false;

    w->tagc = '~';
    if (__builtin_expect( !nss_mcdb_make_mcdbctl_write(w), 0))
        return false;

    for (i = 0; se->s_aliases[i] != NULL; ++i) {
        w->tagc = '~';
        w->klen = strlen(se->s_aliases[i]);
        w->key  = se->s_aliases[i];
        if (__builtin_expect( !nss_mcdb_make_mcdbctl_write(w), 0))
            return false;
    }

    w->tagc = 'x';
    w->klen = sizeof(uint32_t);
    w->key  = (const char *)&n;
    if (__builtin_expect( !nss_mcdb_make_mcdbctl_write(w), 0))
        return false;

    return true;
}



bool
nss_mcdb_netdb_make_hosts_parse(
  struct nss_mcdb_make_winfo * const restrict w, char * restrict p, size_t plen)
{
    char *b;
    int c;
    int n;
    struct hostent he;
    struct in_addr in_addr;
    struct in6_addr in6_addr;
    char *h_addr_list[2] = { NULL, NULL }; /* one addr only in /etc/hosts */
    char *h_aliases[256];   /* use DNS if host has more than 255 aliases! */
    /*(255 aliases + canonical name amounts to 1 KB of (256) 3-char names)*/
    (void)plen; /*(unused)*/

    he.h_aliases   = h_aliases;
    he.h_addr_list = h_addr_list;

    for (; *p; ++p) {

        /* h_addr_list */
        TOKEN_WSDELIM_BEGIN(p);
        b = p;
        TOKEN_WSDELIM_END(p);
        if (*b == '\n')                 /* blank line; continue */
            continue;
        if (*b == '#') {
            do { ++p; } while (*p != '\n' && *p != '\0');
            if (*p == '\n')
                continue;               /* blank line; continue */
            else
                return false;           /* error: no newline; truncated? */
        }
        if (*p == '\n' || *p == '\0')   /* error: invalid line */
            return false;
        *p = '\0';
        if (inet_pton(AF_INET, b, &in_addr) > 0) {
            he.h_addr_list[0] = (char *)&in_addr;
            he.h_length = sizeof(struct in_addr);
            he.h_addrtype = AF_INET;
        }
        else if (inet_pton(AF_INET6, b, &in6_addr) > 0) {
            he.h_addr_list[0] = (char *)&in6_addr;
            he.h_length = sizeof(struct in6_addr);
            he.h_addrtype = AF_INET6;
        }
        else                            /* error: invalid addr */
            return false;

        /* h_name */
        ++p;
        TOKEN_WSDELIM_BEGIN(p);
        b = p;
        TOKEN_WSDELIM_END(p);
        if ((c = *p) == '\0' || *b == '#' || *b == '\n')/* error: invalid line*/
            return false;
        *p = '\0';
        he.h_name = b;

        /* h_aliases */
        n = 0;
        while (c != '#' && c != '\n') {
            ++p;
            TOKEN_WSDELIM_BEGIN(p);
            b = p;
            TOKEN_WSDELIM_END(p);
            if ((c = *p) == '\0')       /* error: invalid line */
                return false;
            if (*b == '#' || *b == '\n')/* done; no more aliases*/
                break;
            *p = '\0';
            if (n < (int)(sizeof(h_aliases)/sizeof(char *) - 1))
                he.h_aliases[n++] = b;
            else
                return false; /* too many aliases to fit in fixed-sized array */
        }
        he.h_aliases[n] = NULL;

        /* find newline (to prep for beginning of next line) */
        if (c != '\n') {
            TOKEN_WSDELIM_BEGIN(p);
            if (*p == '#') {
                do { ++p; } while (*p != '\n' && *p != '\0');
            }
            if (*p != '\n')   /* error: no newline; truncated? */
                return false;
        }

        /* process struct hostent */
        if (!w->encode(w, &he))
            return false;
    }

    return true;
}


bool
nss_mcdb_netdb_make_networks_parse(
  struct nss_mcdb_make_winfo * const restrict w, char * restrict p, size_t plen)
{
    char *b;
    int c;
    int n;
  #ifndef _AIX
    struct netent ne;
  #else
    struct nwent ne;
  #endif
    struct in_addr in_addr;
    char *n_aliases[256];
    /*(255 aliases + canonical name amounts to 1 KB of (256) 3-char names)*/
    (void)plen; /*(unused)*/

    ne.n_aliases = n_aliases;

    for (; *p; ++p) {

        /* n_name */
        TOKEN_WSDELIM_BEGIN(p);
        b = p;
        TOKEN_WSDELIM_END(p);
        if (*b == '\n')                 /* blank line; continue */
            continue;
        if (*b == '#') {
            do { ++p; } while (*p != '\n' && *p != '\0');
            if (*p == '\n')
                continue;               /* blank line; continue */
            else
                return false;           /* error: no newline; truncated? */
        }
        if (*p == '\n' || *p == '\0')   /* error: invalid line */
            return false;
        *p = '\0';
        ne.n_name = b;

        /* n_net, n_addrtype */
        ++p;
        TOKEN_WSDELIM_BEGIN(p);
        b = p;
        TOKEN_WSDELIM_END(p);
        if ((c = *p) == '\0' || *b == '#' || *b == '\n')/* error: invalid line*/
            return false;
        *p = '\0';
        ne.n_addrtype = AF_INET;
      #ifndef _AIX
        if (inet_pton(AF_INET, b, &in_addr) > 0)
            ne.n_net = ntohl((uint32_t)in_addr.s_addr);  /* (host byte order) */
        else                            /* error: invalid or unsupported addr */
            return false;
      #else  /* _AIX: struct nwent has n_addr, n_length instead of n_net */
        ne.n_length = inet_net_pton(AF_INET,b,&in_addr,sizeof(struct in_addr));
        if (ne.n_length != -1)
            ne.n_addr = &in_addr;                     /* (network byte order) */
        else                            /* error: invalid or unsupported addr */
            return false;
      #endif /* _AIX */

        /* n_aliases */
        n = 0;
        while (c != '#' && c != '\n') {
            ++p;
            TOKEN_WSDELIM_BEGIN(p);
            b = p;
            TOKEN_WSDELIM_END(p);
            if ((c = *p) == '\0')       /* error: invalid line */
                return false;
            if (*b == '#' || *b == '\n')/* done; no more aliases*/
                break;
            *p = '\0';
            if (n < (int)(sizeof(n_aliases)/sizeof(char *) - 1))
                ne.n_aliases[n++] = b;
            else
                return false; /* too many aliases to fit in fixed-sized array */
        }
        ne.n_aliases[n] = NULL;

        /* find newline (to prep for beginning of next line) */
        if (c != '\n') {
            TOKEN_WSDELIM_BEGIN(p);
            if (*p == '#') {
                do { ++p; } while (*p != '\n' && *p != '\0');
            }
            if (*p != '\n')   /* error: no newline; truncated? */
                return false;
        }

        /* process struct netent */
        if (!w->encode(w, &ne))
            return false;
    }

    return true;
}


bool
nss_mcdb_netdb_make_protocols_parse(
  struct nss_mcdb_make_winfo * const restrict w, char * restrict p, size_t plen)
{
    char *b, *e;
    int c;
    long n;
    struct protoent pe;
    char *p_aliases[256];
    /*(255 aliases + canonical name amounts to 1 KB of (256) 3-char names)*/
    (void)plen; /*(unused)*/

    pe.p_aliases = p_aliases;

    for (; *p; ++p) {

        /* p_name */
        TOKEN_WSDELIM_BEGIN(p);
        b = p;
        TOKEN_WSDELIM_END(p);
        if (*b == '\n')                 /* blank line; continue */
            continue;
        if (*b == '#') {
            do { ++p; } while (*p != '\n' && *p != '\0');
            if (*p == '\n')
                continue;               /* blank line; continue */
            else
                return false;           /* error: no newline; truncated? */
        }
        if (*p == '\n' || *p == '\0')   /* error: invalid line */
            return false;
        *p = '\0';
        pe.p_name = b;

        /* p_proto */
        ++p;
        TOKEN_WSDELIM_BEGIN(p);
        b = p;
        TOKEN_WSDELIM_END(p);
        if ((c = *p) == '\0' || *b == '#' || *b == '\n')/* error: invalid line*/
            return false;
        *p = '\0';
        n = strtol(b, &e, 10);
        if (*e != '\0' || n < 0 || n == LONG_MAX || n >= INT_MAX)
            return false;               /* error: invalid proto */
        pe.p_proto = (int)n;

        /* p_aliases */
        n = 0;
        while (c != '#' && c != '\n') {
            ++p;
            TOKEN_WSDELIM_BEGIN(p);
            b = p;
            TOKEN_WSDELIM_END(p);
            if ((c = *p) == '\0')       /* error: invalid line */
                return false;
            if (*b == '#' || *b == '\n')/* done; no more aliases*/
                break;
            *p = '\0';
            if (n < (int)(sizeof(p_aliases)/sizeof(char *) - 1))
                pe.p_aliases[n++] = b;
            else
                return false; /* too many aliases to fit in fixed-sized array */
        }
        pe.p_aliases[n] = NULL;

        /* find newline (to prep for beginning of next line) */
        if (c != '\n') {
            TOKEN_WSDELIM_BEGIN(p);
            if (*p == '#') {
                do { ++p; } while (*p != '\n' && *p != '\0');
            }
            if (*p != '\n')   /* error: no newline; truncated? */
                return false;
        }

        /* process struct protoent */
        if (!w->encode(w, &pe))
            return false;
    }

    return true;
}


bool
nss_mcdb_netdb_make_rpc_parse(
  struct nss_mcdb_make_winfo * const restrict w, char * restrict p, size_t plen)
{
    char *b, *e;
    int c;
    long n;
    struct rpcent re;
    char *r_aliases[256];
    /*(255 aliases + canonical name amounts to 1 KB of (256) 3-char names)*/
    (void)plen; /*(unused)*/

    re.r_aliases = r_aliases;

    for (; *p; ++p) {

        /* r_name */
        TOKEN_WSDELIM_BEGIN(p);
        b = p;
        TOKEN_WSDELIM_END(p);
        if (*b == '\n')                 /* blank line; continue */
            continue;
        if (*b == '#') {
            do { ++p; } while (*p != '\n' && *p != '\0');
            if (*p == '\n')
                continue;               /* blank line; continue */
            else
                return false;           /* error: no newline; truncated? */
        }
        if (*p == '\n' || *p == '\0')   /* error: invalid line */
            return false;
        *p = '\0';
        re.r_name = b;

        /* r_number */
        ++p;
        TOKEN_WSDELIM_BEGIN(p);
        b = p;
        TOKEN_WSDELIM_END(p);
        if ((c = *p) == '\0' || *b == '#' || *b == '\n')/* error: invalid line*/
            return false;
        *p = '\0';
        n = strtol(b, &e, 10);
        if (*e != '\0' || n < 0 || n == LONG_MAX || n >= INT_MAX)
            return false;               /* error: invalid number */
        re.r_number = (int)n;

        /* r_aliases */
        n = 0;
        while (c != '#' && c != '\n') {
            ++p;
            TOKEN_WSDELIM_BEGIN(p);
            b = p;
            TOKEN_WSDELIM_END(p);
            if ((c = *p) == '\0')       /* error: invalid line */
                return false;
            if (*b == '#' || *b == '\n')/* done; no more aliases*/
                break;
            *p = '\0';
            if (n < (int)(sizeof(r_aliases)/sizeof(char *) - 1))
                re.r_aliases[n++] = b;
            else
                return false; /* too many aliases to fit in fixed-sized array */
        }
        re.r_aliases[n] = NULL;

        /* find newline (to prep for beginning of next line) */
        if (c != '\n') {
            TOKEN_WSDELIM_BEGIN(p);
            if (*p == '#') {
                do { ++p; } while (*p != '\n' && *p != '\0');
            }
            if (*p != '\n')   /* error: no newline; truncated? */
                return false;
        }

        /* process struct rpcent */
        if (!w->encode(w, &re))
            return false;
    }

    return true;
}


bool
nss_mcdb_netdb_make_services_parse(
  struct nss_mcdb_make_winfo * const restrict w, char * restrict p, size_t plen)
{
    char *b, *e;
    int c;
    long n;
    struct servent se;
    char *s_aliases[256];
    /*(255 aliases + canonical name amounts to 1 KB of (256) 3-char names)*/
    (void)plen; /*(unused)*/

    se.s_aliases = s_aliases;

    for (; *p; ++p) {

        /* r_name */
        TOKEN_WSDELIM_BEGIN(p);
        b = p;
        TOKEN_WSDELIM_END(p);
        if (*b == '\n')                 /* blank line; continue */
            continue;
        if (*b == '#') {
            do { ++p; } while (*p != '\n' && *p != '\0');
            if (*p == '\n')
                continue;               /* blank line; continue */
            else
                return false;           /* error: no newline; truncated? */
        }
        if (*p == '\n' || *p == '\0')   /* error: invalid line */
            return false;
        *p = '\0';
        se.s_name = b;

        /* s_port */
        ++p;
        TOKEN_WSDELIM_BEGIN(p);
        b = p;
        TOKEN_WSDELIM_END(p);
        if ((c = *p) == '\0' || *b == '#' || *b == '\n')/* error: invalid line*/
            return false;
        *p = '\0';
        n = strtol(b, &e, 10);          /* <netinet/in.h> in_port_t uint16_t */
        if (*e != '/' || n < 0 || n == LONG_MAX || n > USHRT_MAX)
            return false;               /* error: invalid number */
        se.s_port = (int)htons((uint16_t)n); /* store in network byte order */
        if (e+1 == p)
            return false;               /* error: empty proto string */
        se.s_proto = e+1;

        /* s_aliases */
        n = 0;
        while (c != '#' && c != '\n') {
            ++p;
            TOKEN_WSDELIM_BEGIN(p);
            b = p;
            TOKEN_WSDELIM_END(p);
            if ((c = *p) == '\0')       /* error: invalid line */
                return false;
            if (*b == '#' || *b == '\n')/* done; no more aliases*/
                break;
            *p = '\0';
            if (n < (int)(sizeof(s_aliases)/sizeof(char *) - 1))
                se.s_aliases[n++] = b;
            else
                return false; /* too many aliases to fit in fixed-sized array */
        }
        se.s_aliases[n] = NULL;

        /* find newline (to prep for beginning of next line) */
        if (c != '\n') {
            TOKEN_WSDELIM_BEGIN(p);
            if (*p == '#') {
                do { ++p; } while (*p != '\n' && *p != '\0');
            }
            if (*p != '\n')   /* error: no newline; truncated? */
                return false;
        }

        /* process struct servent */
        if (!w->encode(w, &se))
            return false;
    }

    return true;
}


/*
 * /etc/netgroup
 *
 * Note: maximum length of host, user, or domain string is 255 chars each
 */

#include "../uint32.h"
#include <ctype.h>      /* isalnum() tolower() */
#include <stdlib.h>     /* malloc() calloc() realloc() free() */


struct ngnode {
  struct ngnode *next;
  uint32_t h;
  uint32_t klen;
  void *k;
  void *v;
};

struct ngtable {
  struct ngarray { struct ngnode **nodes; size_t used; size_t sz; } a;
  struct nghash  { struct ngnode **bkts; size_t nbkts; } h;
};


__attribute_nonnull__()
static void
netgroup_ngtable_free (struct ngtable * const restrict t)
{
    for (size_t i = 0, used = t->a.used; i < used; ++i) {
        free(t->a.nodes[i]->k);
        free(t->a.nodes[i]);
    }
    free(t->a.nodes);
    free(t->h.bkts);
}


__attribute_nonnull__()
static bool
netgroup_ngtable_init (struct ngtable * const restrict t, const size_t hint)
{
    /* create initial number of hash table buckets and array slots based on
     * heuristic from file size to minimize realloc of array, and reduce the
     * need to rebalance hash table (e.g. due to collisions in a full table)
     * Note: initial allocation required here for simple power-2 resize logic
     */
    t->h.bkts = (struct ngnode **)calloc((hint << 1), sizeof(struct ngnode *));
    if (NULL == t->h.bkts) return false;
    t->h.nbkts = (hint << 1);

    t->a.nodes = (struct ngnode **)malloc(hint * sizeof(struct ngnode *));
    if (NULL == t->a.nodes) return false;
    t->a.sz = hint;
    t->a.used = 0;

    return true;
}


__attribute_nonnull__()
static void *
netgroup_node_insert_id (struct ngtable * const restrict t,
                         const void * const restrict vbuf, const size_t sz)
{
    /* hash table lookup */
    const uint32_t hash = uint32_hash_djb(UINT32_HASH_DJB_INIT, vbuf, sz);
    struct ngnode ** restrict bktp = &t->h.bkts[hash & (t->h.nbkts - 1)];
    for (struct ngnode * restrict bn; (bn = *bktp); bktp = &bn->next) {
        if (bn->h == hash && bn->klen == sz && 0 == memcmp(bn->k, vbuf, sz)) {
            return bn->v; /* vbuf found in hash table */
        }
    }

    if (t->a.used > (SIZE_MAX >> 4)) { errno = ERANGE; return (void *)-1; }

    struct ngnode * const restrict n =
      (struct ngnode *)malloc(sizeof(struct ngnode));
    if (NULL == n) return (void *)-1;

    n->next = NULL;
    n->h = hash;
    n->klen = sz;
    n->k = malloc(sz);
    if (NULL == n->k) { free(n); return (void *)-1; }
    memcpy(n->k, vbuf, sz);

    *bktp = n; /* hash table insert */

    if (t->a.used == t->a.sz) { /* array resize (if needed) */
        struct ngnode ** const restrict np = (struct ngnode **)
          realloc(t->a.nodes, (t->a.sz <<= 1) * sizeof(struct ngnode *));
        if (NULL == np) return (void *)-1;
        t->a.nodes = np;
    }
    n->v = (void *)t->a.used;
    t->a.nodes[t->a.used++] = n; /* array insert */

  #if 0
    if (t->h.nbkts <= (t->a.used << 1)) {/* hash table rebalance (if needed) */
        /* not bothering to resize/rebalance if created oversized at init time*/
    }
  #endif

    return n->v;
}


struct ngdata {
  const char * restrict s;
  struct ngtable g; /* netgroup names */
  struct ngtable r; /* netgroup rules */
  /* array of int for string triples (0, +int) and subgroups (-int) */
  struct ngints { int *ptr; uint32_t used; uint32_t sz; } **ap;
  uint32_t apused;
  uint32_t apsz;
  char * restrict data;
  size_t datalen;
  size_t datasz;
  int errnum;
  int subg;
  uint32_t nuniq;
  int *uniq;
};


__attribute_nonnull__()
static void
netgroup_ngdata_uniq_reset (struct ngdata * const restrict ngd)
{
    memset(ngd->uniq+(ngd->nuniq<<6), 0, ngd->nuniq*sizeof(int));
}


__attribute_cold__
__attribute_noinline__
__attribute_nonnull__()
static bool
netgroup_ngdata_uniq_resize (struct ngdata * const restrict ngd)
{
    /* nrows must be non-zero power-of-2 */
    const int nold = ngd->nuniq;
    if (nold > ((UINT32_MAX-(nold<<2)*sizeof(int)) >> (6+2))/sizeof(int)) {
        errno = ERANGE;
        return false;
    }
    const int nuniq = (0 == nold) ? 64 : (nold << 2);

    /* nuniq rows of ncols(64) + array of nrows ints at end for used-per-row */
    int * const restrict uniq =
      (int *)malloc((nuniq<<6)*sizeof(int) + nuniq*sizeof(int));
    if (__builtin_expect( (NULL == uniq), 0)) return false;
    memset(uniq+(nuniq<<6),0,nuniq*sizeof(int));/*netgroup_ngdata_uniq_reset()*/

    if (ngd->uniq) { /* rebalance */
        const int * const restrict ousedp = ngd->uniq+(nold<<6);
        const int * restrict orow = ngd->uniq;
        for (int i = 0; i < nold; ++i, orow += (1<<6)) {
            for (int j = 0, oused = ousedp[i]; j < oused; ++j) {
                const int oid = orow[j];
                const int n = oid & (nuniq-1);
                int * const restrict usedp = uniq+(nuniq<<6)+n;
                int * const restrict row = uniq+(n<<6);
                row[(*usedp)++] = oid;
            }
        }
        free(ngd->uniq);
    }

    ngd->uniq = uniq;
    ngd->nuniq = nuniq;
    return true;
}


__attribute_nonnull__()
static void
netgroup_ngdata_free (struct ngdata * const restrict ngd)
{
    free(ngd->data);
    netgroup_ngtable_free(&ngd->g);
    netgroup_ngtable_free(&ngd->r);
    struct ngints ** const restrict ap = ngd->ap;
    for (uint32_t i = 0, used = ngd->apused; i < used; ++i) free(ap[i]);
    free(ap);
    free(ngd->uniq);
}


__attribute_nonnull__()
static bool
netgroup_ngdata_init (struct ngdata * const restrict ngd,
                      char * restrict p, size_t plen)
{
    size_t p2 = (1u << 5);
    while (p2 < plen && p2 < LONG_MAX) p2 <<= 1;
    plen = p2;
    memset(ngd, 0, sizeof(struct ngdata));
    ngd->s = p;
    ngd->ap = (struct ngints **)malloc((plen >> 5) * sizeof(struct ngints *));
    if (NULL == ngd->ap) return false;
    ngd->apsz = (plen >> 5);
    return netgroup_ngtable_init(&ngd->g, plen >> 5)
        && netgroup_ngtable_init(&ngd->r, plen >> 4);
}


struct ng_string_triple {
  const char *h;
  const char *u;
  const char *d;
  int hln;
  int uln;
  int dln;
};


__attribute_nonnull__()
static int
netgroup_string_triple_id (struct ngdata * const restrict ngd,
                           const struct ng_string_triple * const restrict s)
{
    /* data format: 4 unsigned char bytes, followed by '\0' terminated strings
     * of host, user, domain.  First two unsigned char bytes are total length
     * of strings, including 4 byte header.  Next is unsigned char byte with
     * len of host (not including '\0'), followed by unsigned char byte with
     * len of user (not including '\0').  In list of encoded triplets,
     * byte[0] == 0 and byte[1] == 0 ends list */

    if (s->hln > 255 || s->uln > 255 || s->dln > 255) return -1;
    const int sum = 4
                  + (s->hln ? s->hln+1 : 0)
                  + (s->uln ? s->uln+1 : 0)
                  + (s->dln ? s->dln+1 : 0);
    int off = 4;
    unsigned char r[4+768];
    r[0] = (unsigned char)((sum >> 8) & 0xFF);
    r[1] = (unsigned char)(sum & 0xFF);
    r[2] = (unsigned char)s->hln;
    r[3] = (unsigned char)s->uln;

    if (s->hln) { /* lowercase host for consistency and memcmp */
        const unsigned char * const restrict hun = (const unsigned char *)s->h;
        unsigned char * const restrict h = r+off;
        for (int i = 0; i < s->hln; ++i) h[i] = tolower(hun[i]);
        h[s->hln] = '\0';
        off += s->hln+1;
    }

    if (s->uln) { memcpy(r+off, s->u, s->uln); r[(off+=s->uln)] = '\0'; ++off; }

    if (s->dln) { /* lowercase domain for consistency and memcmp */
        const unsigned char * const restrict dun = (const unsigned char *)s->d;
        unsigned char * const restrict d = r+off;
        for (int i = 0; i < s->dln; ++i) d[i] = tolower(dun[i]);
        d[s->dln] = '\0';
        off += s->dln+1;
    }

    /*assert(off == sum)*/
    return (int)(intptr_t)netgroup_node_insert_id(&ngd->r, r, off);
}


__attribute_nonnull__()
static int
netgroup_id (struct ngdata * const restrict ngd,
             const char * const restrict s, const size_t slen)
{
    const int id = (int)(intptr_t)netgroup_node_insert_id(&ngd->g, s, slen);
    if (id < 0) return -1;

    if (ngd->g.a.used > INT_MAX) { errno = ERANGE; return -1; }
    if (id >= ngd->apused) {
        if (id != ngd->apused) return -1;
        if (ngd->apused == ngd->apsz) {
            if (ngd->apsz > (UINT32_MAX >> 1)) { errno = ERANGE; return -1; }
            struct ngints ** const restrict np = (struct ngints **)
              realloc(ngd->ap, (ngd->apsz <<= 1) * sizeof(struct ngints *));
            if (NULL == np) return -1;
            ngd->ap = np;
        }
        ++ngd->apused;
        ngd->ap[id] = (struct ngints *)calloc(1, sizeof(struct ngints));
        if (NULL == ngd->ap[id]) return -1;
    }

    return id;
}


__attribute_nonnull__()
static bool
netgroup_ngi_append (struct ngints * const restrict ngi, const int id)
{
    if (ngi->used == ngi->sz) {
        if (ngi->sz > (UINT32_MAX >> 1)) { errno = ERANGE; return -1; }
        if (0 == ngi->sz) ngi->sz = 2;
        int * const restrict np = (int *)
          realloc(ngi->ptr, (ngi->sz <<= 1) * sizeof(int));
        if (NULL == np) return false;
        ngi->ptr = np;
    }
    ngi->ptr[ngi->used++] = id;
    return true;
}


__attribute_nonnull__()
__attribute_pure__
static const char *
netgroup_advance_linear_ws_cont (const char * restrict s)
{
    while (*s == ' ' || *s == '\t' || *s == '\\') {
        if (s[0] != '\\')
            ++s;
        else if (s[1] == '\n')
            s+=2;
        else if (s[1] == '\r' && s[2] == '\n')
            s+=3;
        else
            break;
    }
    return s;
}


__attribute_nonnull__()
static bool
netgroup_parse_ngrule (struct ngdata * const restrict ngd,
                       struct ngints * const restrict ngi)
{
    const char * restrict s = ngd->s;
    uintptr_t n;
    int id;
    struct ng_string_triple st = { NULL, NULL, NULL, 0, 0, 0 };

    do {
        s = netgroup_advance_linear_ws_cont(s);
        if (*s == '\n' || *s == '\r' || *s == '\0') {
            ngd->s = s;
            return true;
        }
        if (*s == '\\') { errno = EINVAL; return false; }

        if (*s == '(') { /* netgroup string triple rule */
            ++s;
            s = netgroup_advance_linear_ws_cont(s);
            if (*s == '\0') { errno = EINVAL; return false; }
            if (*s == '\\') { errno = EINVAL; return false; }

            st.hln = 0;
            if (*s != ',') {
                if (!(isalnum(*s) || *s == '_')) { errno=EINVAL; return false; }
                st.h = s;
                do { ++s; } while (isalnum(*s)||*s=='.'||*s=='_'||*s=='-');
                n = (uintptr_t)(s - st.h);
                if (n > INT_MAX) { errno = ERANGE; return false; }
                st.hln = (int)n;
                s = netgroup_advance_linear_ws_cont(s);
                if (*s != ',') { errno = EINVAL; return false; }
            }
            ++s;
            s = netgroup_advance_linear_ws_cont(s);

            st.uln = 0;
            if (*s != ',') {
                if (!(isalnum(*s) || *s == '_')) { errno=EINVAL; return false; }
                st.u = s;
                do { ++s; } while (isalnum(*s)||*s=='.'||*s=='_'||*s=='-');
                n = (uintptr_t)(s - st.u);
                if (n > INT_MAX) { errno = ERANGE; return false; }
                st.uln = (int)n;
                s = netgroup_advance_linear_ws_cont(s);
                if (*s != ',') { errno = EINVAL; return false; }
            }
            ++s;
            s = netgroup_advance_linear_ws_cont(s);

            st.dln = 0;
            if (*s != ')') {
                if (!(isalnum(*s) || *s == '_')) { errno=EINVAL; return false; }
                st.d = s;
                do { ++s; } while (isalnum(*s)||*s=='.'||*s=='_'||*s=='-');
                n = (uintptr_t)(s - st.d);
                if (n > INT_MAX) { errno = ERANGE; return false; }
                st.dln = (int)n;
                s = netgroup_advance_linear_ws_cont(s);
                if (*s != ')') { errno = EINVAL; return false; }
            }
            ++s;

            id = netgroup_string_triple_id(ngd, &st);
            ngd->s = s;
            if (id < 0) return false;
        }
        else {             /* netgroup name (subgroup) */
            if (!(isalnum(*s) || *s == '_')) { errno = EINVAL; return false; }
            ngd->s = s;
            do { ++s; } while (isalnum(*s) || *s=='.' || *s=='_' || *s=='-');
            n = (uintptr_t)(s - ngd->s);
            if (n > INT_MAX) { errno = ERANGE; return false; }

            id = netgroup_id(ngd, ngd->s, n);
            ngd->s = s;
            if (id < 0) return false;
            id = -id;
        }
    } while (netgroup_ngi_append(ngi, id));
    return false;
}


__attribute_nonnull__()
static bool
netgroup_parse_ng (struct ngdata * const restrict ngd)
{
    const char * restrict s = ngd->s;
    uintptr_t n;
    int id;

    /* insert catch-all rule "(,,)" with id 0 (known value) (?for later use?) */
    struct ng_string_triple triple = { NULL, NULL, NULL, 0, 0, 0 };
    if (0 != netgroup_string_triple_id(ngd, &triple)) return false;

    /* insert bogus netgroup name "" with id 0 (known value) */
    /*(start netgroups at id = 1, not 0 due to -id encoding in struct ngints)*/
    if (0 != netgroup_id(ngd, "", 1)) return false;

    while (*s != 0) {
        /* skip blank lines and comment lines */
        do {
            if (*s == '\r') ++s;
            if (*s == '\n') ++s;
            if (*s == '#') {
                do {
                    while (*s != '\n' && *s != '\r' && *s != '\0') ++s;
                    if (*s == '\0' || *(s-1) != '\\') break;
                    if (*s == '\r') ++s;
                    if (*s == '\n') ++s;
                } while (*s != '\0');
            }
        } while (*s == '\n' || *s == '\r');
        if (*s == '\0') break;

        if (!(isalnum(*s) || *s == '_')) { errno = EINVAL; return false; }
        ngd->s = s;
        do { ++s; } while (isalnum(*s) || *s=='.' || *s=='_' || *s=='-');
        n = (uintptr_t)(s - ngd->s);
        if (n > INT_MAX) { errno = ERANGE; return false; }
        if (*s == ' ' || *s == '\t')
            ++s;
        else if (*s != '\\') {
            while (*s != '\n' && *s != '\r' && *s != '\0') ++s; /*(skip line)*/
            continue;
        }

        id = netgroup_id(ngd, ngd->s, n);
        ngd->s = s;
        if (id < 0) return false;
        struct ngints * const restrict ngi = ngd->ap[id];
        if (!netgroup_parse_ngrule(ngd, ngi)) return false;
        s = ngd->s;
    }

    ngd->s = s;

    return true;
}


__attribute_cold__
__attribute_noinline__
__attribute_nonnull__()
static bool
netgroup_datastr_resize (struct ngdata * const restrict ngd, size_t n)
{
    /* resize in 256k blocks */
    if (n > 1024) { errno = ERANGE; return false; } /*(not expected)*/
    if (ngd->datasz > ULONG_MAX - 262144) { errno = ERANGE; return false; }
    char * const ndata = realloc(ngd->data, (ngd->datasz += 262144));
    if (NULL == ndata) return false;
    ngd->data = ndata;
    return true;
}


__attribute_nonnull__()
static bool
netgroup_datastr_extend (struct ngdata * const restrict ngd, size_t n)
{
    return (__builtin_expect( (ngd->datasz - ngd->datalen >= n), 1))
      ? true
      : netgroup_datastr_resize(ngd, n);
}


__attribute_nonnull__()
static bool
netgroup_datastr_append (struct ngdata * const restrict ngd, int id)
{
    const struct ngnode * const restrict n = ngd->r.a.nodes[id];
    if (__builtin_expect( !netgroup_datastr_extend(ngd, n->klen), 0))
        return false;
    memcpy(ngd->data + ngd->datalen, n->k, n->klen);
    ngd->datalen += n->klen;
    return true;
}


__attribute_noinline__
__attribute_nonnull__()
static bool
netgroup_datastr_uniq_id (struct ngdata * const restrict ngd, const int id)
{
    /* Notes:
     *
     * O(n^2) search list of ids (worst case)
     * simple and most netgroup memberships are short, but this can
     * be quite expensive for many netgroups with large memberships
     *
     * Faster lookups might use a bitfield of all ids, but for a large number
     * of ids, it would get expensive to clear the set for each netgroup,
     * since most netgroups are expected to have a small number of members
     *
     * Might also take an adaptive approach.  Might scan ngints and if there
     * are no subgroups, take fast path and skip duplicate detection.  If there
     * are subgroups, keep a linear list until, say 64 is reached, and then
     * switch to a hash table or tree or ...
     *
     * Compromise: skip duplicate detect when no subgroups (done elsewhere),
     * then use power-of-2-sized table with buckets of int arrays in order to
     * reduce n by some orders of magnitude in (O)n^2 searches for dup ids.
     * Array of ints is used instead of linked list chasing pointers through
     * linked list is slower than scanning an array of ints.
     */
    /* see netgroup_ngdata_uniq_resize() for 2-D data structure of ints */
    int n = id & (ngd->nuniq-1);
    int * restrict usedp = ngd->uniq+(ngd->nuniq<<6)+n;
    int * restrict row = ngd->uniq+(n<<6);
    for (int i = 0, used = *usedp; i < used; ++i) {
        if (row[i] == id) return false;
    }

    while (__builtin_expect( (*usedp == (1<<6)), 0)) {
        /*(edge case might require resize more than once)*/
        if (__builtin_expect( !netgroup_ngdata_uniq_resize(ngd), 0))
            return false;
        n = id & (ngd->nuniq-1);
        usedp = ngd->uniq+(ngd->nuniq<<6)+n;
        row = ngd->uniq+(n<<6);
    }

    row[(*usedp)++] = id;
    return true;
}


__attribute_noinline__
__attribute_nonnull__()
static bool
netgroup_datastr_recurse (struct ngdata * const restrict ngd, int id)
{
    /*(might run out of stack space if netgroup subgroup recursion too deep)*/
    /*(future: might store/increment recursion depth in ngd and set a limit)*/
    const struct ngints * const restrict ngi = ngd->ap[id];
    const int no_subg = !ngd->subg;
    if (__builtin_expect( (ngi->used > INT_MAX), 0)){errno=ERANGE;return false;}
    for (int i = 0, used = (int)ngi->used; i < used; ++i) {
        id = ngi->ptr[i];
        if ((no_subg || netgroup_datastr_uniq_id(ngd, id))
            && (id >= 0
                ? __builtin_expect( !netgroup_datastr_append(ngd, id), 0)
                : __builtin_expect( !netgroup_datastr_recurse(ngd, -id), 0))) {
            return false;
        }
    }
    return true;
}


/* (different signature from other nss_mcdb_netdb_make_*_datastr() funcs) */
__attribute_nonnull__()
static bool
netgroup_datastr (struct ngdata * const restrict ngd, int id)
{
    ngd->datalen = 0;
    if (ngd->subg) { ngd->subg = 0; netgroup_ngdata_uniq_reset(ngd); }

    /* scan for subgroups and, if found, enable checks for dups and loops */
    const struct ngints * const restrict ngi = ngd->ap[id];
    const int used = (int)ngi->used;
    int i = 0;
    for (const int * const restrict p = ngi->ptr; i < used && p[i] >= 0; ++i);
    if (i != used) {
        ngd->subg = true;
        if (__builtin_expect( (0 == ngd->nuniq), 0)
            && __builtin_expect( !netgroup_ngdata_uniq_resize(ngd), 0))
            return false;
        /*(add netgroup to list as -id)*/
        if (__builtin_expect( !netgroup_datastr_uniq_id(ngd, -id), 0))
            return false;
    }

    return __builtin_expect( netgroup_datastr_recurse(ngd, id), 1)
        && __builtin_expect( (0 == ngd->errnum), 1);
}


bool
nss_mcdb_netdb_make_netgrent_encode(
  struct nss_mcdb_make_winfo * const restrict w,
  const void * const entp)
{
    struct ngdata * const restrict ngd = (struct ngdata * restrict)w->extra;
    (void)entp; /*(unused)*/
    if (__builtin_expect( (ngd->apused>INT_MAX), 0)){errno=ERANGE;return false;}
    /*(start netgroups at id = 1, not 0 due to -id encoding in struct ngints)*/
    for (int id = 1, used = (int)ngd->apused; id < used; ++id) {
        const struct ngnode * const restrict n = ngd->g.a.nodes[id];
        w->klen = n->klen;
        w->key  = n->k;

        if (__builtin_expect( !netgroup_datastr(ngd, id), 0))
            return false;
        if (0 == ngd->datalen) continue;

        /* end rules for current netgroup with two bytes of 0 (0-len hdr+rule)*/
        if (__builtin_expect( !netgroup_datastr_extend(ngd,2), 0)) return false;
        memcpy(ngd->data + ngd->datalen, "\0\0", 2);
        ngd->datalen += 2;

        w->dlen = ngd->datalen;
        w->data = ngd->data;

        w->tagc = '=';
        if (__builtin_expect( !nss_mcdb_make_mcdbctl_write(w), 0))
            return false;
    }

    return true;
}


bool
nss_mcdb_netdb_make_netgroup_parse(
  struct nss_mcdb_make_winfo * const restrict w, char * restrict p, size_t plen)
{
    struct ngdata ngd;
    bool rc = false;
    if (netgroup_ngdata_init(&ngd, p, plen) && netgroup_parse_ng(&ngd)) {
        /* netgroup data per netgroup might be very large;
         * use separate w->data buffer; save and restore existing buffer in w */
        char * restrict const data = w->data;
        w->data   = NULL;
        w->extra  = &ngd;
        rc = w->encode(w, NULL); /* nss_mcdb_netdb_make_netgrent_encode() */
        w->extra  = NULL;
        w->data   = data;
    }
    netgroup_ngdata_free(&ngd);
    return rc;
}
