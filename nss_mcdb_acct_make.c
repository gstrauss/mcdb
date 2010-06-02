/* memccpy() on Linux requires _XOPEN_SOURCE or _BSD_SOURCE or _SVID_SOURCE */
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif

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

/*
 * validate data sufficiently for successful serialize/deserialize into mcdb.
 *
 * input is database struct so that code is written to consumes it produces.
 */

/* buf size passed should be _SC_GETPW_R_SIZE_MAX + IDX_PW_HDRSZ (not checked)*/
size_t
cdb_pw2str(char * restrict buf, const size_t bufsz,
	   const struct passwd * const restrict pw)
  __attribute_nonnull__;
size_t
cdb_pw2str(char * restrict buf, const size_t bufsz,
	   const struct passwd * const restrict pw)
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
    if (   __builtin_expect(IDX_PW_HDRSZ + pw_shell_end < bufsz, 1)
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
	buf[pw_shell_end] = '\0';      /* end string section */
	return IDX_PW_HDRSZ + pw_shell_end;
    }
    else {
	errno = ERANGE;
	return 0;
    }
}


/* buf size passed should be _SC_GETGR_R_SIZE_MAX + IDX_GR_HDRSZ (not checked)*/
size_t
cdb_gr2str(char * restrict buf, const size_t bufsz,
	   const struct group * const restrict gr)
  __attribute_nonnull__;
size_t
cdb_gr2str(char * restrict buf, const size_t bufsz,
	   const struct group * const restrict gr)
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
	    if (__builtin_expect(memccpy(buf+offset, str, ' ', len) == NULL,1)){
		buf[(offset+=len)] = ',';
		++offset;
		++gr_mem_num;
	    }
	    else { /* bad group; comma-separated data may not contain commas */
		errno = EINVAL;
		return 0;
	    }
	}
	offset -= IDX_GR_HDRSZ;
	if (   __builtin_expect(IDX_GR_HDRSZ + offset < bufsz, 1)
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
	    buf[offset] = '\0';           /* end string section */
	    return IDX_GR_HDRSZ + offset;
	}
    }

    errno = ERANGE;
    return 0;
}


size_t
cdb_spwd2str(char * restrict buf, const size_t bufsz,
	     const struct spwd * const restrict sp)
  __attribute_nonnull__;
size_t
cdb_spwd2str(char * restrict buf, const size_t bufsz,
	     const struct spwd * const restrict sp)
{
    const size_t sp_namp_len = strlen(sp->sp_namp);
    const size_t sp_pwdp_len = strlen(sp->sp_pwdp);
    const uintptr_t sp_namp_offset = 0;
    const uintptr_t sp_pwdp_offset = sp_namp_offset + sp_namp_len + 1;
    const uintptr_t sp_pwdp_end    = sp_pwdp_offset + sp_pwdp_len;
    /*(sp_pwdp_end < bufsz to leave +1 in buffer for final '\n' or '\0')*/
    if (   __builtin_expect(sp_pwdp_end    <  bufsz,     1)
	&& __builtin_expect(sp_pwdp_offset <= USHRT_MAX, 1)) {
	/* store string offsets into buffer */
	uint16_to_ascii4uphex((uint32_t)sp_pwdp_offset, buf+IDX_SP_PWDP);
	uint32_to_ascii8uphex((uint32_t)sp->sp_lstchg,  buf+IDX_SP_LSTCHG);
	uint32_to_ascii8uphex((uint32_t)sp->sp_min,     buf+IDX_SP_MIN);
	uint32_to_ascii8uphex((uint32_t)sp->sp_max,     buf+IDX_SP_MAX);
	uint32_to_ascii8uphex((uint32_t)sp->sp_warn,    buf+IDX_SP_WARN);
	uint32_to_ascii8uphex((uint32_t)sp->sp_inact,   buf+IDX_SP_INACT);
	uint32_to_ascii8uphex((uint32_t)sp->sp_expire,  buf+IDX_SP_EXPIRE);
	uint32_to_ascii8uphex((uint32_t)sp->sp_flag,    buf+IDX_SP_FLAG);
	/* copy strings into buffer */
	buf += IDX_SP_HDRSZ;
	memcpy(buf+sp_namp_offset, sp->sp_namp, sp_namp_len);
	memcpy(buf+sp_pwdp_offset, sp->sp_pwdp, sp_pwdp_len);
	/* separate entries with ':' for readability */
	buf[sp_pwdp_offset-1]  = ':';  /*(between sp_namp and sp_pwdp)*/
	buf[sp_pwdp_end] = '\0';       /* end string section */
	return IDX_SP_HDRSZ + sp_pwdp_end;
    }
    else {
	errno = ERANGE;
	return 0;
    }
}
