/*
 * nss_mcdb_acct - query mcdb of passwd, group nsswitch.conf databases
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

#include "nss_mcdb_acct.h"
#include "nss_mcdb.h"

#include <errno.h>
#include <string.h>

#include <pwd.h>
#include <grp.h>
#include <stdlib.h>     /* realloc */
#include <arpa/inet.h>  /* ntohl(), ntohs() */

/*
 * man passwd(5) getpwnam getpwuid setpwent getpwent endpwent
 *     /etc/passwd
 * man group(5)  getgrnam getgrgid setgrent getgrent endgrent getgrouplist
 *     /etc/group
 */

static nss_status_t
nss_mcdb_acct_passwd_decode(struct mcdb * restrict,
                            const struct nss_mcdb_vinfo * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;

static nss_status_t
nss_mcdb_acct_group_decode(struct mcdb * restrict,
                           const struct nss_mcdb_vinfo * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;

static nss_status_t
nss_mcdb_acct_grouplist_decode(struct mcdb * restrict,
                               const struct nss_mcdb_vinfo * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;


void _nss_mcdb_setpwent(void) { nss_mcdb_setent(NSS_DBTYPE_PASSWD,0); }
void _nss_mcdb_endpwent(void) { nss_mcdb_endent(NSS_DBTYPE_PASSWD);   }
void _nss_mcdb_setgrent(void) { nss_mcdb_setent(NSS_DBTYPE_GROUP,0);  }
void _nss_mcdb_endgrent(void) { nss_mcdb_endent(NSS_DBTYPE_GROUP);    }


nss_status_t
_nss_mcdb_getpwent_r(struct passwd * const restrict pwbuf,
                     char * const restrict buf, const size_t bufsz,
                     int * const restrict errnop)
{
    const struct nss_mcdb_vinfo v = { .decode  = nss_mcdb_acct_passwd_decode,
                                      .vstruct = pwbuf,
                                      .buf     = buf,
                                      .bufsz   = bufsz,
                                      .errnop  = errnop };
    return nss_mcdb_getent(NSS_DBTYPE_PASSWD, &v);
}

nss_status_t
_nss_mcdb_getpwnam_r(const char * const restrict name,
                     struct passwd * const restrict pwbuf,
                     char * const restrict buf, const size_t bufsz,
                     int * const restrict errnop)
{
    const struct nss_mcdb_vinfo v = { .decode  = nss_mcdb_acct_passwd_decode,
                                      .vstruct = pwbuf,
                                      .buf     = buf,
                                      .bufsz   = bufsz,
                                      .errnop  = errnop,
                                      .key     = name,
                                      .klen    = strlen(name),
                                      .tagc    = (unsigned char)'=' };
    return nss_mcdb_get_generic(NSS_DBTYPE_PASSWD, &v);
}

nss_status_t
_nss_mcdb_getpwuid_r(const uid_t uid,
                     struct passwd * const restrict pwbuf,
                     char * const restrict buf, const size_t bufsz,
                     int * const restrict errnop)
{
    const uint32_t n = htonl((uint32_t)uid);
    const struct nss_mcdb_vinfo v = { .decode  = nss_mcdb_acct_passwd_decode,
                                      .vstruct = pwbuf,
                                      .buf     = buf,
                                      .bufsz   = bufsz,
                                      .errnop  = errnop,
                                      .key     = (const char *)&n,
                                      .klen    = sizeof(uint32_t),
                                      .tagc    = (unsigned char)'x' };
    return nss_mcdb_get_generic(NSS_DBTYPE_PASSWD, &v);
}


nss_status_t
_nss_mcdb_getgrent_r(struct group * const restrict grbuf,
                     char * const restrict buf, const size_t bufsz,
                     int * const restrict errnop)
{
    const struct nss_mcdb_vinfo v = { .decode  = nss_mcdb_acct_group_decode,
                                      .vstruct = grbuf,
                                      .buf     = buf,
                                      .bufsz   = bufsz,
                                      .errnop  = errnop };
    return nss_mcdb_getent(NSS_DBTYPE_GROUP, &v);
}

nss_status_t
_nss_mcdb_getgrnam_r(const char * const restrict name,
                     struct group * const restrict grbuf,
                     char * const restrict buf, const size_t bufsz,
                     int * const restrict errnop)
{
    const struct nss_mcdb_vinfo v = { .decode  = nss_mcdb_acct_group_decode,
                                      .vstruct = grbuf,
                                      .buf     = buf,
                                      .bufsz   = bufsz,
                                      .errnop  = errnop,
                                      .key     = name,
                                      .klen    = strlen(name),
                                      .tagc    = (unsigned char)'=' };
    return nss_mcdb_get_generic(NSS_DBTYPE_GROUP, &v);
}

nss_status_t
_nss_mcdb_getgrgid_r(const gid_t gid,
                     struct group * const restrict grbuf,
                     char * const restrict buf, const size_t bufsz,
                     int * const restrict errnop)
{
    const uint32_t n = htonl((uint32_t)gid);
    const struct nss_mcdb_vinfo v = { .decode  = nss_mcdb_acct_group_decode,
                                      .vstruct = grbuf,
                                      .buf     = buf,
                                      .bufsz   = bufsz,
                                      .errnop  = errnop,
                                      .key     = (const char *)&n,
                                      .klen    = sizeof(uint32_t),
                                      .tagc    = (unsigned char)'x' };
    return nss_mcdb_get_generic(NSS_DBTYPE_GROUP, &v);
}


int
nss_mcdb_getgrouplist(const char * const restrict user, gid_t group,
                      gid_t * const restrict groups,
                      int * const restrict ngroups)
{
    int errnum;
    const struct nss_mcdb_vinfo v = { .decode  = nss_mcdb_acct_grouplist_decode,
                                      .vstruct = &group,
                                      .buf     = (char * restrict)groups,
                                      .bufsz   = *ngroups,
                                      .errnop  = &errnum,
                                      .key     = user,
                                      .klen    = strlen(user),
                                      .tagc    = (unsigned char)'~' };
    /* overload 'group' to pass values in and out of decode routine.
     * nss_mcdb_acct_grouplist_decode() needs to know group gid that will be
     * added to grouplist so that gid can be removed if duplicated in grouplist.
     * The value in 'group' after the call is the number of groups that would
     * be in the list, regardless of whether or not there is enough space. */
    const nss_status_t status = nss_mcdb_get_generic(NSS_DBTYPE_GROUP, &v);
    *ngroups = (status == NSS_STATUS_SUCCESS || status == NSS_STATUS_TRYAGAIN)
             ? (int)group  /* overloaded; value returned is num of groups */
             : 0; /*(indicates error; valid value always >= 1 in *ngroups)*/
    return (status == NSS_STATUS_SUCCESS) ? *ngroups : ((errno = errnum), -1);
}

#ifdef __GLIBC__
#include <assert.h>
#endif

static long sc_ngroups_max_cached = -1L;

nss_status_t
_nss_mcdb_initgroups_dyn(const char * const restrict user,
                         const gid_t group,
                         long int * const restrict start,
                         long int * const restrict size,
                         gid_t ** restrict groupsp,
                         const long int limit,
                         int * const restrict errnop)
{
    /* arbitrary limit: NSS_MCDB_NGROUPS_MAX in nss_mcdb_acct.h */
    const long sc_ngroups_max = (sc_ngroups_max_cached != -1L)
      ? sc_ngroups_max_cached
      : (sc_ngroups_max_cached = sysconf(_SC_NGROUPS_MAX));
    int sz = (0 < sc_ngroups_max && sc_ngroups_max < NSS_MCDB_NGROUPS_MAX)
           ? sc_ngroups_max+1
           : NSS_MCDB_NGROUPS_MAX+1;
    gid_t gidlist[sz];
    if (nss_mcdb_getgrouplist(user, group, gidlist, &sz) == -1) {
        /*(sz == 0 indicates error; valid value always >= 1 in *ngroups)
         *(no differentiation between NOTFOUND and UNAVAIL here; use UNAVAIL)
         *(if db unavailable due to ENOENT, do not mistakenly return NOTFOUND)
         *(TRYAGAIN should not happen (matched limit in nss_mcdb_acct_make.c))*/
        *errnop = errno;
        return (sz != 0 ? NSS_STATUS_TRYAGAIN : NSS_STATUS_UNAVAIL);
    }

    /* nss_mcdb_getgrouplist() success above means sz >=1 */

  #ifdef __GLIBC__
    /* nss_mcdb_acct_grouplist_decode() puts gid passed in list first.
     * If -1, remove from list due to how __GLIBC__ nscd caches initgroups */
    if (group == (gid_t)-1) {
        /*(nss_mcdb_acct_grouplist_decode() puts passed group as first entry) */
        assert(gidlist[0] == (gid_t)-1);
        assert((gid_t)-1 != 65535);  /* by convention 'nobody' */
        gidlist[0] = gidlist[--sz];
    }
  #endif

    /* shortcut if enough space and list is empty or contains only gid */
    if (*size >= sz && ((*start==1 && (*groupsp)[0]==group) || *start==0)) {
        *start = sz;
        memcpy(*groupsp, gidlist, sz * sizeof(gid_t));
        return NSS_STATUS_SUCCESS;
    }
    else { /* fill *groupsp, removing dups */
        gid_t * restrict groups = *groupsp;
        uintptr_t i, j;
        const uintptr_t m = (uintptr_t)*start;  /*(*start must always be >= 0)*/
        const uintptr_t n = (uintptr_t)sz;
        for (j = 0; j < n; ++j) {
            i = 0;
            while (i < m && groups[i] != gidlist[j])
                ++i;
            if (i != m)
                continue;  /* skip duplicate gid */
            if (__builtin_expect( *start == *size, 0)) {
                if (*size == limit)
                    return NSS_STATUS_SUCCESS;
                /* realloc groups, as needed, to limit (no limit if limit <= 0)
                 * reallocate only when adding unique gids require, rather than
                 * preallocating and possibly hitting the limit due to dups */
                sz = (0 < limit && limit < (*size << 1)) ? limit : (*size << 1);
                groups = realloc(groups, sz * sizeof(gid_t));
                if (groups == NULL) /*(realloc failed. oh well. truncate here)*/
                    return NSS_STATUS_SUCCESS;
                *groupsp = groups;
                *size = sz;
            }
            groups[(*start)++] = gidlist[j];  /* add new gid to list */
        }
        return NSS_STATUS_SUCCESS;
    }
}


static nss_status_t
nss_mcdb_acct_passwd_decode(struct mcdb * const restrict m,
                            const struct nss_mcdb_vinfo * restrict v)
{
    struct passwd * const pw = (struct passwd *)v->vstruct;
    char * const buf = v->buf;
    union { uint32_t u[NSS_PW_HDRSZ>>2]; uint16_t h[NSS_PW_HDRSZ>>1]; } hdr;
    const char * const restrict dptr = (char *)mcdb_dataptr(m);
    const size_t dlen = ((size_t)mcdb_datalen(m)) - NSS_PW_HDRSZ;
    if (dlen < v->bufsz) {
        memcpy(buf, dptr+NSS_PW_HDRSZ, dlen);
        buf[dlen] = '\0';
        memcpy(hdr.u, dptr, NSS_PW_HDRSZ);
    }
    else {
        *v->errnop = errno = ERANGE;
        return NSS_STATUS_TRYAGAIN;
    }
    pw->pw_uid    = (uid_t) ntohl( hdr.u[NSS_PW_UID>>2] );
    pw->pw_gid    = (gid_t) ntohl( hdr.u[NSS_PW_GID>>2] );
    pw->pw_name   = buf;
    pw->pw_passwd = buf + ntohs( hdr.h[NSS_PW_PASSWD>>1] );
    pw->pw_gecos  = buf + ntohs( hdr.h[NSS_PW_GECOS>>1] );
    pw->pw_dir    = buf + ntohs( hdr.h[NSS_PW_DIR>>1] );
    pw->pw_shell  = buf + ntohs( hdr.h[NSS_PW_SHELL>>1] );
  #if defined(__sun)
    pw->pw_age    = buf + ntohs( hdr.h[NSS_PW_AGE>>1] );
    pw->pw_comment= buf + ntohs( hdr.h[NSS_PW_COMMENT>>1] );
  #elif defined(__FreeBSD__)
    pw->pw_class  = buf + ntohs( hdr.h[NSS_PW_CLASS>>1] );
    pw->pw_change = ((time_t) ntohl( hdr.u[NSS_PW_CHANGE>>2] ))<<32
                  | ((time_t) ntohl( hdr.u[(NSS_PW_CHANGE>>2)+1] ));
    pw->pw_expire = ((time_t) ntohl( hdr.u[NSS_PW_EXPIRE>>2] ))<<32
                  | ((time_t) ntohl( hdr.u[(NSS_PW_EXPIRE>>2)+1] ));
    pw->pw_fields = (int)     ntohl( hdr.u[NSS_PW_FIELDS>>2] );
  #endif
    return NSS_STATUS_SUCCESS;
}


static nss_status_t
nss_mcdb_acct_group_decode(struct mcdb * const restrict m,
                           const struct nss_mcdb_vinfo * const restrict v)
{
    const char * const restrict dptr = (char *)mcdb_dataptr(m);
    struct group * const gr = (struct group *)v->vstruct;
    char *buf = v->buf;
    size_t gr_mem_num;
    union { uint32_t u[NSS_GR_HDRSZ>>2]; uint16_t h[NSS_GR_HDRSZ>>1]; } hdr;
    memcpy(hdr.u, dptr, NSS_GR_HDRSZ);
    gr->gr_name   = buf;
    gr->gr_passwd = buf + ntohs( hdr.h[NSS_GR_PASSWD>>1] );
    gr->gr_gid    = (gid_t) ntohl( hdr.u[NSS_GR_GID>>2] );
    gr_mem_num    = (size_t)ntohs( hdr.h[NSS_GR_MEM_NUM>>1] );
    gr->gr_mem    = /* align to 8-byte boundary for 64-bit */
      (char **)(((uintptr_t)(buf+ntohs(hdr.h[NSS_GR_MEM>>1])+0x7u)) & ~0x7u);
    /* fill buf, (char **) gr_mem (allow 8-byte ptrs), and terminate strings.
     * scan for '\0' instead of precalculating array because names should
     * be short and adding an extra 4 chars per name to store size takes
     * more space and might take just as long to parse as scan for '\0'
     * (assume data consistent, gr_mem_num correct) */
    if (((char *)gr->gr_mem)-buf+((gr_mem_num+1)<<3) <= v->bufsz) {
        char ** const restrict gr_mem = gr->gr_mem;
        memcpy(buf, dptr+NSS_GR_HDRSZ, mcdb_datalen(m)-NSS_GR_HDRSZ);
        gr_mem[0] = (buf += ntohs(hdr.h[NSS_GR_MEM_STR>>1])); /*gr_mem strings*/
        for (size_t i=1; i<gr_mem_num; ++i) {/*(i=1; assigned first str above)*/
            while (*++buf != '\0')
                ;
            gr_mem[i] = ++buf;
        }
        gr_mem[gr_mem_num] = NULL;         /* terminate (char **) gr_mem array*/
        return NSS_STATUS_SUCCESS;
    }
    else {
        *v->errnop = errno = ERANGE;
        return NSS_STATUS_TRYAGAIN;
    }
}


static nss_status_t
nss_mcdb_acct_grouplist_decode(struct mcdb * const restrict m,
                               const struct nss_mcdb_vinfo * const restrict v)
{
    const char * restrict dptr = (char *)mcdb_dataptr(m);
    gid_t * const gidlist = (gid_t *)v->buf;
    const gid_t gid = *(gid_t *)v->vstruct;
    gid_t n, x;
    union { uint32_t u[NSS_GL_HDRSZ>>2]; uint16_t h[NSS_GL_HDRSZ>>1]; } hdr;
    memcpy(hdr.u, dptr, NSS_GL_HDRSZ);
    dptr += NSS_GL_HDRSZ;
    n = 1 + (gid_t) ntohl( hdr.u[NSS_GL_NGROUPS>>2] );
    /* populate gidlist if enough space, else count gids */
    if (__builtin_expect( n > v->bufsz, 0)) { /* not enough space */
        /* count num gids (as if enough space were available) */
        for (x = 1; --n; dptr += 4) {
            if (gid !=(gid_t)((dptr[0]<<24)|(dptr[1]<<16)|(dptr[2]<<8)|dptr[3]))
                ++x;
        }
        if (__builtin_expect( x > v->bufsz, 1)) { /* not enough space */
            *(gid_t *)v->vstruct = x;
            *v->errnop = errno = ERANGE;
            return NSS_STATUS_TRYAGAIN;
        }
        /*(x == v->bufsz; rare case: 1 gid more than space avail, but dup gid)*/
        n = 1+x;
        dptr -= (x << 2);
    }
    gidlist[0] = gid;
    for (x = 1; --n; dptr += 4) {
        gidlist[x] = (gid_t)((dptr[0]<<24)|(dptr[1]<<16)|(dptr[2]<<8)|dptr[3]);
        if (gidlist[x] != gid)
            ++x;
    }
    *(gid_t *)v->vstruct = x;
    return NSS_STATUS_SUCCESS;
}
