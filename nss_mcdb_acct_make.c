#include "nss_mcdb_acct_make.h"
#include "nss_mcdb_acct.h"
#include "nss_mcdb.h"
#include "mcdb.h"
#include "uint32.h"
#include "code_attributes.h"

#include <errno.h>
#include <string.h>

#include <pwd.h>
#include <grp.h>
#include <shadow.h>

/* TODO: ... this might end up being a Perl script ... */

/* FIXME: instead of getpwent() and getgrent(),
 * will need to parse /etc/passwd and /etc/group */

size_t
cdb_pw2str(char * restrict buf, const size_t bufsz,
	   struct passwd * const restrict pw)
  __attribute_nonnull__;
size_t
cdb_pw2str(char * restrict buf, const size_t bufsz,
	   struct passwd * const restrict pw)
{
    const size_t pw_name_len   = strlen(pw->pw_name);
    const size_t pw_passwd_len = strlen(pw->pw_passwd);
    const size_t pw_gecos_len  = strlen(pw->pw_gecos);
    const size_t pw_dir_len    = strlen(pw->pw_dir);
    const size_t pw_shell_len  = strlen(pw->pw_shell);
    const uintptr_t pw_name_offset   = 0;
    const uintptr_t pw_passwd_offset = pw_name_offset   + pw_name_len   + 1;
    const uintptr_t pw_gecos_offset  = pw_passwd_offset + pw_passwd_len + 1;
    const uintptr_t pw_dir_offset    = pw_gecos_offset  + pw_gecos_len  + 1;
    const uintptr_t pw_shell_offset  = pw_dir_offset    + pw_dir_len    + 1;
    const uintptr_t pw_shell_end     = pw_shell_offset  + pw_shell_len;
    /*(pw_shell_end < bufsz to leave +1 in buffer for final '\n' or '\0')*/
    if (   __builtin_expect(pw_shell_end     <  bufsz,     1)
	&& __builtin_expect(pw_passwd_offset <= USHRT_MAX, 1)
	&& __builtin_expect(pw_gecos_offset  <= USHRT_MAX, 1)
	&& __builtin_expect(pw_dir_offset    <= USHRT_MAX, 1)
	&& __builtin_expect(pw_shell_offset  <= USHRT_MAX, 1)) { 
	/* store string offsets into buffer */
	uint16_to_ascii4uphex((uint32_t)pw_passwd_offset, buf+IDX_PW_PASSWD);
	uint16_to_ascii4uphex((uint32_t)pw_gecos_offset,  buf+IDX_PW_GECOS);
	uint16_to_ascii4uphex((uint32_t)pw_dir_offset,    buf+IDX_PW_DIR);
	uint16_to_ascii4uphex((uint32_t)pw_shell_offset,  buf+IDX_PW_SHELL);
	uint32_to_ascii8uphex((uint32_t)pw->pw_uid,       buf+IDX_PW_UID);
	uint32_to_ascii8uphex((uint32_t)pw->pw_gid,       buf+IDX_PW_GID);
	/* copy strings into buffer */
        buf += IDX_PW_HDRSZ;
	memcpy(buf+pw_name_offset,   pw->pw_name,   pw_name_len);
	memcpy(buf+pw_passwd_offset, pw->pw_passwd, pw_passwd_len);
	memcpy(buf+pw_gecos_offset,  pw->pw_gecos,  pw_gecos_len);
	memcpy(buf+pw_dir_offset,    pw->pw_dir,    pw_dir_len);
	memcpy(buf+pw_shell_offset,  pw->pw_shell,  pw_shell_len);
	/* separate entries with ':' for readability */
	buf[pw_passwd_offset-1]= ':';  /*(between pw_name and pw_passwd)*/
	buf[pw_gecos_offset-1] = ':';  /*(between pw_passwd and pw_gecos)*/
	buf[pw_dir_offset-1]   = ':';  /*(between pw_gecos and pw_dir)*/
	buf[pw_shell_offset-1] = ':';  /*(between pw_dir and pw_shell)*/
	return IDX_PW_HDRSZ + pw_shell_end;
    }
    else {
	errno = ERANGE;
	return 0;
    }
}


size_t
cdb_gr2str(char * restrict buf, const size_t bufsz,
	   struct group * const restrict gr)
  __attribute_nonnull__;
size_t
cdb_gr2str(char * restrict buf, const size_t bufsz,
	   struct group * const restrict gr)
{
    const size_t    gr_name_len      = strlen(gr->gr_name);
    const size_t    gr_passwd_len    = strlen(gr->gr_passwd);
    const uintptr_t gr_name_offset   = 0;
    const uintptr_t gr_passwd_offset = gr_name_offset   + gr_name_len   + 1;
    const uintptr_t gr_mem_str_offt  = gr_passwd_offset + gr_passwd_len + 1;
    char ** const restrict gr_mem = gr->gr_mem;
    const char * restrict str;
    size_t len;
    size_t gr_mem_num = 0;
    size_t offset = IDX_GR_HDRSZ + gr_mem_str_offt;
    if (   __builtin_expect(gr_name_len   <= USHRT_MAX, 1)
	&& __builtin_expect(gr_passwd_len <= USHRT_MAX, 1)
	&& __builtin_expect(offset        < bufsz,      1)) {
	while ((str = gr_mem[gr_mem_num]) != NULL
	       && __builtin_expect((len = strlen(str)) < bufsz-offset, 1)) {
	    memcpy(buf+offset, str, len);
	    buf[(offset+=len)] = ',';
	    ++offset;
	    ++gr_mem_num;
	}
	if (   __builtin_expect(offset     <= USHRT_MAX, 1)
	    && __builtin_expect(gr_mem_num <= USHRT_MAX, 1)
	    && __builtin_expect(gr_mem[gr_mem_num] == NULL,  1)
	    && __builtin_expect((gr_mem_num<<3)+8u+7u <= bufsz-offset, 1)) {
	    /* verify space in string for 8-aligned char ** gr_mem array + NULL
	     * (not strictly necessary, but best to catch excessively long
	     *  entries at cdb create time rather than in query at runtime) */

	    /* store string offsets into buffer */
	    uint16_to_ascii4uphex((uint32_t)gr_passwd_offset,buf+IDX_GR_PASSWD);
	    uint16_to_ascii4uphex((uint32_t)gr_mem_str_offt,buf+IDX_GR_MEM_STR);
	    uint16_to_ascii4uphex((uint32_t)offset,         buf+IDX_GR_MEM);
	    uint16_to_ascii4uphex((uint32_t)gr_mem_num,     buf+IDX_GR_MEM_NUM);
	    uint32_to_ascii8uphex((uint32_t)gr->gr_gid,     buf+IDX_GR_GID);
	    /* copy strings into buffer */
            buf += IDX_GR_HDRSZ;
	    memcpy(buf+gr_name_offset,   gr->gr_name,   gr_name_len);
	    memcpy(buf+gr_passwd_offset, gr->gr_passwd, gr_passwd_len);
	    /* separate entries with ':' for readability */
	    buf[gr_passwd_offset-1] =':'; /*(between gr_name and gr_passwd)*/
	    buf[gr_mem_str_offt-1]  =':'; /*(between gr_passwd and gr_mem str)*/
	    return IDX_GR_HDRSZ + offset;
	}
    }

    errno = ERANGE;
    return 0;
}

