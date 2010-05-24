/* fstatat(), openat() */
#define _ATFILE_SOURCE
/* _BSD_SOURCE or _SVID_SOURCE needed for struct rpcent on Linux */
#define _BSD_SOURCE

#include "mcdb.h"
#include "mcdb_uint32.h"
#include "mcdb_attribute.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <nss.h>    /* NSS_STATUS_{TRYAGAIN,UNAVAIL,NOTFOUND,SUCCESS,RETURN} */

#include <pwd.h>
#include <grp.h>
#include <netdb.h>
#include <sys/socket.h>    /* AF_INET */
#include <aliases.h>
#include <shadow.h>
#include <netinet/ether.h>
#include <netinet/in.h>    /* htonl() htons() ntohl() ntohs() */
#include <arpa/inet.h>     /* htonl() htons() ntohl() ntohs() inet_pton() */

#ifdef _THREAD_SAFE
#include <pthread.h>       /* pthread_mutex_t, pthread_mutex_{lock,unlock}() */
#else
#define pthread_mutex_lock(mutexp) 0
#define pthread_mutex_unlock(mutexp) (void)0
#endif

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

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

/* compile-time setting for security */
#ifndef NSS_DBPATH
#define NSS_DBPATH "/var/db/"
#endif

/* path to db must match up to enum nss_dbtype index */
/* NOTE: string must fit into map->fname array (currently 64 bytes) */
static const char * const restrict _nss_dbnames[] = {
    "aliases.mcdb",
    "ethers.mcdb",
    "group.mcdb",
    "hosts.mcdb",
    "netgroup.mcdb",
    "networks.mcdb",
    "passwd.mcdb",
    "protocols.mcdb",
    "publickey.mcdb",
    "rpc.mcdb",
    "services.mcdb",
    "shadow.mcdb"
};

#define _nss_num_dbs (sizeof(_nss_dbnames)/sizeof(char *))

static struct mcdb_mmap _nss_mcdb_mmap_st[_nss_num_dbs];
static struct mcdb_mmap *_nss_mcdb_mmap[_nss_num_dbs];

/* set*ent(), get*ent(), end*ent() are not thread-safe */
/* (use thread-local storage of static struct mcdb array to increase safety) */

static __thread struct mcdb _nss_mcdb_st[_nss_num_dbs];

/* To maintain a consistent view of a database, set*ent() opens a session to
 * the database and it is held open until end*ent() is called.  This session
 * is maintained in thread-local storage.  get*() queries other than get*ent()
 * obtain and release a session to the database upon every call.  To avoid the
 * locking overhead, thread can call set*ent() and then can make (many) get*()
 * calls in the same session.  When done, end*ent() should be called to release
 * the session. */


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


/* TODO take pipelined versions from cdbauthz.c */
static char *  __attribute_noinline__
uint32_to_ascii8uphex(const uint32_t n, char * restrict buf)
  __attribute_nonnull__;
static char *  __attribute_noinline__
uint32_to_ascii8uphex(const uint32_t n, char * restrict buf)
{
    uint32_t nibble;
    uint32_t i = 0;
    do {
        nibble = (n << (i<<2)) >> 28; /* isolate 4 bits */
        *buf++ = nibble + (nibble < 10 ? '0': 'A'-10);
    } while (++i < 8);
    return buf;
}
static char *  __attribute_noinline__
uint16_to_ascii4uphex(const uint32_t n, char * restrict buf)
  __attribute_nonnull__;
static char *  __attribute_noinline__
uint16_to_ascii4uphex(const uint32_t n, char * restrict buf)
{
    uint32_t nibble;
    uint32_t i = 4;
    do {
        nibble = (n << (i<<2)) >> 28; /* isolate 4 bits */
        *buf++ = nibble + (nibble < 10 ? '0': 'A'-10);
    } while (++i < 8);
    return buf;
}


/* custom free for mcdb_mmap_create() to not free initial static storage */
static void  __attribute_noinline__
nss_mcdb_mmap_free(void * const v)
{
    struct mcdb_mmap * const m = v;
    if (&_nss_mcdb_mmap_st[_nss_num_dbs] <= m || m < &_nss_mcdb_mmap_st[0])
        free(v);
}

/* (similar to mcdb_mmap_create() but mutex-protected, shared dir fd,
 *  and use static storage for initial struct mcdb_mmap for each dbtype)
 */
static bool  __attribute_noinline__
_nss_mcdb_db_openshared(const enum nss_dbtype dbtype)
  __attribute_cold__  __attribute_warn_unused_result__;
static bool  __attribute_noinline__
_nss_mcdb_db_openshared(const enum nss_dbtype dbtype)
{
  #ifdef _THREAD_SAFE
    static pthread_mutex_t _nss_mcdb_global_mutex = PTHREAD_MUTEX_INITIALIZER;
  #endif
    static int dfd = -1;
    struct mcdb_mmap * const restrict map = &_nss_mcdb_mmap_st[dbtype];
    int fd;
    bool rc = false;

    if (pthread_mutex_lock(&_nss_mcdb_global_mutex) != 0)
        return false;

    if (_nss_mcdb_mmap[dbtype] != 0) {
        /* init'ed by another thread while waiting for mutex */
        pthread_mutex_unlock(&_nss_mcdb_global_mutex);
        return true;
    }

    map->fn_malloc = malloc;
    map->fn_free   = nss_mcdb_mmap_free;

  #if defined(__linux) || defined(__sun)
    if (__builtin_expect(dfd <= STDERR_FILENO, false)) {
        if ((dfd = open(NSS_DBPATH, O_RDONLY | O_CLOEXEC, 0)) > STDERR_FILENO) {
      #if O_CLOEXEC == 0
        (void) fcntl(dfd, F_SETFD, FD_CLOEXEC);
      #endif
        }
        else {
            if (dfd != -1) /* caller must have open STDIN, STDOUT, STDERR */
                while (close(dfd) != 0 && errno == EINTR)
                    ;
            pthread_mutex_unlock(&_nss_mcdb_global_mutex);
            return false;
        }
    }
    /* assert(sizeof(map->fname) > strlen(_nss_dbnames[dbtype])); */
    memcpy(map->fname, _nss_dbnames[dbtype], strlen(_nss_dbnames[dbtype])+1);
  #else
    if (snprintf(map->fname, sizeof(map->fname), "%s%s",
                 NSS_DBPATH, _nss_dbnames[dbtype]) >= sizeof(map->fname)) {
        pthread_mutex_unlock(&_nss_mcdb_global_mutex);
        return false;
    }
  #endif

  #if defined(__linux) || defined(__sun)
    if ((fd = openat((map->dfd=dfd), map->fname, O_RDONLY|O_NONBLOCK, 0)) != -1)
  #else
    if ((fd = open(map->fname, O_RDONLY|O_NONBLOCK, 0)) != -1)
  #endif
    {
        rc = mcdb_mmap_init(map, fd);
        while (close(fd) != 0 && errno == EINTR) /* close fd once mmap'ed */
            ;
        if (rc)      /*(ought to be preceded by StoreStore memory barrier)*/
            _nss_mcdb_mmap[dbtype] = map;
    }

    pthread_mutex_unlock(&_nss_mcdb_global_mutex);
    return rc;
}

/* Flag to skip mcdb stat() check for protocols, rpc, services dbs.
 * (similar to set*ent(int stayopen) flag in glibc, but default enabled here)
 * (protocols, rpc, services are unlikely to change frequently in most configs)
 * (future: might provide accessor routine to enable/disable) */
static bool _nss_mcdb_stayopen = true;

/* release shared mcdb_mmap */
#define _nss_mcdb_db_relshared(map) mcdb_unregister(map)

/* get shared mcdb_mmap */
static struct mcdb_mmap *  __attribute_noinline__
_nss_mcdb_db_getshared(const enum nss_dbtype dbtype)
  __attribute_nonnull__  __attribute_warn_unused_result__;
static struct mcdb_mmap *  __attribute_noinline__
_nss_mcdb_db_getshared(const enum nss_dbtype dbtype)
{
    /* reuse set*ent(),get*ent(),end*end() session if open in current thread */
    if (_nss_mcdb_st[dbtype].map != NULL)
        return _nss_mcdb_st[dbtype].map;

    /* check if db is open and up-to-date, or else open db
     * (continue with open database if failure refreshing) */
    if (__builtin_expect(_nss_mcdb_mmap[dbtype] != 0, true)) {
        /* protocols, rpc, services unlikely to change often in most configs
         * Therefore, skip stat() check if configured (default skips stat()) */
        switch (dbtype) {
          case NSS_DBTYPE_PROTOCOLS:
          case NSS_DBTYPE_RPC:
          case NSS_DBTYPE_SERVICES: if (_nss_mcdb_stayopen) break;
          default: (void) mcdb_mmap_refresh(_nss_mcdb_mmap[dbtype]); break;
        }
    }
    else if (!_nss_mcdb_db_openshared(dbtype))
        return NULL;

    return mcdb_register(_nss_mcdb_mmap[dbtype])
      ? _nss_mcdb_mmap[dbtype]
      : NULL;
}

static enum nss_status
_nss_mcdb_setent(const enum nss_dbtype dbtype)
  __attribute_nonnull__;
static enum nss_status
_nss_mcdb_setent(const enum nss_dbtype dbtype)
{
    struct mcdb * const restrict m = &_nss_mcdb_st[dbtype];
    if (m->map != NULL || (m->map = _nss_mcdb_db_getshared(dbtype)) != NULL) {
        m->hpos = 0;
        return NSS_STATUS_SUCCESS;
    }
    return NSS_STATUS_UNAVAIL;
}

static enum nss_status
_nss_mcdb_endent(const enum nss_dbtype dbtype)
  __attribute_nonnull__;
static enum nss_status
_nss_mcdb_endent(const enum nss_dbtype dbtype)
{
    struct mcdb * const restrict m = &_nss_mcdb_st[dbtype];
    return (m->map == NULL || _nss_mcdb_db_relshared(m->map))
      ? NSS_STATUS_SUCCESS
      : NSS_STATUS_TRYAGAIN; /* (fails only if obtaining mutex fails) */
}

/* mcdb get*ent() walks db returning successive keys with '=' tag char */
static enum nss_status
_nss_mcdb_getent(const enum nss_dbtype dbtype,
                 const struct _nss_vinfo * const restrict vinfo)
  __attribute_nonnull__  __attribute_warn_unused_result__;
static enum nss_status
_nss_mcdb_getent(const enum nss_dbtype dbtype,
                 const struct _nss_vinfo * const restrict vinfo)
{
    struct mcdb * const restrict m = &_nss_mcdb_st[dbtype];
    unsigned char *map;
    uint32_t eod;
    uint32_t klen;
    if (__builtin_expect(m->map == NULL, false)
        && _nss_mcdb_setent(dbtype) != NSS_STATUS_SUCCESS)
        return NSS_STATUS_UNAVAIL;
    map = m->map->ptr;
    eod = mcdb_uint32_unpack_bigendian_aligned_macro(map) - 7;
    while (m->hpos < eod) {
        unsigned char * const restrict p = map + m->hpos;
        klen    = mcdb_uint32_unpack_bigendian_macro(p);
        m->dlen = mcdb_uint32_unpack_bigendian_macro(p+4);
        m->hpos = (m->dpos = (m->kpos = m->hpos + 8) + klen) + m->dlen;
        if (p[8+klen] == (unsigned char)'=')
            /* valid data in mcdb_datapos() mcdb_datalen() mcdb_dataptr() */
            return vinfo->decode(m, NULL, vinfo);
    }
    return NSS_STATUS_NOTFOUND;
}

static enum nss_status
_nss_mcdb_get_generic(const enum nss_dbtype dbtype,
                      const struct _nss_kinfo * const restrict kinfo,
                      const struct _nss_vinfo * const restrict vinfo)
  __attribute_nonnull__  __attribute_warn_unused_result__;
static enum nss_status
_nss_mcdb_get_generic(const enum nss_dbtype dbtype,
                      const struct _nss_kinfo * const restrict kinfo,
                      const struct _nss_vinfo * const restrict vinfo)
{
    struct mcdb m;
    enum nss_status status = NSS_STATUS_NOTFOUND;

    m.map = _nss_mcdb_db_getshared(dbtype);
    if (__builtin_expect(m.map == NULL, false))
        return NSS_STATUS_UNAVAIL;

    if (  mcdb_findtagstart(&m, kinfo->key, kinfo->klen, kinfo->tagc)
        && mcdb_findtagnext(&m, kinfo->key, kinfo->klen, kinfo->tagc)  )
        status = vinfo->decode(&m, kinfo, vinfo);

    _nss_mcdb_db_relshared(m.map);
    return status;
}


/* _nss_files_setaliasent()   _nss_files_endaliasent() */
/* _nss_files_setetherent()   _nss_files_endetherent() */
/* _nss_files_setgrent()      _nss_files_endgrent()    */
/* _nss_files_sethostent()    _nss_files_endhostent()  */
/* _nss_files_setnetgrent()   _nss_files_endnetgrent() */
/* _nss_files_setnetent()     _nss_files_endnetent()   */
/* _nss_files_setpwent()      _nss_files_endpwent()    */
/* _nss_files_setprotoent()   _nss_files_endprotoent() */
/* _nss_files_setrpcent()     _nss_files_endrpcent()   */
/* _nss_files_setservent()    _nss_files_endservent()  */
/* _nss_files_setspent()      _nss_files_endspent()    */

#define _nss_files_xxxNAMEent(name, dbtype)                          \
  void _nss_files_set##name##ent(void) { _nss_mcdb_setent(dbtype); } \
  void _nss_files_end##name##ent(void) { _nss_mcdb_endent(dbtype); }


_nss_files_xxxNAMEent(alias, NSS_DBTYPE_ALIASES)
_nss_files_xxxNAMEent(ether, NSS_DBTYPE_ETHERS)
_nss_files_xxxNAMEent(gr,    NSS_DBTYPE_GROUP)
_nss_files_xxxNAMEent(host,  NSS_DBTYPE_HOSTS)
_nss_files_xxxNAMEent(netgr, NSS_DBTYPE_NETGROUP)
_nss_files_xxxNAMEent(net,   NSS_DBTYPE_NETWORKS)
_nss_files_xxxNAMEent(pw,    NSS_DBTYPE_PASSWD)
_nss_files_xxxNAMEent(proto, NSS_DBTYPE_PROTOCOLS)
_nss_files_xxxNAMEent(rpc,   NSS_DBTYPE_RPC)
_nss_files_xxxNAMEent(serv,  NSS_DBTYPE_SERVICES)
_nss_files_xxxNAMEent(sp,    NSS_DBTYPE_SHADOW)



static enum nss_status
_nss_mcdb_decode_buf(struct mcdb * restrict,
                     const struct _nss_kinfo * restrict,
                     const struct _nss_vinfo * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;
static enum nss_status
_nss_mcdb_decode_buf(struct mcdb * const restrict m,
                     const struct _nss_kinfo * const restrict kinfo
                       __attribute_unused__,
                     const struct _nss_vinfo * const restrict vinfo)
{   /* generic; simply copy data into target buffer and NIL terminate string */
    const size_t dlen = mcdb_datalen(m);
    if (vinfo->buflen > dlen) {
        memcpy(vinfo->buf, mcdb_dataptr(m), dlen);
        vinfo->buf[dlen] = '\0';
        return NSS_STATUS_SUCCESS;
    }
    return NSS_STATUS_TRYAGAIN;
}

/* GPS: these all should be static (once we define them) */

enum nss_status
_nss_mcdb_decode_passwd(struct mcdb * restrict,
                        const struct _nss_kinfo * restrict,
                        const struct _nss_vinfo * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;
enum nss_status
_nss_mcdb_decode_group(struct mcdb * restrict,
                       const struct _nss_kinfo * restrict,
                       const struct _nss_vinfo * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;
enum nss_status
_nss_mcdb_decode_hostent(struct mcdb * restrict,
                         const struct _nss_kinfo * restrict,
                         const struct _nss_vinfo * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;
  /* TODO: check type match from buf: mcdb_findtagnext() until match
   * buf[0-3] = 0 and take first match */
enum nss_status
_nss_mcdb_decode_netent(struct mcdb * restrict,
                        const struct _nss_kinfo * restrict,
                        const struct _nss_vinfo * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;
enum nss_status
_nss_mcdb_decode_protoent(struct mcdb * restrict,
                          const struct _nss_kinfo * restrict,
                          const struct _nss_vinfo * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;
enum nss_status
_nss_mcdb_decode_rpcent(struct mcdb * restrict,
                        const struct _nss_kinfo * restrict,
                        const struct _nss_vinfo * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;
enum nss_status
_nss_mcdb_decode_servent(struct mcdb * restrict,
                         const struct _nss_kinfo * restrict,
                         const struct _nss_vinfo * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;
  /* TODO: check proto in buf != "" and mcdb_findtagnext() until match 
   *   (buf = "" and take first match) */
enum nss_status
_nss_mcdb_decode_aliasent(struct mcdb * restrict,
                          const struct _nss_kinfo * restrict,
                          const struct _nss_vinfo * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;
enum nss_status
_nss_mcdb_decode_spwd(struct mcdb * restrict,
                      const struct _nss_kinfo * restrict,
                      const struct _nss_vinfo * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;
enum nss_status
_nss_mcdb_decode_ether_addr(struct mcdb * restrict,
                            const struct _nss_kinfo * restrict,
                            const struct _nss_vinfo * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;
    /* TODO: decode into struct ether_addr and hostname in buf, if not NULL */


enum nss_status
_nss_files_getpwent_r(struct passwd * const restrict pwbuf,
                      char * const restrict buf, const size_t buflen,
                      struct passwd ** const restrict pwbufp)
{
    const struct _nss_vinfo vinfo = { .decode = _nss_mcdb_decode_passwd,
                                      .u      = { .passwd = pwbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .passwd = pwbufp } };
    *pwbufp = NULL;
    return _nss_mcdb_getent(NSS_DBTYPE_PASSWD, &vinfo);
}

enum nss_status
_nss_files_getpwnam_r(const char * const restrict name,
                      struct passwd * const restrict pwbuf,
                      char * const restrict buf, const size_t buflen,
                      struct passwd ** const restrict pwbufp)
{
    const struct _nss_kinfo kinfo = { .key    = name,
                                      .klen   = strlen(name),
                                      .tagc   = (unsigned char)'=' };
    const struct _nss_vinfo vinfo = { .decode = _nss_mcdb_decode_passwd,
                                      .u      = { .passwd = pwbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .passwd = pwbufp } };
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
    const struct _nss_kinfo kinfo = { .key    = hexstr,
                                      .klen   = sizeof(hexstr),
                                      .tagc   = (unsigned char)'x' };
    const struct _nss_vinfo vinfo = { .decode = _nss_mcdb_decode_passwd,
                                      .u      = { .passwd = pwbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .passwd = pwbufp } };
    *pwbufp = NULL;
    uint32_to_ascii8uphex((uint32_t)uid, hexstr);
    return _nss_mcdb_get_generic(NSS_DBTYPE_PASSWD, &kinfo, &vinfo);
}


enum nss_status
_nss_files_getgrent_r(struct group * const restrict grbuf,
                      char * const restrict buf, const size_t buflen,
                      struct group ** const restrict grbufp)
{
    const struct _nss_vinfo vinfo = { .decode = _nss_mcdb_decode_group,
                                      .u      = { .group = grbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .group = grbufp } };
    *grbufp = NULL;
    return _nss_mcdb_getent(NSS_DBTYPE_GROUP, &vinfo);
}

enum nss_status
_nss_files_getgrnam_r(const char * const restrict name,
                      struct group * const restrict grbuf,
                      char * const restrict buf, const size_t buflen,
                      struct group ** const restrict grbufp)
{
    const struct _nss_kinfo kinfo = { .key    = name,
                                      .klen   = strlen(name),
                                      .tagc   = (unsigned char)'=' };
    const struct _nss_vinfo vinfo = { .decode = _nss_mcdb_decode_group,
                                      .u      = { .group = grbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .group = grbufp } };
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
    const struct _nss_kinfo kinfo = { .key    = hexstr,
                                      .klen   = sizeof(hexstr),
                                      .tagc   = (unsigned char)'x' };
    const struct _nss_vinfo vinfo = { .decode = _nss_mcdb_decode_group,
                                      .u      = { .group = grbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .group = grbufp } };
    *grbufp = NULL;
    uint32_to_ascii8uphex((uint32_t)gid, hexstr);
    return _nss_mcdb_get_generic(NSS_DBTYPE_GROUP, &kinfo, &vinfo);
}


static enum nss_status  __attribute_noinline__
gethost_fill_h_errnop(enum nss_status status, int * const restrict h_errnop)
{
    switch (status) {
      case NSS_STATUS_TRYAGAIN: *h_errnop = TRY_AGAIN; break;
      case NSS_STATUS_UNAVAIL:  *h_errnop = NO_RECOVERY; break;
      case NSS_STATUS_NOTFOUND: *h_errnop = HOST_NOT_FOUND; break;
      case NSS_STATUS_SUCCESS:  break;
      case NSS_STATUS_RETURN:   *h_errnop = TRY_AGAIN; break;
    }
    return status;
}

static enum nss_status
gethost_query(const int type,
              const struct _nss_kinfo * restrict kinfo,
              const struct _nss_vinfo * restrict vinfo,
              int * const restrict h_errnop)
{
    enum nss_status status;
    if (vinfo->buflen >= 4) {
        /*copy type for later match in _nss_mcdb_decode_hostent()*/
        char * const restrict buf = vinfo->buf;
        buf[0] = (((uint32_t)type)              ) >> 24;
        buf[1] = (((uint32_t)type) & 0x00FF0000u) >> 16;
        buf[2] = (((uint32_t)type) & 0x0000FF00u) >>  8;
        buf[3] = (((uint32_t)type) & 0x000000FFu);
        status = _nss_mcdb_get_generic(NSS_DBTYPE_HOSTS, kinfo, vinfo);
    }
    else {
        errno = ERANGE;
        status = NSS_STATUS_TRYAGAIN;
    }
    return (status == NSS_STATUS_SUCCESS)
      ? NSS_STATUS_SUCCESS
      : gethost_fill_h_errnop(status, h_errnop);
}

static enum nss_status
gethost_filladdr(const void * const restrict addr, const int type,
                 const struct _nss_kinfo * restrict kinfo,
                 const struct _nss_vinfo * restrict vinfo,
                 int * const restrict h_errnop)
{
    struct hostent * const restrict hostbuf = vinfo->u.hostent;
    char * const restrict buf = vinfo->buf;
    char * const aligned = (char *)((uintptr_t)(buf+7) & ~0x7);
    /* supports only AF_INET and AF_INET6
     * (future: create static array of addr sizes indexed by AF_*) */
    int h_length;
    switch (type) {
      case AF_INET:  h_length = sizeof(struct in_addr);  break;
      case AF_INET6: h_length = sizeof(struct in6_addr); break;
      default: return gethost_fill_h_errnop(NSS_STATUS_RETURN, h_errnop);
    }          /* other addr types not implemented */
    /* (pointers in struct hostent must be aligned; align to 8 bytes) */
    /* (align+(char **h_aliases)+(char **h_addr_list)+h_length+addrstr+'\0') */
    if ((aligned - buf) + 8 + 8 + h_length + kinfo->klen + 1 >= vinfo->buflen)
        return gethost_fill_h_errnop(NSS_STATUS_TRYAGAIN, h_errnop);
    hostbuf->h_name      = memcpy(aligned+16+h_length,kinfo->key,kinfo->klen+1);
    hostbuf->h_aliases   = (char **)aligned;
    hostbuf->h_addrtype  = type;
    hostbuf->h_length    = h_length;
    hostbuf->h_addr_list = (char **)(aligned+8);
    hostbuf->h_aliases[0]   = NULL;
    hostbuf->h_addr_list[0] = memcpy(aligned+16, addr, h_length);
    *vinfo->p.hostent       = hostbuf;
    return NSS_STATUS_SUCCESS;
}

enum nss_status
_nss_files_gethostent_r(struct hostent * const restrict hostbuf,
                        char * const restrict buf, const size_t buflen,
                        struct hostent ** const restrict hostbufp,
                        int * const restrict h_errnop)
{
    const struct _nss_vinfo vinfo = { .decode = _nss_mcdb_decode_hostent,
                                      .u      = { .hostent = hostbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .hostent = hostbufp } };
    enum nss_status status;
    *hostbufp = NULL;
    status = _nss_mcdb_getent(NSS_DBTYPE_HOSTS, &vinfo);
    return (status == NSS_STATUS_SUCCESS)
      ? NSS_STATUS_SUCCESS
      : gethost_fill_h_errnop(status, h_errnop);
}

enum nss_status
_nss_files_gethostbyname2_r(const char * const restrict name, const int type,
                            struct hostent * const restrict hostbuf,
                            char * const restrict buf, const size_t buflen,
                            struct hostent ** const restrict hostbufp,
                            int * const restrict h_errnop)
{
    const struct _nss_kinfo kinfo = { .key    = name,
                                      .klen   = strlen(name),
                                      .tagc   = (unsigned char)'=' };
    const struct _nss_vinfo vinfo = { .decode = _nss_mcdb_decode_hostent,
                                      .u      = { .hostent = hostbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .hostent = hostbufp } };
    uint64_t addr[2] = {0,0};/* support addr sizes up to 128-bits (e.g. IPv6) */
    const int is_addr = inet_pton(type, name, &addr);
    *hostbufp = NULL;
    if (is_addr == 0) /* name is not valid address for specified addr family */
        return gethost_query(type, &kinfo, &vinfo, h_errnop);
    else if (is_addr > 0) /* name is valid address for specified addr family */
        return gethost_filladdr(&addr, type, &kinfo, &vinfo, h_errnop);
    else
        return gethost_fill_h_errnop(NSS_STATUS_RETURN, h_errnop);
}

enum nss_status
_nss_files_gethostbyname_r(const char * const restrict name,
                           struct hostent * const restrict hostbuf,
                           char * const restrict buf, const size_t buflen,
                           struct hostent ** const restrict hostbufp,
                           int * const restrict h_errnop)
{
    return _nss_files_gethostbyname2_r(name, AF_INET, hostbuf,
                                       buf, buflen, hostbufp, h_errnop);
}

enum nss_status
_nss_files_gethostbyaddr_r(const void * const restrict addr,
                           const socklen_t len, const int type,
                           struct hostent * const restrict hostbuf,
                           char * const restrict buf, const size_t buflen,
                           struct hostent ** const restrict hostbufp,
                           int * const restrict h_errnop)
{
    char hexstr[32];  /* support 128-bit IPv6 addresses */
    const struct _nss_kinfo kinfo = { .key    = hexstr,
                                      .klen   = len<<1,
                                      .tagc   = (unsigned char)'x' };
    const struct _nss_vinfo vinfo = { .decode = _nss_mcdb_decode_hostent,
                                      .u      = { .hostent = hostbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .hostent = hostbufp } };
    uint32_t u[4];  /* align data to uint32_t for uphex conversion routines */
    *hostbufp = NULL;
    switch (len) {
      case 4:  /* e.g. AF_INET4 */
               memcpy(&u, addr, 4);
               uint32_to_ascii8uphex(u[0], hexstr);
               break;
      case 16: /* e.g. AF_INET6 */
               memcpy(&u, addr, 16);
               uint32_to_ascii8uphex(u[0], hexstr);
               uint32_to_ascii8uphex(u[1], hexstr+8);
               uint32_to_ascii8uphex(u[2], hexstr+16);
               uint32_to_ascii8uphex(u[3], hexstr+24);
               break;
      default: return NSS_STATUS_UNAVAIL; /* other types not implemented */
    }

    return gethost_query(type, &kinfo, &vinfo, h_errnop);
}


/* GPS: should we also provide an efficient innetgr()? */
enum nss_status
_nss_files_getnetgrent_r(char ** const restrict host,
                         char ** const restrict user,
                         char ** const restrict domain,
                         char * const restrict buf, const size_t buflen)
{
    const struct _nss_vinfo vinfo = { .decode = _nss_mcdb_decode_buf,
                                      .u      = { .NA = 0 },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .NA = 0 } };
    const enum nss_status status =
      _nss_mcdb_getent(NSS_DBTYPE_NETGROUP, &vinfo);
    if (status == NSS_STATUS_SUCCESS) {
        /* success ensures valid data (so that memchr will not return NULL) */
        *host   = buf;
        *user   = (char *)memchr(buf,   '\0', buflen) + 1;
        *domain = (char *)memchr(*user, '\0', buflen - (*user - buf)) + 1;
    }
    return status;
}


enum nss_status
_nss_files_getnetent_r(struct netent * const restrict netbuf,
                       char * const restrict buf, const size_t buflen,
                       struct netent ** const restrict netbufp)
{
    const struct _nss_vinfo vinfo = { .decode = _nss_mcdb_decode_netent,
                                      .u      = { .netent = netbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .netent = netbufp } };
    *netbufp = NULL;
    return _nss_mcdb_getent(NSS_DBTYPE_NETWORKS, &vinfo);
}

enum nss_status
_nss_files_getnetbyname_r(const char * const restrict name,
                          struct netent * const restrict netbuf,
                          char * const restrict buf, const size_t buflen,
                          struct netent ** const restrict netbufp)
{
    const struct _nss_kinfo kinfo = { .key    = name,
                                      .klen   = strlen(name),
                                      .tagc   = (unsigned char)'=' };
    const struct _nss_vinfo vinfo = { .decode = _nss_mcdb_decode_netent,
                                      .u      = { .netent = netbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .netent = netbufp } };
    *netbufp = NULL;
    return _nss_mcdb_get_generic(NSS_DBTYPE_NETWORKS, &kinfo, &vinfo);
}

enum nss_status
_nss_files_getnetbyaddr_r(const uint32_t net, const int type,
                          struct netent * const restrict netbuf,
                          char * const restrict buf, const size_t buflen,
                          struct netent ** const restrict netbufp)
{
    char hexstr[16];
    const struct _nss_kinfo kinfo = { .key    = hexstr,
                                      .klen   = sizeof(hexstr),
                                      .tagc   = (unsigned char)'x' };
    const struct _nss_vinfo vinfo = { .decode = _nss_mcdb_decode_netent,
                                      .u      = { .netent = netbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .netent = netbufp } };
    *netbufp = NULL;
    uint32_to_ascii8uphex(net, hexstr);
    uint32_to_ascii8uphex((uint32_t)type, hexstr+8);
    return _nss_mcdb_get_generic(NSS_DBTYPE_NETWORKS, &kinfo, &vinfo);
}


enum nss_status
_nss_files_getprotoent_r(struct protoent * const restrict protobuf,
                         char * const restrict buf, const size_t buflen,
                         struct protoent ** const restrict protobufp)
{
    const struct _nss_vinfo vinfo = { .decode = _nss_mcdb_decode_protoent,
                                      .u      = { .protoent = protobuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .protoent = protobufp } };
    *protobufp = NULL;
    return _nss_mcdb_getent(NSS_DBTYPE_PROTOCOLS, &vinfo);
}

enum nss_status
_nss_files_getprotobyname_r(const char * const restrict name,
                            struct protoent * const restrict protobuf,
                            char * const restrict buf, const size_t buflen,
                            struct protoent ** const restrict protobufp)
{
    const struct _nss_kinfo kinfo = { .key    = name,
                                      .klen   = strlen(name),
                                      .tagc   = (unsigned char)'=' };
    const struct _nss_vinfo vinfo = { .decode = _nss_mcdb_decode_protoent,
                                      .u      = { .protoent = protobuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .protoent = protobufp } };
    *protobufp = NULL;
    return _nss_mcdb_get_generic(NSS_DBTYPE_PROTOCOLS, &kinfo, &vinfo);
}

enum nss_status
_nss_files_getprotobynumber_r(const int proto,
                              struct protoent * const restrict protobuf,
                              char * const restrict buf, const size_t buflen,
                              struct protoent ** const restrict protobufp)
{
    char hexstr[8];
    const struct _nss_kinfo kinfo = { .key    = hexstr,
                                      .klen   = sizeof(hexstr),
                                      .tagc   = (unsigned char)'x' };
    const struct _nss_vinfo vinfo = { .decode = _nss_mcdb_decode_protoent,
                                      .u      = { .protoent = protobuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .protoent = protobufp } };
    *protobufp = NULL;
    uint32_to_ascii8uphex((uint32_t)proto, hexstr);
    return _nss_mcdb_get_generic(NSS_DBTYPE_PROTOCOLS, &kinfo, &vinfo);
}


enum nss_status
_nss_files_getrpcent_r(struct rpcent * const restrict rpcbuf,
                       char * const restrict buf, const size_t buflen,
                       struct rpcent ** const restrict rpcbufp)
{
    const struct _nss_vinfo vinfo = { .decode = _nss_mcdb_decode_rpcent,
                                      .u      = { .rpcent = rpcbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .rpcent = rpcbufp } };
    *rpcbufp = NULL;
    return _nss_mcdb_getent(NSS_DBTYPE_RPC, &vinfo);
}

enum nss_status
_nss_files_getrpcbyname_r(const char * const restrict name,
                          struct rpcent * const restrict rpcbuf,
                          char * const restrict buf, const size_t buflen,
                          struct rpcent ** const restrict rpcbufp)
{
    const struct _nss_kinfo kinfo = { .key    = name,
                                      .klen   = strlen(name),
                                      .tagc   = (unsigned char)'=' };
    const struct _nss_vinfo vinfo = { .decode = _nss_mcdb_decode_rpcent,
                                      .u      = { .rpcent = rpcbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .rpcent = rpcbufp } };
    *rpcbufp = NULL;
    return _nss_mcdb_get_generic(NSS_DBTYPE_RPC, &kinfo, &vinfo);
}

enum nss_status
_nss_files_getrpcbynumber_r(const int number,
                            struct rpcent * const restrict rpcbuf,
                            char * const restrict buf, const size_t buflen,
                            struct rpcent ** const restrict rpcbufp)
{
    char hexstr[8];
    const struct _nss_kinfo kinfo = { .key    = hexstr,
                                      .klen   = sizeof(hexstr),
                                      .tagc   = (unsigned char)'x' };
    const struct _nss_vinfo vinfo = { .decode = _nss_mcdb_decode_rpcent,
                                      .u      = { .rpcent = rpcbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .rpcent = rpcbufp } };
    *rpcbufp = NULL;
    uint32_to_ascii8uphex((uint32_t)number, hexstr);
    return _nss_mcdb_get_generic(NSS_DBTYPE_RPC, &kinfo, &vinfo);
}


enum nss_status
_nss_files_getservent_r(struct servent * const restrict servbuf,
                        char * const restrict buf, const size_t buflen,
                        struct servent ** const restrict servbufp)
{
    const struct _nss_vinfo vinfo = { .decode = _nss_mcdb_decode_servent,
                                      .u      = { .servent = servbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .servent = servbufp } };
    *servbufp = NULL;
    if (buflen > 0) {
        *buf = '\0';
        return _nss_mcdb_getent(NSS_DBTYPE_SERVICES, &vinfo);
    }
    else {
        errno = ERANGE;
        return NSS_STATUS_TRYAGAIN;
    }
}

enum nss_status
_nss_files_getservbyname_r(const char * const restrict name,
                           const char * const restrict proto,
                           struct servent * const restrict servbuf,
                           char * const restrict buf, const size_t buflen,
                           struct servent ** const restrict servbufp)
{
    const struct _nss_kinfo kinfo = { .key    = name,
                                      .klen   = strlen(name),
                                      .tagc   = (unsigned char)'=' };
    const struct _nss_vinfo vinfo = { .decode = _nss_mcdb_decode_servent,
                                      .u      = { .servent = servbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .servent = servbufp } };
    const size_t plen = proto != NULL ? strlen(proto) : 0;
    *servbufp = NULL;
    if (buflen > plen) {
        if (plen)  /*copy proto for later match in _nss_mcdb_decode_servent()*/
            memcpy(buf, proto, plen+1);
        else
            *buf = '\0';
        return _nss_mcdb_get_generic(NSS_DBTYPE_SERVICES, &kinfo, &vinfo);
    }
    else {
        errno = ERANGE;
        return NSS_STATUS_TRYAGAIN;
    }
}

enum nss_status
_nss_files_getservbyport_r(const int port, const char * const restrict proto,
                           struct servent * const restrict servbuf,
                           char * const restrict buf, const size_t buflen,
                           struct servent ** const restrict servbufp)
{
    char hexstr[8];
    const struct _nss_kinfo kinfo = { .key    = hexstr,
                                      .klen   = sizeof(hexstr),
                                      .tagc   = (unsigned char)'x' };
    const struct _nss_vinfo vinfo = { .decode = _nss_mcdb_decode_servent,
                                      .u      = { .servent = servbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .servent = servbufp } };
    const size_t plen = proto != NULL ? strlen(proto) : 0;
    *servbufp = NULL;
    if (buflen > plen) {
        if (plen)  /*copy proto for later match in _nss_mcdb_decode_servent()*/
            memcpy(buf, proto, plen+1);
        else
            *buf = '\0';
        uint32_to_ascii8uphex((uint32_t)port, hexstr);
        return _nss_mcdb_get_generic(NSS_DBTYPE_SERVICES, &kinfo, &vinfo);
    }
    else {
        errno = ERANGE;
        return NSS_STATUS_TRYAGAIN;
    }
}


enum nss_status
_nss_files_getaliasent_r(struct aliasent * const restrict aliasbuf,
                         char * const restrict buf, const size_t buflen,
                         struct aliasent ** const restrict aliasbufp)
{
    const struct _nss_vinfo vinfo = { .decode = _nss_mcdb_decode_aliasent,
                                      .u      = { .aliasent = aliasbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .aliasent = aliasbufp } };
    *aliasbufp = NULL;
    return _nss_mcdb_getent(NSS_DBTYPE_ALIASES, &vinfo);
}

enum nss_status
_nss_files_getaliasbyname_r(const char * const restrict name,
                            struct aliasent * const restrict aliasbuf,
                            char * const restrict buf, const size_t buflen,
                            struct aliasent ** const restrict aliasbufp)
{
    const struct _nss_kinfo kinfo = { .key    = name,
                                      .klen   = strlen(name),
                                      .tagc   = (unsigned char)'=' };
    const struct _nss_vinfo vinfo = { .decode = _nss_mcdb_decode_aliasent,
                                      .u      = { .aliasent = aliasbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .aliasent = aliasbufp } };
    *aliasbufp = NULL;
    return _nss_mcdb_get_generic(NSS_DBTYPE_ALIASES, &kinfo, &vinfo);
}


enum nss_status
_nss_files_getspent_r(struct spwd * const restrict spbuf,
                      char * const restrict buf, const size_t buflen,
                      struct spwd ** const restrict spbufp)
{
    const struct _nss_vinfo vinfo = { .decode = _nss_mcdb_decode_spwd,
                                      .u      = { .spwd = spbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .spwd = spbufp } };
    *spbufp = NULL;
    return _nss_mcdb_getent(NSS_DBTYPE_SHADOW, &vinfo);
}

enum nss_status
_nss_files_getspnam_r(const char * const restrict name,
                      struct spwd * const restrict spbuf,
                      char * const restrict buf, const size_t buflen,
                      struct spwd ** const restrict spbufp)
{
    const struct _nss_kinfo kinfo = { .key    = name,
                                      .klen   = strlen(name),
                                      .tagc   = (unsigned char)'=' };
    const struct _nss_vinfo vinfo = { .decode = _nss_mcdb_decode_spwd,
                                      .u      = { .spwd = spbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .spwd = spbufp } };
    *spbufp = NULL;
    return _nss_mcdb_get_generic(NSS_DBTYPE_SHADOW, &kinfo, &vinfo);
}


enum nss_status
_nss_files_getpublickey(const char * const restrict name,
                        char * const restrict buf, const size_t buflen)
{   /*  man getpublickey() *//* Solaris */
    const struct _nss_kinfo kinfo = { .key    = name,
                                      .klen   = strlen(name),
                                      .tagc   = (unsigned char)'=' };
    const struct _nss_vinfo vinfo = { .decode = _nss_mcdb_decode_buf,
                                      .u      = { .NA = 0 },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .NA = 0 } };
    return _nss_mcdb_get_generic(NSS_DBTYPE_PUBLICKEY, &kinfo, &vinfo);
}


enum nss_status
_nss_files_getsecretkey(const char * const restrict name,
                        const char * const restrict decryptkey,
                        char * const restrict buf, const size_t buflen)
{   /*  man getsecretkey() *//* Solaris */
    const struct _nss_kinfo kinfo = { .key    = name,
                                      .klen   = strlen(name),
                                      .tagc   = (unsigned char)'*' };
    const struct _nss_vinfo vinfo = { .decode = _nss_mcdb_decode_buf,
                                      .u      = { .NA = 0 },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .NA = 0 } };
    const enum nss_status status =
      _nss_mcdb_get_generic(NSS_DBTYPE_PUBLICKEY, &kinfo, &vinfo);
    /* TODO: decrypt buf using decryptkey */
    return status;
}


enum nss_status
_nss_files_getetherent_r(struct ether_addr * const restrict etherbuf,
                         char * const restrict buf, const size_t buflen,
                         struct ether_addr ** const restrict etherbufp)
{   /*  man ether_line()  */
    const struct _nss_vinfo vinfo = { .decode = _nss_mcdb_decode_ether_addr,
                                      .u      = { .ether_addr = etherbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .ether_addr = etherbufp } };
    *etherbufp = NULL;
    return _nss_mcdb_getent(NSS_DBTYPE_ETHERS, &vinfo);
}

enum nss_status
_nss_files_gethostton_r(const char * const restrict name,
                        struct ether_addr * const restrict etherbuf)
{   /*  man ether_hostton()  */
    const struct _nss_kinfo kinfo = { .key    = name,
                                      .klen   = strlen(name),
                                      .tagc   = (unsigned char)'=' };
    const struct _nss_vinfo vinfo = { .decode = _nss_mcdb_decode_ether_addr,
                                      .u      = { .ether_addr = etherbuf },
                                      .buf    = NULL,
                                      .buflen = 0,
                                      .p      = { .NA = 0 } };
    return _nss_mcdb_get_generic(NSS_DBTYPE_ETHERS, &kinfo, &vinfo);
}

enum nss_status
_nss_files_getntohost_r(struct ether_addr * const restrict ether,
                        char * const restrict buf, const size_t buflen)
{   /*  man ether_ntohost()  */
    char hexstr[12]; /* (6) 8-bit octets = 48-bits */
    const struct _nss_kinfo kinfo = { .key    = hexstr,
                                      .klen   = sizeof(hexstr),
                                      .tagc   = (unsigned char)'x' };
    const struct _nss_vinfo vinfo = { .decode = _nss_mcdb_decode_buf,
                                      .u      = { .NA = 0 },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .NA = 0 } };
    /* (assumes hexstr[] on stack is aligned to at least 4-byte boundary) */
    memcpy(hexstr, &(ether->ether_addr_octet[0]), sizeof(hexstr));
    uint32_to_ascii8uphex(ntohl(*((uint32_t *)&hexstr[0])), hexstr);
    uint16_to_ascii4uphex((uint32_t)ntohs(*((uint16_t *)&hexstr[8])), hexstr+8);
    return _nss_mcdb_get_generic(NSS_DBTYPE_ETHERS, &kinfo, &vinfo);
}




/* GPS:
 *   ==> lookups might take lock, lookup, decode, unlock mutex
 *   instead of lock, register, unlock, lookup, decode, lock, unregister, unlock
 *   (need to create an interface for this and document as dangerous if not
 *    done carefully, in an encapsulated manner)
 */
/* TODO create an atexit() routine to walk static maps and munmap recursively
 * (might only do if compiled with -D_FORTIFY_SOURCE or something so that we
 *  free() memory allocated to demonstrate lack of leaking)
 */
/* GPS: verify what nss passes in and what it wants as exit values
 * It probably always wants enum nss_status
 */





/* GPS: FIXME FIXME FIXME
 * mcdb_unregister() calls down to mcdb_mmap_free() if there are no other
 * references.  mcdb_mmap_free() calls mcdb_mmap_unmap() which will set
 * map->ptr to be NULL.  We either need to detect this to reopen mmap on
 * every usage, or we need to provide a persistence mechanism which checks
 * if map needs to be reopened.  (Yes, add a stat()) */



#if 0

_nss_files_parse_etherent
_nss_files_parse_grent@@GLIBC_PRIVATE
_nss_files_parse_netent
_nss_files_parse_protoent
_nss_files_parse_pwent@@GLIBC_PRIVATE
_nss_files_parse_rpcent
_nss_files_parse_servent
_nss_files_parse_spent@@GLIBC_PRIVATE

_nss_netgroup_parseline


/* GPS: document which routines are POSIX-1.2001 and which are GNU extensions */
/* GPS: make sure to fill in h_errnop on error */
/* GPS: if gethost* buffer sizes are not large enough,
 *      man page says return ERANGE ?  In errno or in enum nss_status ? */

/* GPS: TODO
 * POSIX.1-2001  marks  gethostbyaddr()  and  gethostbyname() obsolescent.
 * See getaddrinfo(), getnameinfo(), gai_strerror() and see if we can implement
 * the parsing and lookups according to those interfaces.
 */

GPS: how about a mcdb for /etc/nsswitch.conf ?
  (where does nscd socket get plugged in to all this?
   and should I add something to nscd.conf to implicitly use mcdb
   before its cache?)

#endif
