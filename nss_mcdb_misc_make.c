/*
 * nss_mcdb_misc_make - mcdb of aliases, ethers, publickey, secretkey
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

#include "nss_mcdb_misc_make.h"
#include "nss_mcdb_misc.h"

#include <errno.h>
#include <string.h>

/*#include <aliases.h>*/       /* not portable; see nss_mcdb_misc.h */
/*#include <netinet/ether.h>*/ /* not portable; see nss_mcdb_misc.h */

#include <netinet/in.h> /* htonl(), htons() */
#include <arpa/inet.h>  /* htonl(), htons() */

size_t
nss_mcdb_misc_make_aliasent_datastr(char * restrict buf, const size_t bufsz,
				    const struct aliasent * const restrict ae)
{
    const size_t    ae_name_len       = 1 + strlen(ae->alias_name);
    const uintptr_t ae_mem_str_offset = ae_name_len;
    char ** restrict ae_mem           = ae->alias_members;
    size_t len;
    size_t ae_mem_num = ae->alias_members_len;
    size_t dlen = NSS_AE_HDRSZ + ae_mem_str_offset;
    union { uint32_t u[NSS_AE_HDRSZ>>2]; uint16_t h[NSS_AE_HDRSZ>>1]; } hdr;
    if (   __builtin_expect(ae_name_len <= USHRT_MAX, 1)
	&& __builtin_expect(dlen        <  bufsz,     1)) {
	while (ae_mem_num--
	       && (__builtin_expect((len=1+strlen(*ae_mem))<=bufsz-dlen, 1))){
	    memcpy(buf+dlen, *ae_mem, len);
	    dlen += len;
	    ++ae_mem;
	}
	if (ae_mem_num != (size_t)-1) {
	    errno = ERANGE;
	    return 0;
	}
	ae_mem_num = ae->alias_members_len;
	if (   __builtin_expect(ae_mem_num <= USHRT_MAX, 1)
	    && __builtin_expect((ae_mem_num<<3)+8u+7u <= bufsz-dlen, 1)) {
	    /* verify space in string for 8-aligned char ** ae_mem array + NULL
	     * (not strictly necessary, but best to catch excessively long
	     *  entries at mcdb create time rather than in query at runtime) */

	    /* store string offsets into aligned header, then copy into buf */
	    /* copy strings into buffer, including string terminating '\0' */
	    hdr.u[NSS_AE_LOCAL>>2]    = htonl((uint32_t) ae->alias_local);
	    hdr.h[NSS_AE_MEM>>1]      = htons((uint16_t) dlen - NSS_AE_HDRSZ);
	    hdr.h[NSS_AE_MEM_STR>>1]  = htons((uint16_t) ae_mem_str_offset);
	    hdr.h[NSS_AE_MEM_NUM>>1]  = htons((uint16_t) ae_mem_num);
	    hdr.h[(NSS_AE_HDRSZ>>1)-1]= 0;/*(clear last 2 bytes (consistency))*/
	    memcpy(buf,              hdr.u,          NSS_AE_HDRSZ);
	    memcpy(buf+NSS_AE_HDRSZ, ae->alias_name, ae_name_len);
	    return dlen - 1; /* subtract final '\0' */
	}
    }

    errno = ERANGE;
    return 0;
}


size_t
nss_mcdb_misc_make_ether_addr_datastr(char * restrict buf, const size_t bufsz,
				      const void * const restrict entp,
				      const char * const restrict hostname)
{   /* (take void *entp arg to avoid need to set _BSD_SOURCE in header) */
    const struct ether_addr * const restrict ea = entp;
    const size_t ea_name_len = 1 + strlen(hostname);
    if (__builtin_expect(NSS_EA_HDRSZ + ea_name_len <= bufsz, 1)) {
	/* (48-bit ether_addr == 6 bytes == NSS_EA_HDRSZ) */
	/* copy strings into buffer, including string terminating '\0' */
	memcpy(buf, &ea->ether_addr_octet[0], 6);
	memcpy(buf+NSS_EA_HDRSZ, hostname, ea_name_len);
	return NSS_EA_HDRSZ + ea_name_len - 1; /* subtract final '\0' */
    }

    errno = ERANGE;
    return 0;
}



bool
nss_mcdb_misc_make_aliasent_encode(
  struct nss_mcdb_make_winfo * const restrict w,
  const void * const restrict entp)
{
    const struct aliasent * const restrict ae = entp;
    w->dlen = nss_mcdb_misc_make_aliasent_datastr(w->data, w->datasz, ae);
    if (__builtin_expect( w->dlen == 0, 0))
        return false;

    w->tagc = '=';
    w->klen = strlen(ae->alias_name);
    w->key  = ae->alias_name;
    if (__builtin_expect( !nss_mcdb_make_mcdbctl_write(w), 0))
        return false;

    return true;
}


bool
nss_mcdb_misc_make_ether_addr_encode(
  struct nss_mcdb_make_winfo * const restrict w,
  const void * const restrict entp)
{
    const struct ether_addr * const restrict ea = entp;
    w->dlen = nss_mcdb_misc_make_ether_addr_datastr(w->data, w->datasz, entp,
                                                    w->extra);
    if (__builtin_expect( w->dlen == 0, 0))
        return false;

    w->tagc = '=';
    w->klen = strlen((const char *)w->extra);
    w->key  = (const char *)w->extra;
    if (__builtin_expect( !nss_mcdb_make_mcdbctl_write(w), 0))
        return false;

    w->tagc = 'b';  /* binary */
    w->klen = sizeof(struct ether_addr);
    w->key  = (char *)ea;
    if (__builtin_expect( !nss_mcdb_make_mcdbctl_write(w), 0))
        return false;

    return true;
}



bool
nss_mcdb_misc_make_ethers_parse(
  struct nss_mcdb_make_winfo * const restrict w, char * restrict p)
{
    char *b;
    struct ether_addr ea;
    char hostname[w->datasz + 1];       /* (>= _SC_HOST_NAME_MAX) */

    for (; *p; ++p) {

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
        if (*p == '\0')                 /* error: no newline; truncated? */
            return false;
        *p = '\0';

        if ((p - b) >= sizeof(hostname))/* line too long */
            return false;
        if (ether_line(b, &ea, hostname) != 0)
            return false;
        w->extra = hostname;

        /* find newline (to prep for beginning of next line) */
        if (*p != '\n') {
            TOKEN_WSDELIM_BEGIN(p);
            if (*p == '#') {
                do { ++p; } while (*p != '\n' && *p != '\0');
            }
            if (*p != '\n')             /* error: no newline; truncated? */
                return false;
        }

        /* process struct ether_addr */
        if (!w->encode(w, &ea))
            return false;
    }

    return true;
}


bool
nss_mcdb_misc_make_aliases_parse(
  struct nss_mcdb_make_winfo * const restrict w, char * restrict p)
{
    char *b;
    struct aliasent ae;
    size_t n;
    int c;
    char *alias_members[256];
    /* (255 aliases amounts to 1 KB of (256) 3-char names) */

    ae.alias_members = alias_members;

    for (; *p; ++p) {

        /* alias_name */
        TOKEN_WSDELIM_BEGIN(p);
        b = p;
        TOKEN_COLONHASHDELIM_END(p);    /* colon-delimited, not whitespace */
        if (*b == '\n')                 /* blank line; continue */
            continue;
        if (*b == '#') {
            do { ++p; } while (*p != '\n' && *p != '\0');
            if (*p == '\n')
                continue;               /* blank line; continue */
            else
                return false;           /* error: no newline; truncated? */
        }
        /* note: not supporting colon on continuation line */
        if (*b == ':' || *p != ':')     /* error: invalid line */
            return false;
        ae.alias_name = b;
        ae.alias_local= true;           /* local database; not e.g. ldap */
        for (b = p; *(b-1) == ' ' || *(b-1) == '\t'; --b)
            ;
        *b = '\0';                      /* remove trailing spaces */
        for (b = ae.alias_name+1; *b != ' ' && *b != '\t' && *b != '\0'; ++b)
            ;
        if (*b != '\0')                 /* error: invalid alias (whitespace) */
            return false;

        /* alias_members */
        /* note: not supporting comma on continuation line after alias */
        /* note: not supporting comma on continuation line after # comment */
        /* note: not supporting comma or pound/hash inside quoted string */
        /* note: not supporting backslash escaping of backslash before newline*/
        c = ',';
        n = 0;
        do {
            ++p;
            TOKEN_WSDELIM_BEGIN(p);
            b = p;
            if (*b == '\\' && *(b+1) == '\n') {    /* continuation line */
                p += 2;
                continue;
            }
            do {
                TOKEN_COMMAHASHDELIM_END(p);/* comma-delimited, not whitespace*/
                if ((c = *p) == '\0')   /* error: invalid line */
                    return false;
                if (c == '\n' && *(p-1) == '\\') { /* continuation line */
                    c = *(p-1) = *p = ' ';
                    ++p;
                }
            } while (c == ' ');
            if (*b == '#' || *b == '\n')/* done; no more aliases*/
                break;
            if (n < (sizeof(alias_members)/sizeof(char *))) {
                ae.alias_members[n++] = b;
                for (b = p; *(b-1) == ' ' || *(b-1) == '\t'; --b)
                    ;
                *b = '\0';              /* remove trailing spaces */
            }
            else
                return false; /* too many aliases to fit in fixed-sized array */
        } while (c != '#' && c != '\n');
        ae.alias_members_len = n;
        if (n == 0)                     /* 1+ aliases required for each name */
            return false;

        /* find newline (to prep for beginning of next line) */
        if (c != '\n') {
            TOKEN_WSDELIM_BEGIN(p);
            if (*p == '#') {
                do { ++p; } while (*p != '\n' && *p != '\0');
            }
            if (*p != '\n')   /* error: no newline; truncated? */
                return false;
        }

        /* process struct aliasent */
        if (!w->encode(w, &ae))
            return false;
    }

    return true;
}
