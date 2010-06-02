#ifndef INCLUDED_NSS_MCDB_H
#define INCLUDED_NSS_MCDB_H

#include "mcdb.h"
#include "code_attributes.h"

#ifdef __linux
#include <nss.h>    /* NSS_STATUS_{TRYAGAIN,UNAVAIL,NOTFOUND,SUCCESS,RETURN} */
typedef enum nss_status nss_status_t;
#else
#include <nss_common.h> /* NSS_{TRYAGAIN,UNAVAIL,NOTFOUND,SUCCESS,RETURN} */
#define NSS_STATUS_SUCCESS  NSS_SUCCESS
#define NSS_STATUS_NOTFOUND NSS_NOTFOUND
#define NSS_STATUS_TRYAGAIN NSS_TRYAGAIN
#define NSS_STATUS_UNAVAIL  NSS_UNAVAIL
#define NSS_STATUS_RETURN   NSS_NOTFOUND
#endif

/* enum nss_dbtype index must match path in nss_mcdb.c char *_nss_dbnames[] */
enum nss_dbtype {
    NSS_DBTYPE_ALIASES = 0,
    NSS_DBTYPE_ETHERS,
    NSS_DBTYPE_GROUP,
    NSS_DBTYPE_HOSTS,
    NSS_DBTYPE_NETGROUP,
    NSS_DBTYPE_NETWORKS,
    NSS_DBTYPE_PASSWD,
    NSS_DBTYPE_PROTOCOLS,
    NSS_DBTYPE_PUBLICKEY,
    NSS_DBTYPE_RPC,
    NSS_DBTYPE_SERVICES,
    NSS_DBTYPE_SHADOW
};

/* To maintain a consistent view of a database, set*ent() opens a session to
 * the database and it is held open until end*ent() is called.  This session
 * is maintained in thread-local storage.  get*() queries other than get*ent()
 * obtain and release a session to the database upon every call.  To avoid the
 * locking overhead, thread can call set*ent() and then can make (many) get*()
 * calls in the same session.  When done, end*ent() should be called to release
 * the session. */

struct _nss_kinfo {
  const char * const restrict key;
  const size_t klen;
  const unsigned char tagc;
};

struct _nss_vinfo {
  /* fail with errno = ERANGE if insufficient buf space supplied */
  nss_status_t (* const decode)(struct mcdb * restrict,
                                const struct _nss_kinfo * restrict,
                                const struct _nss_vinfo * restrict);
  void * const restrict vstruct;
  char * const restrict buf;
  const size_t buflen;
  int  * const errnop;
};


nss_status_t
_nss_mcdb_setent(const enum nss_dbtype)
  __attribute_nonnull__;

nss_status_t
_nss_mcdb_endent(const enum nss_dbtype)
  __attribute_nonnull__;

nss_status_t
/* mcdb get*ent() walks db returning successive keys with '=' tag char */
_nss_mcdb_getent(const enum nss_dbtype,
                 const struct _nss_vinfo * const restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;


nss_status_t
_nss_mcdb_get_generic(const enum nss_dbtype,
                      const struct _nss_kinfo * const restrict,
                      const struct _nss_vinfo * const restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;

nss_status_t
_nss_mcdb_decode_buf(struct mcdb * const restrict,
                     const struct _nss_kinfo * const restrict,
                     const struct _nss_vinfo * const restrict)
  __attribute_nonnull_x__((1,3))  __attribute_warn_unused_result__;


#endif
