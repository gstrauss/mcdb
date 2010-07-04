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
#include <stdlib.h>  /* malloc() calloc() free() strtol() strtoul() */

#include <pwd.h>
#include <grp.h>
#include <shadow.h>

/*
 * validate data sufficiently for successful serialize/deserialize into mcdb.
 *
 * input is database struct so that code is written to consumes what it produces
 */


/*
 * initgroups() and getgrouplist() support
 * (keep hash map of group memberships per member)
 */

struct nss_mcdb_acct_make_groupmem {
  struct nss_mcdb_acct_make_groupmem * restrict next;
  char * restrict name;
  uint32_t namelen;
  uint32_t hash;
  uint32_t ngids;
  uint32_t gidlist_sz;
  gid_t gidlist[];  /* C99 VLA */
};

enum { nss_mcdb_acct_make_groupmem_hashmap_sz = 2048 }; /* must be power of 2 */

static struct nss_mcdb_acct_make_groupmem ** restrict
  nss_mcdb_acct_make_groupmem_hashmap = NULL;

static struct nss_mcdb_acct_make_groupmem *
  nss_mcdb_acct_make_groupmem_pool = NULL;

static char *
  nss_mcdb_acct_make_groupmem_strpool = NULL;


static bool  __attribute_noinline__
nss_mcdb_acct_make_groupmem_pool_alloc(const size_t sz,
                                       size_t * const restrict pool_sz,
                                       size_t * const restrict pool_idx,
                                       void * const restrict pool)
{
    enum { GROUPMEM_POOL_SZ = 65536 + sizeof(uintptr_t) }; /*arbitrary pool_sz*/
    uintptr_t * restrict mem;

    const long sc_getgr_r_size_max = sysconf(_SC_GETGR_R_SIZE_MAX);
    if (sz > sc_getgr_r_size_max)
        return false;
    /* assert(GROUPMEM_POOL_SZ >= sc_getgr_r_size_max); */

    mem = (uintptr_t *) malloc(GROUPMEM_POOL_SZ);
    if (mem == NULL)
        return false;

    /* (first pool uintptr_t used for simple linked-list of prev allocations)
     * (for later free()) */

    *mem = (uintptr_t)*(uintptr_t **)pool;
    *(uintptr_t **)pool = mem;
    *pool_idx = sizeof(uintptr_t);
    *pool_sz  = GROUPMEM_POOL_SZ;

    return true;
}

static struct nss_mcdb_acct_make_groupmem *
nss_mcdb_acct_make_groupmem_alloc(const size_t sz)
{
    static size_t pool_sz  = 0;
    static size_t pool_idx = 0;

    /* caller should request aligned size; no need to realign here */
  #if defined(_LP64) || defined(__LP64__)
    /* assert(0 == (sz & 7)); */
  #else
    /* assert(0 == (sz & 3)); */
  #endif

    if ((sz <= pool_sz && pool_idx <= pool_sz - sz)
        || nss_mcdb_acct_make_groupmem_pool_alloc(
             sz, &pool_sz, &pool_idx,
             &nss_mcdb_acct_make_groupmem_pool)) {

        struct nss_mcdb_acct_make_groupmem * const restrict groupmem =
          (struct nss_mcdb_acct_make_groupmem *)
          (((char *)nss_mcdb_acct_make_groupmem_pool) + pool_idx);
        pool_idx += sz;
        return groupmem;
    }

    return NULL;
}

static char *
nss_mcdb_acct_make_groupmem_stralloc(const size_t sz)
{
    static size_t strpool_sz  = 0;
    static size_t strpool_idx = 0;

    if ((sz <= strpool_sz && strpool_idx <= strpool_sz - sz)
        || (nss_mcdb_acct_make_groupmem_pool_alloc(
              sz, &strpool_sz, &strpool_idx,
              &nss_mcdb_acct_make_groupmem_strpool))) {

        char * const restrict str =
          nss_mcdb_acct_make_groupmem_strpool + strpool_idx;
        strpool_idx += sz;
        return str;
    }

    return NULL;
}

static bool  __attribute_noinline__
nss_mcdb_acct_make_grouplist_free(const bool rc)
{
    uintptr_t * restrict mem;
    uintptr_t * restrict next;

    free(nss_mcdb_acct_make_groupmem_hashmap);
    nss_mcdb_acct_make_groupmem_hashmap = NULL;

    next = (uintptr_t *)nss_mcdb_acct_make_groupmem_pool;
    nss_mcdb_acct_make_groupmem_pool = NULL;
    while ((mem = next)) { next = (uintptr_t *)*mem; free(mem); }

    next = (uintptr_t *)nss_mcdb_acct_make_groupmem_strpool;
    nss_mcdb_acct_make_groupmem_strpool = NULL;
    while ((mem = next)) { next = (uintptr_t *)*mem; free(mem); }

    return rc;
}

/* copy data and insert into simple hash map */
static bool
nss_mcdb_acct_make_grouplist_add(const char * const restrict name,
                                 const gid_t gid)
{
    struct nss_mcdb_acct_make_groupmem *groupmem;
    struct nss_mcdb_acct_make_groupmem ** restrict hashmap_bucket;
    const size_t namelen = strlen(name);
    const uint32_t hash = uint32_hash_djb(UINT32_HASH_DJB_INIT, name, namelen);

    if (__builtin_expect( nss_mcdb_acct_make_groupmem_hashmap == NULL, 0)) {
        nss_mcdb_acct_make_groupmem_hashmap =
          (struct nss_mcdb_acct_make_groupmem **)
          calloc(nss_mcdb_acct_make_groupmem_hashmap_sz,
                 sizeof(struct nss_mcdb_acct_make_groupmem_hashmap *));
        if (__builtin_expect( nss_mcdb_acct_make_groupmem_hashmap == NULL, 0))
            return nss_mcdb_acct_make_grouplist_free(false);
    }

    hashmap_bucket =
      &nss_mcdb_acct_make_groupmem_hashmap[
        hash & (nss_mcdb_acct_make_groupmem_hashmap_sz-1)];
    for (groupmem = *hashmap_bucket; groupmem; groupmem = groupmem->next) {
        if (groupmem->hash == hash
            && groupmem->name[0] == name[0]
            && memcmp(groupmem->name, name, namelen+1) == 0)
            break;
    }

    if (groupmem == NULL) {
        /* allocate gidlist in multiples of 4 
         * (for array align of 4-bytes in 32-bit and 8-bytes in 64-bit) */
        groupmem = nss_mcdb_acct_make_groupmem_alloc(
          sizeof(struct nss_mcdb_acct_make_groupmem) + (sizeof(gid_t) * 4));
        if (__builtin_expect( groupmem == NULL, 0))
            return nss_mcdb_acct_make_grouplist_free(false);
        groupmem->name = nss_mcdb_acct_make_groupmem_stralloc(namelen+1);
        if (__builtin_expect( groupmem->name == NULL, 0))
            return nss_mcdb_acct_make_grouplist_free(false);
        memcpy(groupmem->name, name, namelen+1);
        groupmem->namelen    = (uint32_t)namelen;
        groupmem->hash       = hash;
        groupmem->ngids      = 0;
        groupmem->gidlist_sz = 4;   /* initial gidlist allocation above */
        groupmem->next       = *hashmap_bucket;
        *hashmap_bucket      = groupmem;
    }
    else if (groupmem->ngids == groupmem->gidlist_sz) {
        /* allocate gidlist in multiples of 4 
         * (for array align of 4-bytes in 32-bit and 8-bytes in 64-bit) */
        const size_t groupmem_sz = sizeof(struct nss_mcdb_acct_make_groupmem)
                                 + sizeof(gid_t) * groupmem->gidlist_sz;
        struct nss_mcdb_acct_make_groupmem * const restrict groupmem_new =
          nss_mcdb_acct_make_groupmem_alloc(groupmem_sz + (sizeof(gid_t) * 32));
        if (__builtin_expect( groupmem_new == NULL, 0))
            return nss_mcdb_acct_make_grouplist_free(false);
        groupmem = memcpy(groupmem_new, groupmem, groupmem_sz);
        groupmem->gidlist_sz += 32; /* incremental gidlist allocation above */
    }

    groupmem->gidlist[groupmem->ngids++] = gid;

    return true;
}

static size_t
nss_mcdb_acct_make_grouplist_datastr(char * restrict buf, const size_t bufsz,
                                     const struct nss_mcdb_acct_make_groupmem *
                                       const restrict groupmem)
{
    /* nss_mcdb_acct_make_group_flush() validates sane number of gids (ngids) */
    const gid_t * const gidlist = groupmem->gidlist;
    const uint32_t ngids = groupmem->ngids;
    const size_t sz = NSS_GL_HDRSZ + (ngids << 3); /* 8-char encoding per gid */
    if (sz <= bufsz) {
	uint32_to_ascii8uphex(ngids, buf+NSS_GL_NGROUPS);
	buf += NSS_GL_HDRSZ;
	for (uint32_t i = 0; i < ngids; ++i, buf+=8)
	    uint32_to_ascii8uphex(gidlist[i], buf);
	return sz;
    }
    else {
	errno = ERANGE;
	return 0;
    }
}

bool
nss_mcdb_acct_make_group_flush(struct nss_mcdb_make_winfo * const restrict w)
{
    const struct nss_mcdb_acct_make_groupmem * restrict groupmem;
    int i = 0;
    /* arbitrary limit: NSS_MCDB_NGROUPS_MAX in nss_mcdb_acct.h */
    /*(permit max ngids - 1 supplemental groups to validate input)*/
    const long sc_ngroups_max = sysconf(_SC_NGROUPS_MAX);
    const int ngids =
      (0 < sc_ngroups_max && sc_ngroups_max < NSS_MCDB_NGROUPS_MAX)
        ? sc_ngroups_max-1
        : NSS_MCDB_NGROUPS_MAX-1;

    w->tagc = '~';

    do {
        for (groupmem = nss_mcdb_acct_make_groupmem_hashmap[i];
             groupmem;
             groupmem = groupmem->next) {
            if (__builtin_expect( groupmem->ngids > ngids, 0))
                return nss_mcdb_acct_make_grouplist_free(false);
            w->klen = groupmem->namelen;
            w->key  = groupmem->name;
            w->dlen =
              nss_mcdb_acct_make_grouplist_datastr(w->data,w->datasz,groupmem);
            if (__builtin_expect( w->dlen == 0, 0))
                return nss_mcdb_acct_make_grouplist_free(false);
            if (__builtin_expect( !nss_mcdb_make_mcdbctl_write(w), 0))
                return nss_mcdb_acct_make_grouplist_free(false);
        }
    } while (++i < nss_mcdb_acct_make_groupmem_hashmap_sz);

    return nss_mcdb_acct_make_grouplist_free(true);
}

/*
 * end initgroups() and getgrouplist() support
 */



size_t
nss_mcdb_acct_make_passwd_datastr(char * restrict buf, const size_t bufsz,
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
    if (   __builtin_expect(NSS_PW_HDRSZ + pw_shell_end < bufsz, 1)
	&& __builtin_expect(pw_passwd_offset <= USHRT_MAX, 1)
	&& __builtin_expect(pw_gecos_offset  <= USHRT_MAX, 1)
	&& __builtin_expect(pw_dir_offset    <= USHRT_MAX, 1)
	&& __builtin_expect(pw_shell_offset  <= USHRT_MAX, 1)) { 
	/* store string offsets into buffer */
	uint16_to_ascii4uphex((uint32_t)pw_passwd_offset, buf+NSS_PW_PASSWD);
	uint16_to_ascii4uphex((uint32_t)pw_gecos_offset,  buf+NSS_PW_GECOS);
	uint16_to_ascii4uphex((uint32_t)pw_dir_offset,    buf+NSS_PW_DIR);
	uint16_to_ascii4uphex((uint32_t)pw_shell_offset,  buf+NSS_PW_SHELL);
	uint32_to_ascii8uphex((uint32_t)pw->pw_uid,       buf+NSS_PW_UID);
	uint32_to_ascii8uphex((uint32_t)pw->pw_gid,       buf+NSS_PW_GID);
	/* copy strings into buffer */
	buf += NSS_PW_HDRSZ;
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
	return NSS_PW_HDRSZ + pw_shell_end;
    }
    else {
	errno = ERANGE;
	return 0;
    }
}


size_t
nss_mcdb_acct_make_group_datastr(char * restrict buf, const size_t bufsz,
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
    size_t offset = NSS_GR_HDRSZ + gr_mem_str_offt;
    if (   __builtin_expect(gr_name_len   <= USHRT_MAX, 1)
	&& __builtin_expect(gr_passwd_len <= USHRT_MAX, 1)
	&& __builtin_expect(offset        < bufsz,      1)) {
	while ((str = gr_mem[gr_mem_num]) != NULL
	       && __builtin_expect((len = strlen(str)) < bufsz-offset, 1)) {
	    if (__builtin_expect(memccpy(buf+offset,str,',',len) == NULL,1)) {
		buf[(offset+=len)] = ',';
		++offset;
		++gr_mem_num;
	    }
	    else { /* bad group; comma-separated data may not contain commas */
		errno = EINVAL;
		return 0;
	    }
	} /* check for gr_mem[gr_mem_num] == NULL for sufficient buf space */
	if (   __builtin_expect(gr_mem_num <= USHRT_MAX, 1)
	    && __builtin_expect(gr_mem[gr_mem_num] == NULL,  1)
	    && __builtin_expect((gr_mem_num<<3)+8u+7u <= bufsz-offset, 1)) {
	    /* verify space in string for 8-aligned char ** gr_mem array + NULL
	     * (not strictly necessary, but best to catch excessively long
	     *  entries at cdb create time rather than in query at runtime) */

	    /* store string offsets into buffer */
	    offset -= NSS_GR_HDRSZ;
	    uint16_to_ascii4uphex((uint32_t)offset,         buf+NSS_GR_MEM);
	    uint16_to_ascii4uphex((uint32_t)gr_passwd_offset,buf+NSS_GR_PASSWD);
	    uint16_to_ascii4uphex((uint32_t)gr_mem_str_offt,buf+NSS_GR_MEM_STR);
	    uint16_to_ascii4uphex((uint32_t)gr_mem_num,     buf+NSS_GR_MEM_NUM);
	    uint32_to_ascii8uphex((uint32_t)gr->gr_gid,     buf+NSS_GR_GID);
	    /* copy strings into buffer */
	    buf += NSS_GR_HDRSZ;
	    memcpy(buf+gr_name_offset,   gr->gr_name,   gr_name_len);
	    memcpy(buf+gr_passwd_offset, gr->gr_passwd, gr_passwd_len);
	    /* separate entries with ':' for readability */
	    buf[gr_passwd_offset-1] =':'; /*(between gr_name and gr_passwd)*/
	    buf[gr_mem_str_offt-1]  =':'; /*(between gr_passwd and gr_mem str)*/
	    buf[offset] = '\0';           /* end string section */
	    return NSS_GR_HDRSZ + offset;
	}
    }

    errno = ERANGE;
    return 0;
}


size_t
nss_mcdb_acct_make_spwd_datastr(char * restrict buf, const size_t bufsz,
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
	uint16_to_ascii4uphex((uint32_t)sp_pwdp_offset, buf+NSS_SP_PWDP);
	uint32_to_ascii8uphex((uint32_t)sp->sp_lstchg,  buf+NSS_SP_LSTCHG);
	uint32_to_ascii8uphex((uint32_t)sp->sp_min,     buf+NSS_SP_MIN);
	uint32_to_ascii8uphex((uint32_t)sp->sp_max,     buf+NSS_SP_MAX);
	uint32_to_ascii8uphex((uint32_t)sp->sp_warn,    buf+NSS_SP_WARN);
	uint32_to_ascii8uphex((uint32_t)sp->sp_inact,   buf+NSS_SP_INACT);
	uint32_to_ascii8uphex((uint32_t)sp->sp_expire,  buf+NSS_SP_EXPIRE);
	uint32_to_ascii8uphex((uint32_t)sp->sp_flag,    buf+NSS_SP_FLAG);
	/* copy strings into buffer */
	buf += NSS_SP_HDRSZ;
	memcpy(buf+sp_namp_offset, sp->sp_namp, sp_namp_len);
	memcpy(buf+sp_pwdp_offset, sp->sp_pwdp, sp_pwdp_len);
	/* separate entries with ':' for readability */
	buf[sp_pwdp_offset-1]  = ':';  /*(between sp_namp and sp_pwdp)*/
	buf[sp_pwdp_end] = '\0';       /* end string section */
	return NSS_SP_HDRSZ + sp_pwdp_end;
    }
    else {
	errno = ERANGE;
	return 0;
    }
}



bool
nss_mcdb_acct_make_passwd_encode(
  struct nss_mcdb_make_winfo * const restrict w,
  const void * const restrict entp)
{
    const struct passwd * const restrict pw = entp;
    char hexstr[8];

    w->dlen = nss_mcdb_acct_make_passwd_datastr(w->data, w->datasz, pw);
    if (__builtin_expect( w->dlen == 0, 0))
        return false;

    w->tagc = '=';
    w->klen = strlen(pw->pw_name);
    w->key  = pw->pw_name;
    if (__builtin_expect( !nss_mcdb_make_mcdbctl_write(w), 0))
        return false;

    w->tagc = 'x';
    w->klen = sizeof(hexstr);
    w->key  = hexstr;
    uint32_to_ascii8uphex((uint32_t)pw->pw_uid, hexstr);
    if (__builtin_expect( !nss_mcdb_make_mcdbctl_write(w), 0))
        return false;

    return true;
}


bool
nss_mcdb_acct_make_group_encode(
  struct nss_mcdb_make_winfo * const restrict w,
  const void * const restrict entp)
{
    const struct group * const restrict gr = entp;
    char hexstr[8];

    for (unsigned int i = 0; gr->gr_mem[i] != NULL; ++i) {
        if (__builtin_expect(
              !nss_mcdb_acct_make_grouplist_add(gr->gr_mem[i], gr->gr_gid), 0))
            return false;
    }

    w->dlen = nss_mcdb_acct_make_group_datastr(w->data, w->datasz, gr);
    if (__builtin_expect( w->dlen == 0, 0))
        return false;

    w->tagc = '=';
    w->klen = strlen(gr->gr_name);
    w->key  = gr->gr_name;
    if (__builtin_expect( !nss_mcdb_make_mcdbctl_write(w), 0))
        return false;

    w->tagc = 'x';
    w->klen = sizeof(hexstr);
    w->key  = hexstr;
    uint32_to_ascii8uphex((uint32_t)gr->gr_gid, hexstr);
    if (__builtin_expect( !nss_mcdb_make_mcdbctl_write(w), 0))
        return false;

    return true;
}


bool
nss_mcdb_acct_make_spwd_encode(
  struct nss_mcdb_make_winfo * const restrict w,
  const void * const restrict entp)
{
    const struct spwd * const restrict sp = entp;
    w->dlen = nss_mcdb_acct_make_spwd_datastr(w->data, w->datasz, sp);
    if (__builtin_expect( w->dlen == 0, 0))
        return false;

    w->tagc = '=';
    w->klen = strlen(sp->sp_namp);
    w->key  = sp->sp_namp;
    if (__builtin_expect( !nss_mcdb_make_mcdbctl_write(w), 0))
        return false;

    return true;
}



bool
nss_mcdb_acct_make_passwd_parse(
  struct nss_mcdb_make_winfo * const restrict w, char * restrict p)
{
    char *b, *e;
    long n;
    struct passwd pw;

    for (; *p; ++p) {

        /* pw_name */
        pw.pw_name = b = p;
        TOKEN_COLONDELIM_END(p);
        if (*b == ':' || *p != ':')
            return false;               /* error: invalid line */
        *p = '\0';

        /* pw_passwd */
        pw.pw_passwd = b = ++p;
        TOKEN_COLONDELIM_END(p);
        if (*b == ':' || *p != ':')
            return false;               /* error: invalid line */
        *p = '\0';

        /* pw_uid */
        b = ++p;
        TOKEN_COLONDELIM_END(p);
        if (*b == ':' || *p != ':')
            return false;               /* error: invalid line */
        *p = '\0';
        n = strtol(b, &e, 10);
        if (*e != '\0' || n < 0 || n == LONG_MAX || n >= INT_MAX)
            return false;               /* error: invalid uid */
        if ((uid_t)-1 > 0 && !(n <= (uid_t)-1))
            return false;               /* error: invalid uid */
        pw.pw_uid = (uid_t)n;

        /* pw_gid */
        b = ++p;
        TOKEN_COLONDELIM_END(p);
        if (*b == ':' || *p != ':')
            return false;               /* error: invalid line */
        *p = '\0';
        n = strtol(b, &e, 10);
        if (*e != '\0' || n < 0 || n == LONG_MAX || n >= INT_MAX)
            return false;               /* error: invalid gid */
        if ((gid_t)-1 > 0 && !(n <= (gid_t)-1))
            return false;               /* error: invalid gid */
        pw.pw_gid = (gid_t)n;

        /* pw_gecos */
        pw.pw_gecos = b = ++p;
        TOKEN_COLONDELIM_END(p);
        if (*p != ':')                  /* gecos can be blank */
            return false;               /* error: invalid line */
        *p = '\0';

        /* pw_dir */
        pw.pw_dir = b = ++p;
        TOKEN_COLONDELIM_END(p);
        if (*p != ':')                  /* dir can be blank; default: "/" */
            return false;               /* error: invalid line */
        *p = '\0';

        /* pw_shell */
        pw.pw_shell = b = ++p;
        TOKEN_COLONDELIM_END(p);
        if (*p != '\n')                 /* shell can be blank */
            return false;               /* error: invalid line */
        *p = '\0';

        /* find newline (to prep for beginning of next line) (checked above) */

        /* process struct passwd */
        if (!w->encode(w, &pw))
            return false;
    }

    return true;
}


bool
nss_mcdb_acct_make_group_parse(
  struct nss_mcdb_make_winfo * const restrict w, char * restrict p)
{
    char *b, *e;
    int c;
    long n;
    struct group gr;
    /* arbitrary limit: NSS_MCDB_NGROUPS_MAX in nss_mcdb_acct.h */
    /*(permit max gr_mem-1 supplemental groups + NULL term to validate input)*/
    const long sc_ngroups_max = sysconf(_SC_NGROUPS_MAX);
    char *gr_mem[(0 < sc_ngroups_max && sc_ngroups_max < NSS_MCDB_NGROUPS_MAX-1)
                 ? sc_ngroups_max
                 : NSS_MCDB_NGROUPS_MAX];

    gr.gr_mem = gr_mem;

    for (; *p; ++p) {

        /* gr_name */
        gr.gr_name = b = p;
        TOKEN_COLONDELIM_END(p);
        if (*b == ':' || *p != ':')
            return false;               /* error: invalid line */
        *p = '\0';

        /* gr_passwd */
        gr.gr_passwd = b = ++p;
        TOKEN_COLONDELIM_END(p);
        if (*b == ':' || *p != ':')
            return false;               /* error: invalid line */
        *p = '\0';

        /* gr_gid */
        b = ++p;
        TOKEN_COLONDELIM_END(p);
        if (*b == ':' || *p != ':')
            return false;               /* error: invalid line */
        *p = '\0';
        n = strtol(b, &e, 10);
        if (*e != '\0' || n < 0 || n == LONG_MAX || n >= INT_MAX)
            return false;               /* error: invalid gid */
        if ((gid_t)-1 > 0 && !(n <= (gid_t)-1))
            return false;               /* error: invalid gid */
        gr.gr_gid = (gid_t)n;

        /* gr_mem */
        n = 0;
        do {
            if (*(b = ++p) == '\n')
                break;                  /* done; no more aliases*/
            TOKEN_COMMADELIM_END(p);
            if ((c = *p) == '\0')       /* error: invalid line */
                return false;
            *p = '\0';
            if (n < (sizeof(gr_mem)/sizeof(char *) - 1))
                gr.gr_mem[n++] = b;
            else
                return false; /* too many members to fit in fixed-sized array */
        } while (c != '\n');
        gr.gr_mem[n] = NULL;

        /* find newline (to prep for beginning of next line) */
        if (c != '\n')   /* error: no newline; truncated? */
            return false;

        /* process struct group */
        if (!w->encode(w, &gr))
            return false;
    }

    return true;
}


static char *
nss_mcdb_acct_make_parse_long_int_colon(char * const restrict b,
                                        long int * const restrict n)
{
    char *p = b;
    char *e;
    TOKEN_COLONDELIM_END(p);
    if (*p != ':')
        return NULL;                    /* error: invalid line */
    *n = -1;                            /* -1 if field is empty */
    if (b != p) {
        *p = '\0';
        errno = 0;
        *n = strtol(b, &e, 10);
        if (*e != '\0' || ((*n==LONG_MIN || *n==LONG_MAX) && errno == ERANGE))
            return NULL;                /* error: invalid number */
    }
    return p;
}


bool
nss_mcdb_acct_make_shadow_parse(
  struct nss_mcdb_make_winfo * const restrict w, char * restrict p)
{
    char *b, *e;
    struct spwd sp;

    for (; *p; ++p) {

        /* sp_namp */
        sp.sp_namp = b = p;
        TOKEN_COLONDELIM_END(p);
        if (*b == ':' || *p != ':')
            return false;               /* error: invalid line */
        *p = '\0';

        /* sp_pwdp */
        sp.sp_pwdp = b = ++p;
        TOKEN_COLONDELIM_END(p);
        if (*b == ':' || *p != ':')
            return false;               /* error: invalid line */
        *p = '\0';

        /* sp_lstchg */
        p = nss_mcdb_acct_make_parse_long_int_colon(++p, &sp.sp_lstchg);
        if (p == NULL)
            return false;               /* error: invalid line */
        /* sp_min */
        p = nss_mcdb_acct_make_parse_long_int_colon(++p, &sp.sp_min);
        if (p == NULL)
            return false;               /* error: invalid line */
        /* sp_max */
        p = nss_mcdb_acct_make_parse_long_int_colon(++p, &sp.sp_max);
        if (p == NULL)
            return false;               /* error: invalid line */
        /* sp_warn */
        p = nss_mcdb_acct_make_parse_long_int_colon(++p, &sp.sp_warn);
        if (p == NULL)
            return false;               /* error: invalid line */
        /* sp_inact */
        p = nss_mcdb_acct_make_parse_long_int_colon(++p, &sp.sp_inact);
        if (p == NULL)
            return false;               /* error: invalid line */
        /* sp_expire */
        p = nss_mcdb_acct_make_parse_long_int_colon(++p, &sp.sp_expire);
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
            sp.sp_flag = strtoul(b, &e, 10);
            if (*e != '\0' || (sp.sp_flag == ULONG_MAX && errno == ERANGE))
                return false;           /* error: invalid number */
        }

        /* find newline (to prep for beginning of next line) (checked above) */

        /* process struct spwd */
        if (!w->encode(w, &sp))
            return false;
    }

    return true;
}
