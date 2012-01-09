/*
 * nss_mcdb_acct_make - mcdb of passwd, group nsswitch.conf databases
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

/* strtoull() needs _XOPEN_SOURCE 600 on some platforms */
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#include "nss_mcdb_acct_make.h"
#include "nss_mcdb_acct.h"
#include "nss_mcdb.h"
#include "uint32.h"

#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>     /* malloc() calloc() free() strtol() strtoul() */
#include <arpa/inet.h>  /* htonl(), htons() */

#include <pwd.h>
#include <grp.h>

/*
 * validate data sufficiently for successful serialize/deserialize into mcdb.
 *
 * input is database struct so that code is written to consumes what it produces
 *
 * (future: consider making *_datastr() routines 'static')
 *
 * (buf passed to *_datastr() routines is aligned,
 *  but care is taken to not rely on char *buf alignment)
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
    const size_t sz = NSS_GL_HDRSZ + (ngids << 2); /* 4-char encoding per gid */
    union { uint32_t u[NSS_GL_HDRSZ>>2]; uint16_t h[NSS_GL_HDRSZ>>1]; } hdr;
    union { uint32_t u; char c[4]; } g;
    hdr.u[NSS_GL_NGROUPS>>2] = htonl(ngids);
    if (sz <= bufsz) {
	memcpy(buf, hdr.u, NSS_GL_HDRSZ);
	buf += NSS_GL_HDRSZ;
	for (uint32_t i = 0; i < ngids; ++i, buf+=4) {
	    g.u = htonl(gidlist[i]);
	    buf[0] = g.c[0]; buf[1] = g.c[1]; buf[2] = g.c[2]; buf[3] = g.c[3];
	}
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
    /*(permit max ngids supplemental groups to validate input)*/
    const long sc_ngroups_max = sysconf(_SC_NGROUPS_MAX);
    const int ngids =
      (0 < sc_ngroups_max && sc_ngroups_max < NSS_MCDB_NGROUPS_MAX)
        ? sc_ngroups_max
        : NSS_MCDB_NGROUPS_MAX;

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
    const size_t pw_name_len   = 1 + strlen(pw->pw_name);
    const size_t pw_passwd_len = 1 + strlen(pw->pw_passwd);
  #if defined(__sun)
    const size_t pw_age_len    = 1 + strlen(pw->pw_age);
    const size_t pw_comment_len= 1 + strlen(pw->pw_comment);
  #elif defined(__FreeBSD__)
    const size_t pw_class_len  = 1 + strlen(pw->pw_class);
  #endif
    const size_t pw_gecos_len  = 1 + strlen(pw->pw_gecos);
    const size_t pw_dir_len    = 1 + strlen(pw->pw_dir);
    const size_t pw_shell_len  = 1 + strlen(pw->pw_shell);
    const uintptr_t pw_passwd_offset =                    pw_name_len;
  #if defined(__sun)
    const uintptr_t pw_age_offset    = pw_passwd_offset + pw_passwd_len;
    const uintptr_t pw_comment_offset= pw_age_offset    + pw_age_len;
    const uintptr_t pw_gecos_offset  = pw_comment_offset+ pw_comment_len;
  #elif defined(__FreeBSD__)
    const uintptr_t pw_class_offset  = pw_passwd_offset + pw_passwd_len;
    const uintptr_t pw_gecos_offset  = pw_class_offset  + pw_class_len;
  #else
    const uintptr_t pw_gecos_offset  = pw_passwd_offset + pw_passwd_len;
  #endif
    const uintptr_t pw_dir_offset    = pw_gecos_offset  + pw_gecos_len;
    const uintptr_t pw_shell_offset  = pw_dir_offset    + pw_dir_len;
    const size_t dlen = NSS_PW_HDRSZ + pw_shell_offset  + pw_shell_len;
    union { uint32_t u[NSS_PW_HDRSZ>>2]; uint16_t h[NSS_PW_HDRSZ>>1]; } hdr;
    if (   __builtin_expect(dlen             <= bufsz,     1)
	&& __builtin_expect(pw_passwd_offset <= USHRT_MAX, 1)
      #if defined(__sun)
	&& __builtin_expect(pw_age_offset    <= USHRT_MAX, 1)
	&& __builtin_expect(pw_comment_offset<= USHRT_MAX, 1)
      #elif defined(__FreeBSD__)
	&& __builtin_expect(pw_class_offset  <= USHRT_MAX, 1)
      #endif
	&& __builtin_expect(pw_gecos_offset  <= USHRT_MAX, 1)
	&& __builtin_expect(pw_dir_offset    <= USHRT_MAX, 1)
	&& __builtin_expect(pw_shell_offset  <= USHRT_MAX, 1)) { 
	/* store string offsets into aligned header, then copy into buf */
	/* copy strings into buffer, including string terminating '\0' */
	hdr.h[NSS_PW_PASSWD>>1]     = htons((uint16_t) pw_passwd_offset);
	hdr.h[NSS_PW_GECOS>>1]      = htons((uint16_t) pw_gecos_offset);
	hdr.h[NSS_PW_DIR>>1]        = htons((uint16_t) pw_dir_offset);
	hdr.h[NSS_PW_SHELL>>1]      = htons((uint16_t) pw_shell_offset);
	hdr.u[NSS_PW_UID>>2]        = htonl((uint32_t) pw->pw_uid);
	hdr.u[NSS_PW_GID>>2]        = htonl((uint32_t) pw->pw_gid);
      #if defined(__sun)
	hdr.h[NSS_PW_AGE>>1]        = htons((uint16_t) pw_age_offset);
	hdr.h[NSS_PW_COMMENT>>1]    = htons((uint16_t) pw_comment_offset);
      #elif defined(__FreeBSD__)
	hdr.u[NSS_PW_CHANGE>>2]     = htonl((uint32_t)(pw->pw_change>>32));
	hdr.u[(NSS_PW_CHANGE>>2)+1] = htonl((uint32_t) pw->pw_change);
	hdr.u[NSS_PW_EXPIRE>>2]     = htonl((uint32_t)(pw->pw_expire>>32));
	hdr.u[(NSS_PW_EXPIRE>>2)+1] = htonl((uint32_t) pw->pw_expire);
	hdr.u[NSS_PW_FIELDS>>2]     = htonl((uint32_t) pw->pw_fields);
	hdr.h[NSS_PW_CLASS>>1]      = htons((uint16_t) pw_class_offset);
	hdr.h[(NSS_PW_HDRSZ>>1)-1]  = 0;/*(clear last 2 bytes for consistency)*/
      #endif
	memcpy(buf,                  hdr.u,         NSS_PW_HDRSZ);
	memcpy((buf+=NSS_PW_HDRSZ),  pw->pw_name,   pw_name_len);
	memcpy(buf+pw_passwd_offset, pw->pw_passwd, pw_passwd_len);
      #if defined(__sun)
	memcpy(buf+pw_age_offset,    pw->pw_age,    pw_age_len);
	memcpy(buf+pw_comment_offset,pw->pw_comment,pw_comment_len);
      #elif defined(__FreeBSD__)
	memcpy(buf+pw_class_offset,  pw->pw_class,  pw_class_len);
      #endif
	memcpy(buf+pw_gecos_offset,  pw->pw_gecos,  pw_gecos_len);
	memcpy(buf+pw_dir_offset,    pw->pw_dir,    pw_dir_len);
	memcpy(buf+pw_shell_offset,  pw->pw_shell,  pw_shell_len);
	return dlen - 1; /* subtract final '\0' */
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
    const size_t    gr_name_len      = 1 + strlen(gr->gr_name);
    const size_t    gr_passwd_len    = 1 + strlen(gr->gr_passwd);
    const uintptr_t gr_passwd_offset =                    gr_name_len;
    const uintptr_t gr_mem_str_offset= gr_passwd_offset + gr_passwd_len;
    char ** const restrict gr_mem = gr->gr_mem;
    const char * restrict str;
    size_t len;
    size_t gr_mem_num = 0;
    size_t dlen = NSS_GR_HDRSZ + gr_mem_str_offset;
    union { uint32_t u[NSS_GR_HDRSZ>>2]; uint16_t h[NSS_GR_HDRSZ>>1]; } hdr;
    if (   __builtin_expect(gr_name_len   <= USHRT_MAX, 1)
	&& __builtin_expect(gr_passwd_len <= USHRT_MAX, 1)
	&& __builtin_expect(dlen          <  bufsz,     1)) {
	while ((str = gr_mem[gr_mem_num]) != NULL
	       && __builtin_expect((len = 1+strlen(str)) <= bufsz-dlen, 1)) {
	    memcpy(buf+dlen, str, len);
	    dlen += len;
	    ++gr_mem_num;
	} /* check for gr_mem[gr_mem_num] == NULL for sufficient buf space */
	if (   __builtin_expect(gr_mem_num <= USHRT_MAX, 1)
	    && __builtin_expect(gr_mem[gr_mem_num] == NULL,  1)
	    && __builtin_expect((gr_mem_num<<3)+8u+7u <= bufsz-dlen, 1)) {
	    /* verify space in string for 8-aligned char ** gr_mem array + NULL
	     * (not strictly necessary, but best to catch excessively long
	     *  entries at mcdb create time rather than in query at runtime) */

	    /* store string offsets into aligned header, then copy into buf */
	    /* copy strings into buffer, including string terminating '\0' */
	    hdr.h[NSS_GR_MEM>>1]     = htons((uint16_t) (dlen-NSS_GR_HDRSZ));
	    hdr.h[NSS_GR_PASSWD>>1]  = htons((uint16_t) gr_passwd_offset);
	    hdr.h[NSS_GR_MEM_STR>>1] = htons((uint16_t) gr_mem_str_offset);
	    hdr.h[NSS_GR_MEM_NUM>>1] = htons((uint16_t) gr_mem_num);
	    hdr.u[NSS_GR_GID>>2]     = htonl((uint32_t) gr->gr_gid);
	    memcpy(buf,                  hdr.u,         NSS_GR_HDRSZ);
	    memcpy((buf+=NSS_GR_HDRSZ),  gr->gr_name,   gr_name_len);
	    memcpy(buf+gr_passwd_offset, gr->gr_passwd, gr_passwd_len);
	    return dlen;
	}
    }

    errno = ERANGE;
    return 0;
}



bool
nss_mcdb_acct_make_passwd_encode(
  struct nss_mcdb_make_winfo * const restrict w,
  const void * const restrict entp)
{
    const struct passwd * const restrict pw = entp;
    const uint32_t n = htonl((uint32_t)pw->pw_uid);

    w->dlen = nss_mcdb_acct_make_passwd_datastr(w->data, w->datasz, pw);
    if (__builtin_expect( w->dlen == 0, 0))
        return false;

    w->tagc = '=';
    w->klen = strlen(pw->pw_name);
    w->key  = pw->pw_name;
    if (__builtin_expect( !nss_mcdb_make_mcdbctl_write(w), 0))
        return false;

    w->tagc = 'x';
    w->klen = sizeof(uint32_t);
    w->key  = (const char *)&n;
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
    const uint32_t n = htonl((uint32_t)gr->gr_gid);

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
    w->klen = sizeof(uint32_t);
    w->key  = (const char *)&n;
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

        /* skip comment lines beginning with '#' (supported by some vendors) */
        if (*p == '#')
            TOKEN_POUNDCOMMENT_SKIP(p);

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
        errno = 0;
        n = strtol(b, &e, 10);
        if (*e != '\0' || ((n == LONG_MIN || n == LONG_MAX) && errno == ERANGE))
            return false;               /* error: invalid number */
        pw.pw_uid = (n >= 0 && n == (uid_t)n)
          ? (uid_t)n
          : (n == -2 ? (uid_t)-2 : (uid_t)-1);
        if (pw.pw_uid == (uid_t)-1)
            return false;

        /* pw_gid */
        b = ++p;
        TOKEN_COLONDELIM_END(p);
        if (*b == ':' || *p != ':')
            return false;               /* error: invalid line */
        *p = '\0';
        errno = 0;
        n = strtol(b, &e, 10);
        if (*e != '\0' || ((n == LONG_MIN || n == LONG_MAX) && errno == ERANGE))
            return false;               /* error: invalid number */
        pw.pw_gid = (n >= 0 && n == (gid_t)n)
          ? (gid_t)n
          : (n == -2 ? (gid_t)-2 : (gid_t)-1);
        if (pw.pw_gid == (gid_t)-1)
            return false;

      #ifdef __FreeBSD__
        /* pw_change */
        b = ++p;
        TOKEN_COLONDELIM_END(p);
        if (*p != ':')
            return NULL;                /* error: invalid line */
        pw.pw_change = (time_t)-1;      /* -1 if field is empty */
        if (b != p) {
            *p = '\0';
            errno = 0;
            pw.pw_change = strtoull(b, &e, 10);
            if (*e != '\0' || (pw.pw_change == ULLONG_MAX && errno == ERANGE))
                return false;           /* error: invalid number */
        }

        /* pw_class */
        pw.pw_class = b = ++p;
        TOKEN_COLONDELIM_END(p);
        if (*p != ':')                  /* class can be blank (?) */
            return false;               /* error: invalid line */
        *p = '\0';
      #endif

      #ifdef __sun
        /* pw_age */
        pw.pw_age = b = ++p;
        TOKEN_COLONDELIM_END(p);
        if (*p != ':')                  /* age can be blank */
            return false;               /* error: invalid line */
        *p = '\0';

        /* pw_comment */
        pw.pw_comment = b = ++p;
        TOKEN_COLONDELIM_END(p);
        if (*p != ':')                  /* comment can be blank */
            return false;               /* error: invalid line */
        *p = '\0';
      #endif

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
      #ifdef __FreeBSD__
        if (*p != ':')                  /* shell can be blank (delim:colon) */
      #else
        if (*p != '\n')                 /* shell can be blank (delim:newline) */
      #endif
            return false;               /* error: invalid line */
        *p = '\0';

      #ifdef __FreeBSD__
        /* pw_expire */
        b = ++p;
        TOKEN_COLONDELIM_END(p);
        if (*p != ':')
            return NULL;                /* error: invalid line */
        pw.pw_expire = (time_t)-1;      /* -1 if field is empty */
        if (b != p) {
            *p = '\0';
            errno = 0;
            pw.pw_expire = strtoull(b, &e, 10);
            if (*e != '\0' || (pw.pw_expire == ULLONG_MAX && errno == ERANGE))
                return false;           /* error: invalid number */
        }

        /* pw_fields */
        b = ++p;
        TOKEN_COLONDELIM_END(p);
        if (*p != '\n')                 /* (delim:newline) */
            return false;               /* error: invalid line */
        pw.pw_fields = -1;              /* -1 if field is empty */
        if (b != p) {
            *p = '\0';
            errno = 0;
            n = strtol(b, &e, 10);
            if (*e != '\0' || ((n==LONG_MIN || n==LONG_MAX) && errno == ERANGE))
                return false;           /* error: invalid number */
            if (LONG_MAX != INT_MAX && (n < INT_MIN || INT_MAX < n))
                return false;           /* error: number out of range */
            pw.pw_fields = (int)n;
        }
      #endif

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
    /*(permit nmax gr_mem supplemental groups + NULL)*/
    const long sc_ngroups_max = sysconf(_SC_NGROUPS_MAX);
    const long nmax =
      (0 < sc_ngroups_max && sc_ngroups_max < NSS_MCDB_NGROUPS_MAX)
        ? sc_ngroups_max
        : NSS_MCDB_NGROUPS_MAX;
    char *gr_mem[nmax+1];

    gr.gr_mem = gr_mem;

    for (; *p; ++p) {

        /* skip comment lines beginning with '#' (supported by some vendors) */
        if (*p == '#')
            TOKEN_POUNDCOMMENT_SKIP(p);

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
        errno = 0;
        n = strtol(b, &e, 10);
        if (*e != '\0' || ((n == LONG_MIN || n == LONG_MAX) && errno == ERANGE))
            return false;               /* error: invalid number */
        gr.gr_gid = (n >= 0 && n == (gid_t)n)
          ? (gid_t)n
          : (n == -2 ? (gid_t)-2 : (gid_t)-1);
        if (gr.gr_gid == (gid_t)-1)
            return false;

        /* gr_mem */
        n = 0;
        do {
            if ((c = *(b = ++p)) == '\n')
                break;                  /* done; no more aliases*/
            TOKEN_COMMADELIM_END(p);
            if ((c = *p) == '\0')       /* error: invalid line */
                return false;
            *p = '\0';
            if (n < nmax)
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
