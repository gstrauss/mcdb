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
 * man passwd(5) getpwnam getpwuid setpwent getpwent endpwent
 *     /etc/passwd
 * man group(5)  getgrnam getgrgid setgrent getgrent endgrent
 *     /etc/group
 * man shadow(5) getspnam
 *     /etc/shadow
 */

static enum nss_status
_nss_mcdb_decode_passwd(struct mcdb * restrict,
                        const struct _nss_kinfo * restrict,
                        const struct _nss_vinfo * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;

static enum nss_status
_nss_mcdb_decode_group(struct mcdb * restrict,
                       const struct _nss_kinfo * restrict,
                       const struct _nss_vinfo * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;

static enum nss_status
_nss_mcdb_decode_spwd(struct mcdb * restrict,
                      const struct _nss_kinfo * restrict,
                      const struct _nss_vinfo * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;


void _nss_files_setpwent(void) { _nss_mcdb_setent(NSS_DBTYPE_PASSWD); }
void _nss_files_endpwent(void) { _nss_mcdb_endent(NSS_DBTYPE_PASSWD); }
void _nss_files_setgrent(void) { _nss_mcdb_setent(NSS_DBTYPE_GROUP);  }
void _nss_files_endgrent(void) { _nss_mcdb_endent(NSS_DBTYPE_GROUP);  }
void _nss_files_setspent(void) { _nss_mcdb_setent(NSS_DBTYPE_SHADOW); }
void _nss_files_endspent(void) { _nss_mcdb_endent(NSS_DBTYPE_SHADOW); }


enum nss_status
_nss_files_getpwent_r(struct passwd * const restrict pwbuf,
                      char * const restrict buf, const size_t buflen,
                      struct passwd ** const restrict pwbufp)
{
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_passwd,
                                      .vstruct = pwbuf,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .vstructp= pwbufp };
    *pwbufp = NULL;
    return _nss_mcdb_getent(NSS_DBTYPE_PASSWD, &vinfo);
}

enum nss_status
_nss_files_getpwnam_r(const char * const restrict name,
                      struct passwd * const restrict pwbuf,
                      char * const restrict buf, const size_t buflen,
                      struct passwd ** const restrict pwbufp)
{
    const struct _nss_kinfo kinfo = { .key     = name,
                                      .klen    = strlen(name),
                                      .tagc    = (unsigned char)'=' };
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_passwd,
                                      .vstruct = pwbuf,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .vstructp= pwbufp };
    *pwbufp = NULL;
    return _nss_mcdb_get_generic(NSS_DBTYPE_PASSWD, &kinfo, &vinfo);
}

enum nss_status
_nss_files_getpwuid_r(const uid_t uid,
                      struct passwd * const restrict pwbuf,
                      char * const restrict buf, const size_t buflen,
                      struct passwd ** const restrict pwbufp)
{
    char hexstr[8];
    const struct _nss_kinfo kinfo = { .key     = hexstr,
                                      .klen    = sizeof(hexstr),
                                      .tagc    = (unsigned char)'x' };
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_passwd,
                                      .vstruct = pwbuf,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .vstructp= pwbufp };
    *pwbufp = NULL;
    uint32_to_ascii8uphex((uint32_t)uid, hexstr);
    return _nss_mcdb_get_generic(NSS_DBTYPE_PASSWD, &kinfo, &vinfo);
}


enum nss_status
_nss_files_getgrent_r(struct group * const restrict grbuf,
                      char * const restrict buf, const size_t buflen,
                      struct group ** const restrict grbufp)
{
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_group,
                                      .vstruct = grbuf,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .vstructp= grbufp };
    *grbufp = NULL;
    return _nss_mcdb_getent(NSS_DBTYPE_GROUP, &vinfo);
}

enum nss_status
_nss_files_getgrnam_r(const char * const restrict name,
                      struct group * const restrict grbuf,
                      char * const restrict buf, const size_t buflen,
                      struct group ** const restrict grbufp)
{
    const struct _nss_kinfo kinfo = { .key     = name,
                                      .klen    = strlen(name),
                                      .tagc    = (unsigned char)'=' };
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_group,
                                      .vstruct = grbuf,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .vstructp= grbufp };
    *grbufp = NULL;
    return _nss_mcdb_get_generic(NSS_DBTYPE_GROUP, &kinfo, &vinfo);
}

enum nss_status
_nss_files_getgrgid_r(const gid_t gid,
                      struct group * const restrict grbuf,
                      char * const restrict buf, const size_t buflen,
                      struct group ** const restrict grbufp)
{
    char hexstr[8];
    const struct _nss_kinfo kinfo = { .key     = hexstr,
                                      .klen    = sizeof(hexstr),
                                      .tagc    = (unsigned char)'x' };
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_group,
                                      .vstruct = grbuf,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .vstructp= grbufp };
    *grbufp = NULL;
    uint32_to_ascii8uphex((uint32_t)gid, hexstr);
    return _nss_mcdb_get_generic(NSS_DBTYPE_GROUP, &kinfo, &vinfo);
}


enum nss_status
_nss_files_getspent_r(struct spwd * const restrict spbuf,
                      char * const restrict buf, const size_t buflen,
                      struct spwd ** const restrict spbufp)
{
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_spwd,
                                      .vstruct = spbuf,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .vstructp= spbufp };
    *spbufp = NULL;
    return _nss_mcdb_getent(NSS_DBTYPE_SHADOW, &vinfo);
}

enum nss_status
_nss_files_getspnam_r(const char * const restrict name,
                      struct spwd * const restrict spbuf,
                      char * const restrict buf, const size_t buflen,
                      struct spwd ** const restrict spbufp)
{
    const struct _nss_kinfo kinfo = { .key     = name,
                                      .klen    = strlen(name),
                                      .tagc    = (unsigned char)'=' };
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_spwd,
                                      .vstruct = spbuf,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .vstructp= spbufp };
    *spbufp = NULL;
    return _nss_mcdb_get_generic(NSS_DBTYPE_SHADOW, &kinfo, &vinfo);
}


static enum nss_status
_nss_mcdb_decode_passwd(struct mcdb * restrict m,
                        const struct _nss_kinfo * restrict kinfo,
                        const struct _nss_vinfo * restrict vinfo)
{
    /* TODO */
    return NSS_STATUS_SUCCESS;
}


static enum nss_status
_nss_mcdb_decode_group(struct mcdb * restrict m,
                       const struct _nss_kinfo * restrict kinfo,
                       const struct _nss_vinfo * restrict vinfo)
{
    /* TODO */
    return NSS_STATUS_SUCCESS;
}


static enum nss_status
_nss_mcdb_decode_spwd(struct mcdb * restrict m,
                      const struct _nss_kinfo * restrict kinfo,
                      const struct _nss_vinfo * restrict vinfo)
{
    /* TODO */
    return NSS_STATUS_SUCCESS;
}
