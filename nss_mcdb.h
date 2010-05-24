#ifndef INCLUDED_NSS_MCDB_H
#define INCLUDED_NSS_MCDB_H

#include "mcdb.h"
#include "mcdb_attribute.h"

#include <nss.h>    /* NSS_STATUS_{TRYAGAIN,UNAVAIL,NOTFOUND,SUCCESS,RETURN} */

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

/* might remove union _nss_ent and union _nss_entp for better encapsulation */
#include <pwd.h>
#include <grp.h>
#include <netdb.h>
#include <aliases.h>
#include <shadow.h>
#include <netinet/ether.h>

union _nss_ent {
  struct passwd     * restrict passwd;
  struct group      * restrict group;
  struct hostent    * restrict hostent;
  struct netent     * restrict netent;
  struct protoent   * restrict protoent;
  struct rpcent     * restrict rpcent;
  struct servent    * restrict servent;
  struct aliasent   * restrict aliasent;
  struct spwd       * restrict spwd;
  struct ether_addr * restrict ether_addr;
  void              * restrict extension;
  uintptr_t         NA;
};

union _nss_entp {
  struct passwd     ** restrict passwd;
  struct group      ** restrict group;
  struct hostent    ** restrict hostent;
  struct netent     ** restrict netent;
  struct protoent   ** restrict protoent;
  struct rpcent     ** restrict rpcent;
  struct servent    ** restrict servent;
  struct aliasent   ** restrict aliasent;
  struct spwd       ** restrict spwd;
  struct ether_addr ** restrict ether_addr;
  void              ** restrict extension;
  uintptr_t         NA;
};

struct _nss_kinfo {
  const char * restrict key;
  size_t klen;
  unsigned char tagc;
};

struct _nss_vinfo {
  /* fail with errno = ERANGE if insufficient buf space supplied */
  enum nss_status (*decode)(struct mcdb * restrict,
                            const struct _nss_kinfo * restrict,
                            const struct _nss_vinfo * restrict);
  union _nss_ent u;
  char * restrict buf;
  size_t buflen;
  union _nss_entp p;
};


char *  __attribute_noinline__
uint32_to_ascii8uphex(const uint32_t n, char * restrict buf)
  __attribute_nonnull__;
char *  __attribute_noinline__
uint16_to_ascii4uphex(const uint32_t n, char * restrict buf)
  __attribute_nonnull__;



enum nss_status
_nss_mcdb_setent(const enum nss_dbtype dbtype)
  __attribute_nonnull__;
enum nss_status
_nss_mcdb_endent(const enum nss_dbtype dbtype)
  __attribute_nonnull__;
enum nss_status
/* mcdb get*ent() walks db returning successive keys with '=' tag char */
_nss_mcdb_getent(const enum nss_dbtype dbtype,
                 const struct _nss_vinfo * const restrict vinfo)
  __attribute_nonnull__  __attribute_warn_unused_result__;


enum nss_status
_nss_mcdb_get_generic(const enum nss_dbtype dbtype,
                      const struct _nss_kinfo * const restrict kinfo,
                      const struct _nss_vinfo * const restrict vinfo)
  __attribute_nonnull__  __attribute_warn_unused_result__;
enum nss_status
_nss_mcdb_decode_buf(struct mcdb * restrict,
                     const struct _nss_kinfo * restrict,
                     const struct _nss_vinfo * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;

#endif
