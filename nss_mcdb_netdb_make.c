/* memccpy() on Linux requires _XOPEN_SOURCE or _BSD_SOURCE or _SVID_SOURCE */
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif

#include "nss_mcdb_netdb_make.h"
#include "nss_mcdb_netdb.h"

#include "uint32.h"
#include "code_attributes.h"

#include <errno.h>
#include <string.h>
#include <netdb.h>

/* buf size passed should be >= _SC_HOST_NAME_MAX + IDX_HE_HDRSZ (not checked)
 * and probably larger.  1K + IDX_HE_HDRSZ is probably reasonable */
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
    size_t offset = IDX_HE_HDRSZ + he_mem_str_offt;
    if (   __builtin_expect(he_name_len <= USHRT_MAX, 1)
	&& __builtin_expect(offset      < bufsz,      1)) {
	while ((str = he_mem[he_mem_num]) != NULL
	       && __builtin_expect((len = strlen(str)) < bufsz-offset, 1)) {
	    if (memccpy(buf+offset, str, ' ', len) == NULL) {
		buf[(offset+=len)] = ' ';
		++offset;
		++he_mem_num;
	    }
	    else {/* bad hostent; space-separated data may not contain spaces */
		errno = EINVAL;
		return 0;
	    }
	}
	he_lst_str_offt = offset - IDX_HE_HDRSZ;
	while ((str = he_lst[he_lst_num]) != NULL
	       && __builtin_expect((len = strlen(str)) < bufsz-offset, 1)) {
	    if (memccpy(buf+offset, str, ' ', len) == NULL) {
		buf[(offset+=len)] = ' ';
		++offset;
		++he_lst_num;
	    }
	    else {/* bad hostent; space-separated data may not contain spaces */
		errno = EINVAL;
		return 0;
	    }
	}
	offset -= IDX_HE_HDRSZ;
	if (   __builtin_expect(IDX_HE_HDRSZ + offset < bufsz, 1)
	    && __builtin_expect(he_mem_num <= USHRT_MAX, 1)
	    && __builtin_expect(he_lst_num <= USHRT_MAX, 1)
	    && __builtin_expect(he_mem[he_mem_num] == NULL,  1)
	    && __builtin_expect(he_lst[he_lst_num] == NULL,  1)
	    && __builtin_expect(((he_mem_num+he_lst_num)<<3)+8u+8u+7u
                                <= bufsz-offset, 1)) {
	    /* verify space in string for (2) 8-aligned char ** array + NULL
	     * (not strictly necessary, but best to catch excessively long
	     *  entries at cdb create time rather than in query at runtime) */

	    uint32_to_ascii8uphex((uint32_t)he->h_addrtype, buf+IDX_H_ADDRTYPE);
	    uint32_to_ascii8uphex((uint32_t)he->h_length,   buf+IDX_H_LENGTH);
	    /* store string offsets into buffer */
	    uint16_to_ascii4uphex((uint32_t)he_mem_str_offt,buf+IDX_HE_MEM_STR);
	    uint16_to_ascii4uphex((uint32_t)he_lst_str_offt,buf+IDX_HE_LST_STR);
	    uint16_to_ascii4uphex((uint32_t)offset,         buf+IDX_HE_MEM);
	    uint16_to_ascii4uphex((uint32_t)he_mem_num,     buf+IDX_HE_MEM_NUM);
	    uint16_to_ascii4uphex((uint32_t)he_lst_num,     buf+IDX_HE_LST_NUM);
	    /* copy strings into buffer */
	    buf += IDX_HE_HDRSZ;
	    memcpy(buf+he_name_offset, he->h_name, he_name_len);
	    /* separate entries with ' ' for readability */
	    buf[he_mem_str_offt-1]  =' '; /*(between he_name and he_mem str)*/
	    buf[offset] = '\0';           /* end string section */
	    return IDX_HE_HDRSZ + offset;
	}
    }

    errno = ERANGE;
    return 0;
}


/* buf size passed should be >= MAXNETNAMELEN + IDX_NE_HDRSZ (not checked)
 * and probably larger.  1K + IDX_NE_HDRSZ is probably reasonable */
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
    size_t offset = IDX_NE_HDRSZ + ne_mem_str_offt;
    if (   __builtin_expect(ne_name_len <= USHRT_MAX, 1)
	&& __builtin_expect(offset      < bufsz,      1)) {
	while ((str = ne_mem[ne_mem_num]) != NULL
	       && __builtin_expect((len = strlen(str)) < bufsz-offset, 1)) {
	    if (memccpy(buf+offset, str, ' ', len) == NULL) {
		buf[(offset+=len)] = ' ';
		++offset;
		++ne_mem_num;
	    }
	    else {/* bad netent; space-separated data may not contain spaces */
		errno = EINVAL;
		return 0;
	    }
	}
	offset -= IDX_NE_HDRSZ;
	if (   __builtin_expect(IDX_NE_HDRSZ + offset < bufsz, 1)
	    && __builtin_expect(ne_mem_num <= USHRT_MAX, 1)
	    && __builtin_expect(ne_mem[ne_mem_num] == NULL,  1)
	    && __builtin_expect((ne_mem_num<<3)+8u+7u <= bufsz-offset, 1)) {
	    /* verify space in string for 8-aligned char ** ne_mem array + NULL
	     * (not strictly necessary, but best to catch excessively long
	     *  entries at cdb create time rather than in query at runtime) */

	    uint32_to_ascii8uphex((uint32_t)ne->n_addrtype, buf+IDX_N_ADDRTYPE);
	    uint32_to_ascii8uphex((uint32_t)ne->n_net,      buf+IDX_N_NET);
	    /* store string offsets into buffer */
	    uint16_to_ascii4uphex((uint32_t)ne_mem_str_offt,buf+IDX_NE_MEM_STR);
	    uint16_to_ascii4uphex((uint32_t)offset,         buf+IDX_NE_MEM);
	    uint16_to_ascii4uphex((uint32_t)ne_mem_num,     buf+IDX_NE_MEM_NUM);
	    /* copy strings into buffer */
	    buf += IDX_NE_HDRSZ;
	    memcpy(buf+ne_name_offset, ne->n_name, ne_name_len);
	    /* separate entries with ' ' for readability */
	    buf[ne_mem_str_offt-1]  =' '; /*(between ne_name and ne_mem str)*/
	    buf[offset] = '\0';           /* end string section */
	    return IDX_NE_HDRSZ + offset;
	}
    }

    errno = ERANGE;
    return 0;
}


