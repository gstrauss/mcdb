/* memccpy() on Linux requires _XOPEN_SOURCE or _BSD_SOURCE or _SVID_SOURCE */
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif
/* _BSD_SOURCE or _SVID_SOURCE for struct rpcent on Linux */
#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif

#include "nss_mcdb_netdb_make.h"
#include "nss_mcdb_netdb.h"

#include "uint32.h"
#include "code_attributes.h"

#include <errno.h>
#include <string.h>
#include <netdb.h>

/* buf size passed should be >= _SC_HOST_NAME_MAX + NSS_HE_HDRSZ (not checked)
 * and probably larger.  1K + NSS_HE_HDRSZ is probably reasonable */
size_t
cdb_he2str(char * restrict buf, const size_t bufsz,
	   const struct hostent * const restrict he)
  __attribute_nonnull__;
size_t
cdb_he2str(char * restrict buf, const size_t bufsz,
	   const struct hostent * const restrict he)
{
    const size_t    he_name_len     = strlen(he->h_name);
    const uintptr_t he_name_offset  = 0;
    const uintptr_t he_mem_str_offt = he_name_offset + he_name_len + 1;
    uintptr_t he_lst_str_offt;
    char ** const restrict he_mem = he->h_aliases;
    char ** const restrict he_lst = he->h_addr_list;
    const char * restrict str;
    size_t len;
    size_t he_mem_num  = 0;
    size_t he_lst_num = 0;
    size_t offset = NSS_HE_HDRSZ + he_mem_str_offt;
    if (   __builtin_expect(he_name_len <= USHRT_MAX, 1)
	&& __builtin_expect(offset      < bufsz,      1)) {
	while ((str = he_mem[he_mem_num]) != NULL
	       && __builtin_expect((len = strlen(str)) < bufsz-offset, 1)) {
	    if (__builtin_expect(memccpy(buf+offset, str, ' ', len) == NULL,1)){
		buf[(offset+=len)] = ' ';
		++offset;
		++he_mem_num;
	    }
	    else {/* bad hostent; space-separated data may not contain spaces */
		errno = EINVAL;
		return 0;
	    }
	}
	he_lst_str_offt = offset - NSS_HE_HDRSZ;
	while ((str = he_lst[he_lst_num]) != NULL
	       && __builtin_expect((len = strlen(str)) < bufsz-offset, 1)) {
	    if (__builtin_expect(memccpy(buf+offset, str, ' ', len) == NULL,1)){
		buf[(offset+=len)] = ' ';
		++offset;
		++he_lst_num;
	    }
	    else {/* bad hostent; space-separated data may not contain spaces */
		errno = EINVAL;
		return 0;
	    }
	}
	if (   __builtin_expect(he_mem_num <= USHRT_MAX, 1)
	    && __builtin_expect(he_lst_num <= USHRT_MAX, 1)
	    && __builtin_expect(he_mem[he_mem_num] == NULL,  1)
	    && __builtin_expect(he_lst[he_lst_num] == NULL,  1)
	    && __builtin_expect(((he_mem_num+he_lst_num)<<3)+8u+8u+7u
                                <= bufsz-offset, 1)) {
	    /* verify space in string for (2) 8-aligned char ** array + NULL
	     * (not strictly necessary, but best to catch excessively long
	     *  entries at cdb create time rather than in query at runtime) */

	    uint32_to_ascii8uphex((uint32_t)he->h_addrtype, buf+NSS_H_ADDRTYPE);
	    uint32_to_ascii8uphex((uint32_t)he->h_length,   buf+NSS_H_LENGTH);
	    /* store string offsets into buffer */
	    offset -= NSS_HE_HDRSZ;
	    uint16_to_ascii4uphex((uint32_t)offset,         buf+NSS_HE_MEM);
	    uint16_to_ascii4uphex((uint32_t)he_mem_str_offt,buf+NSS_HE_MEM_STR);
	    uint16_to_ascii4uphex((uint32_t)he_lst_str_offt,buf+NSS_HE_LST_STR);
	    uint16_to_ascii4uphex((uint32_t)he_mem_num,     buf+NSS_HE_MEM_NUM);
	    uint16_to_ascii4uphex((uint32_t)he_lst_num,     buf+NSS_HE_LST_NUM);
	    /* copy strings into buffer */
	    buf += NSS_HE_HDRSZ;
	    memcpy(buf+he_name_offset, he->h_name, he_name_len);
	    /* separate entries with ' ' for readability */
	    buf[he_mem_str_offt-1]  =' '; /*(between he_name and he_mem str)*/
	    buf[offset] = '\0';           /* end string section */
	    return NSS_HE_HDRSZ + offset;
	}
    }

    errno = ERANGE;
    return 0;
}


/* buf size passed should be >= MAXNETNAMELEN + NSS_NE_HDRSZ (not checked)
 * and probably larger.  1K + NSS_NE_HDRSZ is probably reasonable */
size_t
cdb_ne2str(char * restrict buf, const size_t bufsz,
	   const struct netent * const restrict ne)
  __attribute_nonnull__;
size_t
cdb_ne2str(char * restrict buf, const size_t bufsz,
	   const struct netent * const restrict ne)
{
    const size_t    ne_name_len     = strlen(ne->n_name);
    const uintptr_t ne_name_offset  = 0;
    const uintptr_t ne_mem_str_offt = ne_name_offset + ne_name_len + 1;
    char ** const restrict ne_mem = ne->n_aliases;
    const char * restrict str;
    size_t len;
    size_t ne_mem_num  = 0;
    size_t offset = NSS_NE_HDRSZ + ne_mem_str_offt;
    if (   __builtin_expect(ne_name_len <= USHRT_MAX, 1)
	&& __builtin_expect(offset      < bufsz,      1)) {
	while ((str = ne_mem[ne_mem_num]) != NULL
	       && __builtin_expect((len = strlen(str)) < bufsz-offset, 1)) {
	    if (__builtin_expect(memccpy(buf+offset, str, ' ', len) == NULL,1)){
		buf[(offset+=len)] = ' ';
		++offset;
		++ne_mem_num;
	    }
	    else {/* bad netent; space-separated data may not contain spaces */
		errno = EINVAL;
		return 0;
	    }
	}
	if (   __builtin_expect(ne_mem_num <= USHRT_MAX, 1)
	    && __builtin_expect(ne_mem[ne_mem_num] == NULL,  1)
	    && __builtin_expect((ne_mem_num<<3)+8u+7u <= bufsz-offset, 1)) {
	    /* verify space in string for 8-aligned char ** ne_mem array + NULL
	     * (not strictly necessary, but best to catch excessively long
	     *  entries at cdb create time rather than in query at runtime) */

	    uint32_to_ascii8uphex((uint32_t)ne->n_addrtype, buf+NSS_N_ADDRTYPE);
	    uint32_to_ascii8uphex((uint32_t)ne->n_net,      buf+NSS_N_NET);
	    /* store string offsets into buffer */
	    offset -= NSS_NE_HDRSZ;
	    uint16_to_ascii4uphex((uint32_t)offset,         buf+NSS_NE_MEM);
	    uint16_to_ascii4uphex((uint32_t)ne_mem_str_offt,buf+NSS_NE_MEM_STR);
	    uint16_to_ascii4uphex((uint32_t)ne_mem_num,     buf+NSS_NE_MEM_NUM);
	    /* copy strings into buffer */
	    buf += NSS_NE_HDRSZ;
	    memcpy(buf+ne_name_offset, ne->n_name, ne_name_len);
	    /* separate entries with ' ' for readability */
	    buf[ne_mem_str_offt-1]  =' '; /*(between ne_name and ne_mem str)*/
	    buf[offset] = '\0';           /* end string section */
	    return NSS_NE_HDRSZ + offset;
	}
    }

    errno = ERANGE;
    return 0;
}


/* buf size passed should be >= ??? + NSS_PE_HDRSZ (not checked)
 * and probably larger.  1K + NSS_PE_HDRSZ is probably reasonable */
size_t
cdb_pe2str(char * restrict buf, const size_t bufsz,
	   const struct protoent * const restrict pe)
  __attribute_nonnull__;
size_t
cdb_pe2str(char * restrict buf, const size_t bufsz,
	   const struct protoent * const restrict pe)
{
    const size_t    pe_name_len     = strlen(pe->p_name);
    const uintptr_t pe_name_offset  = 0;
    const uintptr_t pe_mem_str_offt = pe_name_offset + pe_name_len + 1;
    char ** const restrict pe_mem = pe->p_aliases;
    const char * restrict str;
    size_t len;
    size_t pe_mem_num  = 0;
    size_t offset = NSS_PE_HDRSZ + pe_mem_str_offt;
    if (   __builtin_expect(pe_name_len <= USHRT_MAX, 1)
	&& __builtin_expect(offset      < bufsz,      1)) {
	while ((str = pe_mem[pe_mem_num]) != NULL
	       && __builtin_expect((len = strlen(str)) < bufsz-offset, 1)) {
	    if (__builtin_expect(memccpy(buf+offset, str, ' ', len) == NULL,1)){
		buf[(offset+=len)] = ' ';
		++offset;
		++pe_mem_num;
	    }
	    else {/* bad protoent; space-separated data may not contain spaces*/
		errno = EINVAL;
		return 0;
	    }
	}
	if (   __builtin_expect(pe_mem_num <= USHRT_MAX, 1)
	    && __builtin_expect(pe_mem[pe_mem_num] == NULL,  1)
	    && __builtin_expect((pe_mem_num<<3)+8u+7u <= bufsz-offset, 1)) {
	    /* verify space in string for 8-aligned char ** pe_mem array + NULL
	     * (not strictly necessary, but best to catch excessively long
	     *  entries at cdb create time rather than in query at runtime) */

	    uint32_to_ascii8uphex((uint32_t)pe->p_proto,    buf+NSS_P_PROTO);
	    /* store string offsets into buffer */
	    offset -= NSS_PE_HDRSZ;
	    uint16_to_ascii4uphex((uint32_t)offset,         buf+NSS_PE_MEM);
	    uint16_to_ascii4uphex((uint32_t)pe_mem_str_offt,buf+NSS_PE_MEM_STR);
	    uint16_to_ascii4uphex((uint32_t)pe_mem_num,     buf+NSS_PE_MEM_NUM);
	    /* copy strings into buffer */
	    buf += NSS_PE_HDRSZ;
	    memcpy(buf+pe_name_offset, pe->p_name, pe_name_len);
	    /* separate entries with ' ' for readability */
	    buf[pe_mem_str_offt-1]  =' '; /*(between pe_name and pe_mem str)*/
	    buf[offset] = '\0';           /* end string section */
	    return NSS_PE_HDRSZ + offset;
	}
    }

    errno = ERANGE;
    return 0;
}


/* buf size passed should be >= ??? + NSS_RE_HDRSZ (not checked)
 * and probably larger.  1K + NSS_RE_HDRSZ is probably reasonable */
size_t
cdb_re2str(char * restrict buf, const size_t bufsz,
	   const struct rpcent * const restrict re)
  __attribute_nonnull__;
size_t
cdb_re2str(char * restrict buf, const size_t bufsz,
	   const struct rpcent * const restrict re)
{
    const size_t    re_name_len     = strlen(re->r_name);
    const uintptr_t re_name_offset  = 0;
    const uintptr_t re_mem_str_offt = re_name_offset + re_name_len + 1;
    char ** const restrict re_mem = re->r_aliases;
    const char * restrict str;
    size_t len;
    size_t re_mem_num  = 0;
    size_t offset = NSS_RE_HDRSZ + re_mem_str_offt;
    if (   __builtin_expect(re_name_len <= USHRT_MAX, 1)
	&& __builtin_expect(offset      < bufsz,      1)) {
	while ((str = re_mem[re_mem_num]) != NULL
	       && __builtin_expect((len = strlen(str)) < bufsz-offset, 1)) {
	    if (__builtin_expect(memccpy(buf+offset, str, ' ', len) == NULL,1)){
		buf[(offset+=len)] = ' ';
		++offset;
		++re_mem_num;
	    }
	    else {/* bad rpcent; space-separated data may not contain spaces */
		errno = EINVAL;
		return 0;
	    }
	}
	if (   __builtin_expect(re_mem_num <= USHRT_MAX, 1)
	    && __builtin_expect(re_mem[re_mem_num] == NULL,  1)
	    && __builtin_expect((re_mem_num<<3)+8u+7u <= bufsz-offset, 1)) {
	    /* verify space in string for 8-aligned char ** re_mem array + NULL
	     * (not strictly necessary, but best to catch excessively long
	     *  entries at cdb create time rather than in query at runtime) */

	    uint32_to_ascii8uphex((uint32_t)re->r_number,   buf+NSS_R_NUMBER);
	    /* store string offsets into buffer */
	    offset -= NSS_RE_HDRSZ;
	    uint16_to_ascii4uphex((uint32_t)offset,         buf+NSS_RE_MEM);
	    uint16_to_ascii4uphex((uint32_t)re_mem_str_offt,buf+NSS_RE_MEM_STR);
	    uint16_to_ascii4uphex((uint32_t)re_mem_num,     buf+NSS_RE_MEM_NUM);
	    /* copy strings into buffer */
	    buf += NSS_RE_HDRSZ;
	    memcpy(buf+re_name_offset, re->r_name, re_name_len);
	    /* separate entries with ' ' for readability */
	    buf[re_mem_str_offt-1]  =' '; /*(between re_name and re_mem str)*/
	    buf[offset] = '\0';           /* end string section */
	    return NSS_RE_HDRSZ + offset;
	}
    }

    errno = ERANGE;
    return 0;
}


/* buf size passed should be >= ??? + NSS_SE_HDRSZ (not checked)
 * and probably larger.  1K + NSS_SE_HDRSZ is probably reasonable */
size_t
cdb_se2str(char * restrict buf, const size_t bufsz,
	   const struct servent * const restrict se)
  __attribute_nonnull__;
size_t
cdb_se2str(char * restrict buf, const size_t bufsz,
	   const struct servent * const restrict se)
{
    const size_t    se_name_len     = strlen(se->s_name);
    const size_t    se_proto_len    = strlen(se->s_proto);
    const uintptr_t se_proto_offset = 0; /*(proto before name for query usage)*/
    const uintptr_t se_name_offset  = se_proto_offset + se_proto_len + 1;
    const uintptr_t se_mem_str_offt = se_name_offset  + se_name_len  + 1;
    char ** const restrict se_mem = se->s_aliases;
    const char * restrict str;
    size_t len;
    size_t se_mem_num  = 0;
    size_t offset = NSS_SE_HDRSZ + se_mem_str_offt;
    if (   __builtin_expect(se_name_len <= USHRT_MAX, 1)
	&& __builtin_expect(offset      < bufsz,      1)) {
	while ((str = se_mem[se_mem_num]) != NULL
	       && __builtin_expect((len = strlen(str)) < bufsz-offset, 1)) {
	    if (__builtin_expect(memccpy(buf+offset, str, ' ', len) == NULL,1)){
		buf[(offset+=len)] = ' ';
		++offset;
		++se_mem_num;
	    }
	    else {/* bad servent; space-separated data may not contain spaces */
		errno = EINVAL;
		return 0;
	    }
	}
	if (   __builtin_expect(se_mem_num <= USHRT_MAX, 1)
	    && __builtin_expect(se_mem[se_mem_num] == NULL,  1)
	    && __builtin_expect((se_mem_num<<3)+8u+7u <= bufsz-offset, 1)) {
	    /* verify space in string for 8-aligned char ** se_mem array + NULL
	     * (not strictly necessary, but best to catch excessively long
	     *  entries at cdb create time rather than in query at runtime) */

	    uint32_to_ascii8uphex((uint32_t)se->s_port,     buf+NSS_S_PORT);
	    /* store string offsets into buffer */
	    offset -= NSS_SE_HDRSZ;
	    uint16_to_ascii4uphex((uint32_t)offset,         buf+NSS_SE_MEM);
	    uint16_to_ascii4uphex((uint32_t)se_mem_str_offt,buf+NSS_SE_MEM_STR);
	    uint16_to_ascii4uphex((uint32_t)se_mem_num,     buf+NSS_SE_MEM_NUM);
	    /* copy strings into buffer */
	    buf += NSS_SE_HDRSZ;
	    memcpy(buf+se_name_offset,  se->s_name,  se_name_len);
	    memcpy(buf+se_proto_offset, se->s_proto, se_proto_len);
	    /* separate entries with ' ' for readability */
	    buf[se_proto_offset-1]  =' '; /*(between se_name and se_proto)*/
	    buf[se_mem_str_offt-1]  =' '; /*(between se_proto and se_mem str)*/
	    buf[offset] = '\0';           /* end string section */
	    return NSS_SE_HDRSZ + offset;
	}
    }

    errno = ERANGE;
    return 0;
}
