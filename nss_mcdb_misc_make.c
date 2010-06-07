/* memccpy() on Linux requires _XOPEN_SOURCE or _BSD_SOURCE or _SVID_SOURCE */
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif
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

/* buf size passed should be >= ??? + NSS_AE_HDRSZ (not checked)
 * and probably larger.  1K + NSS_AE_HDRSZ is probably reasonable */
size_t
cdb_ae2str(char * restrict buf, const size_t bufsz,
	   const struct aliasent * const restrict ae)
  __attribute_nonnull__;
size_t
cdb_ae2str(char * restrict buf, const size_t bufsz,
	   const struct aliasent * const restrict ae)
{
    const size_t    ae_name_len     = strlen(ae->alias_name);
    const uintptr_t ae_name_offset  = 0;
    const uintptr_t ae_mem_str_offt = ae_name_offset + ae_name_len + 1;
    char ** restrict ae_mem         = ae->alias_members;
    size_t len;
    size_t ae_mem_num = ae->alias_members_len;
    size_t offset = NSS_AE_HDRSZ + ae_mem_str_offt;
    if (   __builtin_expect(ae_name_len <= USHRT_MAX, 1)
	&& __builtin_expect(offset      < bufsz,      1)) {
	while (ae_mem_num--
	       && __builtin_expect((len = strlen(*ae_mem)) < bufsz-offset, 1)) {
	    if (__builtin_expect(memccpy(buf+offset,*ae_mem,',',len)==NULL,1)) {
		buf[(offset+=len)] = ',';
		++offset;
		++ae_mem;
	    }
	    else {/* bad aliasent; comma-separated data may not contain commas*/
		errno = EINVAL;
		return 0;
	    }
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
	    /* copy strings into buffer */
	    buf += NSS_AE_HDRSZ;
	    memcpy(buf+ae_name_offset, ae->alias_name, ae_name_len);
	    /* separate entries with ' ' for readability */
	    buf[ae_mem_str_offt-1]  =' '; /*(between ae_name and ae_mem str)*/
	    buf[offset] = '\0';           /* end string section */
	    return NSS_AE_HDRSZ + offset;
	}
    }

    errno = ERANGE;
    return 0;
}


/* buf size passed should be >= ??? + NSS_EA_HDRSZ (not checked)
 * and probably larger.  1K + NSS_EA_HDRSZ is probably reasonable */
size_t
cdb_ea2str(char * restrict buf, const size_t bufsz,
	   const struct ether_addr * const restrict ea,
	   const char * const restrict hostname)
  __attribute_nonnull__;
size_t
cdb_ea2str(char * restrict buf, const size_t bufsz,
	   const struct ether_addr * const restrict ea,
	   const char * const restrict hostname)
{
    const size_t ea_name_len = strlen(hostname);
    uint32_t u[2];
    if (__builtin_expect(NSS_EA_HDRSZ + ea_name_len < bufsz, 1)) {

	/* (12 hex chars, each encoding (1) 4-bit nibble == 48-bit ether_addr)*/
	/* (copy for alignment) */
	memcpy(u, &ea->ether_addr_octet[0], 4);
	u[1] = (ea->ether_addr_octet[4]<<8) | ea->ether_addr_octet[5];
	uint32_to_ascii8uphex(u[0], buf);
	uint16_to_ascii4uphex(u[1], buf+8);

	/* copy strings into buffer */
	memcpy(buf+NSS_EA_HDRSZ,hostname,ea_name_len+1); /*+1 to term last str*/
	return NSS_EA_HDRSZ + ea_name_len;
    }

    errno = ERANGE;
    return 0;
}
