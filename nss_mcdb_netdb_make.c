/*
 * nss_mcdb_netdb_make - mcdb of hosts, protocols, networks, services, rpc
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

/* _BSD_SOURCE or _SVID_SOURCE for struct rpcent on Linux */
#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif

#include "nss_mcdb_netdb_make.h"
#include "nss_mcdb_netdb.h"
#include "nss_mcdb_make.h"

#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>     /* strtol() */
#include <netdb.h>
#include <arpa/inet.h>  /* inet_pton() ntohl() ntohs() htons() */

/* (similar to code nss_mcdb_acct_make.c:nss_mcdb_acct_make_group_datastr()) */
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
	return (size_t)-1;
    }
}

size_t
nss_mcdb_netdb_make_hostent_datastr(char * restrict buf, const size_t bufsz,
				    const struct hostent * const restrict he)
{
    const size_t he_name_len       = 1 + strlen(he->h_name);
    const size_t he_mem_str_offset = he_name_len;
    size_t he_lst_str_offset;
    char ** const restrict he_lst  = he->h_addr_list;
    const size_t len               = (size_t)he->h_length;
    size_t he_mem_num;
    size_t he_lst_num = 0;
    size_t dlen = NSS_HE_HDRSZ + he_mem_str_offset;
    union { uint32_t u[NSS_HE_HDRSZ>>2]; uint16_t h[NSS_HE_HDRSZ>>1]; } hdr;
    if (   __builtin_expect(he_name_len <= USHRT_MAX, 1)
	&& __builtin_expect(dlen        <  bufsz,     1)) {
	he_mem_num =nss_mcdb_netdb_make_list2str(buf,bufsz,he->h_aliases,&dlen);
	if (__builtin_expect( he_mem_num == (size_t)-1, 0))
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


size_t
nss_mcdb_netdb_make_netent_datastr(char * restrict buf, const size_t bufsz,
				   const struct netent * const restrict ne)
{
    const size_t    ne_name_len       = 1 + strlen(ne->n_name);
    const uintptr_t ne_mem_str_offset = ne_name_len;
    size_t ne_mem_num;
    size_t dlen = NSS_NE_HDRSZ + ne_mem_str_offset;
    union { uint32_t u[NSS_NE_HDRSZ>>2]; uint16_t h[NSS_NE_HDRSZ>>1]; } hdr;
    if (   __builtin_expect(ne_name_len <= USHRT_MAX, 1)
	&& __builtin_expect(dlen        <  bufsz,     1)) {
	ne_mem_num =nss_mcdb_netdb_make_list2str(buf,bufsz,ne->n_aliases,&dlen);
	if (__builtin_expect( ne_mem_num == (size_t)-1, 0))
	    return 0;
	if (   __builtin_expect(ne_mem_num <= USHRT_MAX, 1)
	    && __builtin_expect((ne_mem_num<<3)+8u+7u <= bufsz-dlen, 1)) {
	    /* verify space in string for 8-aligned char ** ne_mem array + NULL
	     * (not strictly necessary, but best to catch excessively long
	     *  entries at mcdb create time rather than in query at runtime) */

	    /* store string offsets into aligned header, then copy into buf */
	    /* copy strings into buffer, including string terminating '\0' */
	    hdr.u[NSS_N_ADDRTYPE>>2]  = htonl((uint32_t) ne->n_addrtype);
	    hdr.u[NSS_N_NET>>2]       = htonl((uint32_t) ne->n_net);
	    hdr.h[NSS_NE_MEM>>1]      = htons((uint16_t)(dlen - NSS_NE_HDRSZ));
	    hdr.h[NSS_NE_MEM_STR>>1]  = htons((uint16_t) ne_mem_str_offset);
	    hdr.h[NSS_NE_MEM_NUM>>1]  = htons((uint16_t) ne_mem_num);
	    hdr.h[(NSS_NE_HDRSZ>>1)-1]= 0;/*(clear last 2 bytes (consistency))*/
	    memcpy(buf,              hdr.u,      NSS_NE_HDRSZ);
	    memcpy(buf+NSS_NE_HDRSZ, ne->n_name, ne_name_len);
	    return dlen;
	}
    }

    errno = ERANGE;
    return 0;
}


size_t
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
	if (__builtin_expect( pe_mem_num == (size_t)-1, 0))
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


size_t
nss_mcdb_netdb_make_rpcent_datastr(char * restrict buf, const size_t bufsz,
				   const void * const restrict entp)
{   /* (take void *entp arg to avoid need to set _BSD_SOURCE in header) */
    const struct rpcent * const restrict re = entp;
    const size_t    re_name_len       = 1 + strlen(re->r_name);
    const uintptr_t re_mem_str_offset = re_name_len;
    size_t re_mem_num;
    size_t dlen = NSS_RE_HDRSZ + re_mem_str_offset;
    union { uint32_t u[NSS_RE_HDRSZ>>2]; uint16_t h[NSS_RE_HDRSZ>>1]; } hdr;
    if (   __builtin_expect(re_name_len <= USHRT_MAX, 1)
	&& __builtin_expect(dlen        <  bufsz,     1)) {
	re_mem_num =nss_mcdb_netdb_make_list2str(buf,bufsz,re->r_aliases,&dlen);
	if (__builtin_expect( re_mem_num == (size_t)-1, 0))
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


size_t
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
	if (__builtin_expect( se_mem_num == (size_t)-1, 0))
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
  const void * const restrict entp)
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
    w->klen = he->h_length;
    w->key  = he->h_addr_list[0];
    if (__builtin_expect( !nss_mcdb_make_mcdbctl_write(w), 0))
        return false;

    return true;
}


bool
nss_mcdb_netdb_make_netent_encode(
  struct nss_mcdb_make_winfo * const restrict w,
  const void * const restrict entp)
{
    const struct netent * const restrict ne = entp;
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
    n[0] = htonl((uint32_t) ne->n_net);
    n[1] = htonl((uint32_t) ne->n_addrtype);
    if (__builtin_expect( !nss_mcdb_make_mcdbctl_write(w), 0))
        return false;

    return true;
}


bool
nss_mcdb_netdb_make_protoent_encode(
  struct nss_mcdb_make_winfo * const restrict w,
  const void * const restrict entp)
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
  const void * const restrict entp)
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
  const void * const restrict entp)
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
  struct nss_mcdb_make_winfo * const restrict w, char * restrict p)
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
            if (n < (sizeof(h_aliases)/sizeof(char *) - 1))
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
  struct nss_mcdb_make_winfo * const restrict w, char * restrict p)
{
    char *b;
    int c;
    int n;
    struct netent ne;
    struct in_addr in_addr;
    char *n_aliases[256];
    /*(255 aliases + canonical name amounts to 1 KB of (256) 3-char names)*/

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

        /* n_net */
        ++p;
        TOKEN_WSDELIM_BEGIN(p);
        b = p;
        TOKEN_WSDELIM_END(p);
        if ((c = *p) == '\0' || *b == '#' || *b == '\n')/* error: invalid line*/
            return false;
        *p = '\0';
        if (inet_pton(AF_INET, b, &in_addr) > 0) {
            ne.n_net = ntohl(*(uint32_t *)&in_addr);
            ne.n_addrtype = AF_INET;
        }
        else                            /* error: invalid or unsupported addr */
            return false;

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
            if (n < (sizeof(n_aliases)/sizeof(char *) - 1))
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
  struct nss_mcdb_make_winfo * const restrict w, char * restrict p)
{
    char *b, *e;
    int c;
    long n;
    struct protoent pe;
    char *p_aliases[256];
    /*(255 aliases + canonical name amounts to 1 KB of (256) 3-char names)*/

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
            if (n < (sizeof(p_aliases)/sizeof(char *) - 1))
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
  struct nss_mcdb_make_winfo * const restrict w, char * restrict p)
{
    char *b, *e;
    int c;
    long n;
    struct rpcent re;
    char *r_aliases[256];
    /*(255 aliases + canonical name amounts to 1 KB of (256) 3-char names)*/

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
            if (n < (sizeof(r_aliases)/sizeof(char *) - 1))
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
  struct nss_mcdb_make_winfo * const restrict w, char * restrict p)
{
    char *b, *e;
    int c;
    long n;
    struct servent se;
    char *s_aliases[256];
    /*(255 aliases + canonical name amounts to 1 KB of (256) 3-char names)*/

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
            if (n < (sizeof(s_aliases)/sizeof(char *) - 1))
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
