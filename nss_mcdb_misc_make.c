/* _BSD_SOURCE or _SVID_SOURCE for struct ether_addr on Linux */
#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif

#include "nss_mcdb_misc_make.h"
#include "nss_mcdb_misc.h"

#include "uint32.h"
#include "code_attributes.h"

#include <errno.h>
#include <string.h>
#include <aliases.h>
#include <netinet/ether.h>

size_t
nss_mcdb_misc_make_aliasent_datastr(char * restrict buf, const size_t bufsz,
				    const struct aliasent * const restrict ae)
{
    const size_t    ae_name_len     = 1 + strlen(ae->alias_name);
    const uintptr_t ae_name_offset  = 0;
    const uintptr_t ae_mem_str_offt = ae_name_offset + ae_name_len;
    char ** restrict ae_mem         = ae->alias_members;
    size_t len;
    size_t ae_mem_num = ae->alias_members_len;
    size_t offset = NSS_AE_HDRSZ + ae_mem_str_offt;
    if (   __builtin_expect(ae_name_len <= USHRT_MAX, 1)
	&& __builtin_expect(offset      <  bufsz,     1)) {
	while (ae_mem_num--
	       && (__builtin_expect((len=1+strlen(*ae_mem))<=bufsz-offset, 1))){
	    memcpy(buf+offset, *ae_mem, len);
	    offset += len;
	    ++ae_mem;
	}
	if (ae_mem_num != (size_t)-1) {
	    errno = ERANGE;
	    return 0;
	}
	ae_mem_num = ae->alias_members_len;
	if (   __builtin_expect(ae_mem_num <= USHRT_MAX, 1)
	    && __builtin_expect((ae_mem_num<<3)+8u+7u <= bufsz-offset, 1)) {
	    /* verify space in string for 8-aligned char ** ae_mem array + NULL
	     * (not strictly necessary, but best to catch excessively long
	     *  entries at cdb create time rather than in query at runtime) */

	    uint32_to_ascii8uphex((uint32_t)ae->alias_local,buf+NSS_AE_LOCAL);
	    /* store string offsets into buffer */
	    offset -= NSS_AE_HDRSZ;
	    uint16_to_ascii4uphex((uint32_t)offset,         buf+NSS_AE_MEM);
	    uint16_to_ascii4uphex((uint32_t)ae_mem_str_offt,buf+NSS_AE_MEM_STR);
	    uint16_to_ascii4uphex((uint32_t)ae_mem_num,     buf+NSS_AE_MEM_NUM);
	    /* copy strings into buffer, including string terminating '\0' */
	    buf += NSS_AE_HDRSZ;
	    memcpy(buf+ae_name_offset, ae->alias_name, ae_name_len);
	    return NSS_AE_HDRSZ + offset - 1; /* subtract final '\0' */
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
    uint32_t u[2];
    if (__builtin_expect(NSS_EA_HDRSZ + ea_name_len <= bufsz, 1)) {

	/* (12 hex chars, each encoding (1) 4-bit nibble == 48-bit ether_addr)*/
	/* (copy for alignment) */
	memcpy(u, &ea->ether_addr_octet[0], 4);
	u[1] = (ea->ether_addr_octet[4]<<8) | ea->ether_addr_octet[5];
	uint32_to_ascii8uphex(u[0], buf);
	uint16_to_ascii4uphex(u[1], buf+8);

	/* copy strings into buffer, including string terminating '\0' */
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
