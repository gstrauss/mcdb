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
#include <arpa/inet.h>     /* htonl() htons() ntohl() ntohs() */

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
static const char * const restrict nss_dbnames[] = {
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


static struct mcdb_mmap nss_mcdb_mmap_st[sizeof(nss_dbnames)/sizeof(char *)];
static struct mcdb_mmap *nss_mcdb_mmap[sizeof(nss_dbnames)/sizeof(char *)];


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
uint32_to_ascii4uphex(const uint32_t n, char * restrict buf)
  __attribute_nonnull__;
static char *  __attribute_noinline__
uint32_to_ascii4uphex(const uint32_t n, char * restrict buf)
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
    if (&nss_mcdb_mmap_st[sizeof(nss_mcdb_mmap_st)/sizeof(struct mcdb_mmap)]<=m
        || m < &nss_mcdb_mmap_st[0])
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
    static pthread_mutex_t nss_mcdb_global_mutex = PTHREAD_MUTEX_INITIALIZER;
  #endif
    static int dfd = -1;
    struct mcdb_mmap * const restrict map = &nss_mcdb_mmap_st[dbtype];
    int fd;
    bool rc = false;

    if (pthread_mutex_lock(&nss_mcdb_global_mutex) != 0)
        return false;

    if (nss_mcdb_mmap[dbtype] != 0) {
        /* init'ed by another thread while waiting for mutex */
        pthread_mutex_unlock(&nss_mcdb_global_mutex);
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
            pthread_mutex_unlock(&nss_mcdb_global_mutex);
            return false;
        }
    }
    /* assert(sizeof(map->fname) > strlen(nss_dbnames[dbtype])); */
    memcpy(map->fname, nss_dbnames[dbtype], strlen(nss_dbnames[dbtype])+1);
  #else
    if (snprintf(map->fname, sizeof(map->fname), "%s%s",
                 NSS_DBPATH, nss_dbnames[dbtype]) >= sizeof(map->fname)) {
        pthread_mutex_unlock(&nss_mcdb_global_mutex);
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
            nss_mcdb_mmap[dbtype] = map;
    }

    pthread_mutex_unlock(&nss_mcdb_global_mutex);
    return rc;
}

static struct mcdb_mmap *  __attribute_noinline__
_nss_mcdb_db_getshared(const enum nss_dbtype dbtype)
  __attribute_nonnull__  __attribute_warn_unused_result__;
static struct mcdb_mmap *  __attribute_noinline__
_nss_mcdb_db_getshared(const enum nss_dbtype dbtype)
{
/* GPS: should we be doing stat() of mcdb to see if it has changed? */
/* Is gettimeofday() much less expensive than stat()?  If so, we might
 * implement something like a 15 second cache */
    if (__builtin_expect(nss_mcdb_mmap[dbtype] != 0, true)
        || _nss_mcdb_db_openshared(dbtype)) {
        if (mcdb_register(nss_mcdb_mmap[dbtype]))
            return nss_mcdb_mmap[dbtype];
    }
    return NULL;
}

static enum nss_status
_nss_mcdb_setent(struct mcdb * const restrict m, const enum nss_dbtype dbtype)
  __attribute_nonnull__;
static enum nss_status
_nss_mcdb_setent(struct mcdb * const restrict m, const enum nss_dbtype dbtype)
{
    if (m->map != NULL || (m->map = _nss_mcdb_db_getshared(dbtype)) != NULL) {
        m->hpos = 0;
        return NSS_STATUS_SUCCESS;
    }
    return NSS_STATUS_UNAVAIL;
}

static enum nss_status
_nss_mcdb_endent(struct mcdb * const restrict m)
  __attribute_nonnull__;
static enum nss_status
_nss_mcdb_endent(struct mcdb * const restrict m)
{
    return (m->map == NULL || mcdb_unregister(m->map))
      ? NSS_STATUS_SUCCESS
      : NSS_STATUS_TRYAGAIN; /* (fails only if obtaining mutex fails) */
}

static enum nss_status
_nss_mcdb_getent(struct mcdb * const restrict m, const enum nss_dbtype dbtype,
                 const struct _nss_vinfo * const restrict vinfo)
  __attribute_nonnull__  __attribute_warn_unused_result__;
static enum nss_status
_nss_mcdb_getent(struct mcdb * const restrict m, const enum nss_dbtype dbtype,
                 const struct _nss_vinfo * const restrict vinfo)
{
    unsigned char *map;
    uint32_t eod;
    uint32_t klen;
    if (__builtin_expect(m->map == NULL, false)
        && _nss_mcdb_setent(m, dbtype) != NSS_STATUS_SUCCESS)
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
_nss_files_get_generic(const enum nss_dbtype dbtype,
                       const struct _nss_kinfo * const restrict kinfo,
                       const struct _nss_vinfo * const restrict vinfo)
  __attribute_nonnull__  __attribute_warn_unused_result__;
static enum nss_status
_nss_files_get_generic(const enum nss_dbtype dbtype,
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

    mcdb_unregister(m.map);
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

/* set*ent(), get*ent(), end*ent() are not thread-safe */
/* (use thread-local storage of static struct mcdb array to increase safety) */

static __thread struct mcdb nss_mcdb_st[sizeof(nss_dbnames)/sizeof(char *)];

#define _nss_files_xxxNAMEent(name, dbtype)               \
                                                          \
  void _nss_files_set##name##ent(void)                    \
  { _nss_mcdb_setent(&nss_mcdb_st[(dbtype)], (dbtype)); } \
                                                          \
  void _nss_files_end##name##ent(void)                    \
  { _nss_mcdb_endent(&nss_mcdb_st[(dbtype)]); }


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
_nss_files_decode_buf(struct mcdb * restrict,
                      const struct _nss_kinfo * restrict,
                      const struct _nss_vinfo * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;
static enum nss_status
_nss_files_decode_buf(struct mcdb * const restrict m,
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
_nss_files_decode_passwd(struct mcdb * restrict,
                         const struct _nss_kinfo * restrict,
                         const struct _nss_vinfo * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;
enum nss_status
_nss_files_decode_group(struct mcdb * restrict,
                        const struct _nss_kinfo * restrict,
                        const struct _nss_vinfo * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;
enum nss_status
_nss_files_decode_hostent(struct mcdb * restrict,
                          const struct _nss_kinfo * restrict,
                          const struct _nss_vinfo * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;
enum nss_status
_nss_files_decode_netent(struct mcdb * restrict,
                         const struct _nss_kinfo * restrict,
                         const struct _nss_vinfo * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;
enum nss_status
_nss_files_decode_protoent(struct mcdb * restrict,
                           const struct _nss_kinfo * restrict,
                           const struct _nss_vinfo * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;
enum nss_status
_nss_files_decode_rpcent(struct mcdb * restrict,
                         const struct _nss_kinfo * restrict,
                         const struct _nss_vinfo * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;
enum nss_status
_nss_files_decode_servent(struct mcdb * restrict,
                          const struct _nss_kinfo * restrict,
                          const struct _nss_vinfo * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;
  /* TODO: check proto in buf != "" and mcdb_findtagnext() until match 
   *   (buf = "" and take first match) */
enum nss_status
_nss_files_decode_aliasent(struct mcdb * restrict,
                           const struct _nss_kinfo * restrict,
                           const struct _nss_vinfo * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;
enum nss_status
_nss_files_decode_spwd(struct mcdb * restrict,
                       const struct _nss_kinfo * restrict,
                       const struct _nss_vinfo * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;
enum nss_status
_nss_files_decode_ether_addr(struct mcdb * restrict,
                             const struct _nss_kinfo * restrict,
                             const struct _nss_vinfo * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;
    /* TODO: decode into struct ether_addr and hostname in buf, if not NULL */


enum nss_status
_nss_files_getpwent_r(struct passwd * const restrict pwbuf,
                      char * const restrict buf, const size_t buflen,
                      struct passwd ** const restrict pwbufp)
{
    const struct _nss_vinfo vinfo = { .decode = _nss_files_decode_passwd,
                                      .u      = { .passwd = pwbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .passwd = pwbufp } };
    return _nss_mcdb_getent(&nss_mcdb_st[NSS_DBTYPE_PASSWD],
                                         NSS_DBTYPE_PASSWD, &vinfo);
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
    const struct _nss_vinfo vinfo = { .decode = _nss_files_decode_passwd,
                                      .u      = { .passwd = pwbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .passwd = pwbufp } };
    return _nss_files_get_generic(NSS_DBTYPE_PASSWD, &kinfo, &vinfo);
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
    const struct _nss_vinfo vinfo = { .decode = _nss_files_decode_passwd,
                                      .u      = { .passwd = pwbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .passwd = pwbufp } };
    uint32_to_ascii8uphex((uint32_t)uid, hexstr);
    return _nss_files_get_generic(NSS_DBTYPE_PASSWD, &kinfo, &vinfo);
}


enum nss_status
_nss_files_getgrent_r(struct group * const restrict grbuf,
                      char * const restrict buf, const size_t buflen,
                      struct group ** const restrict grbufp)
{
    const struct _nss_vinfo vinfo = { .decode = _nss_files_decode_group,
                                      .u      = { .group = grbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .group = grbufp } };
    return _nss_mcdb_getent(&nss_mcdb_st[NSS_DBTYPE_GROUP],
                                         NSS_DBTYPE_GROUP, &vinfo);
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
    const struct _nss_vinfo vinfo = { .decode = _nss_files_decode_group,
                                      .u      = { .group = grbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .group = grbufp } };
    return _nss_files_get_generic(NSS_DBTYPE_GROUP, &kinfo, &vinfo);
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
    const struct _nss_vinfo vinfo = { .decode = _nss_files_decode_group,
                                      .u      = { .group = grbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .group = grbufp } };
    uint32_to_ascii8uphex((uint32_t)gid, hexstr);
    return _nss_files_get_generic(NSS_DBTYPE_GROUP, &kinfo, &vinfo);
}


/* GPS: TODO check for each get*ent if buflen is size_t or int
 * and document if we choose to use size_t anyway */
/* GPS: some sethostent() calls have a 'stayopen' parameter.
 * Is nss looking for this?  Check */
/* GPS: document which routines are POSIX-1.2001 and which are GNU extensions */
/* GPS: make sure to fill in h_errnop on error */
/* GPS: if buffer sizes are not large enough, man page says return ERANGE ? */

static enum nss_status
fill_h_errnop(enum nss_status status, int * const restrict h_errnop)
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

enum nss_status
_nss_files_gethostent_r(struct hostent * const restrict hostbuf,
                        char * const restrict buf, const size_t buflen,
                        struct hostent ** const restrict hostbufp,
                        int * const restrict h_errnop)
{
    const struct _nss_vinfo vinfo = { .decode = _nss_files_decode_hostent,
                                      .u      = { .hostent = hostbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .hostent = hostbufp } };
    const enum nss_status status =
      _nss_mcdb_getent(&nss_mcdb_st[NSS_DBTYPE_HOSTS],
                                    NSS_DBTYPE_HOSTS, &vinfo);
    return (status == NSS_STATUS_SUCCESS)
      ? NSS_STATUS_SUCCESS
      : fill_h_errnop(status, h_errnop);
}

/* GPS: TODO
 * POSIX.1-2001  marks  gethostbyaddr()  and  gethostbyname() obsolescent.
 * See getaddrinfo(), getnameinfo(), gai_strerror() and see if we can implement
 * the parsing and lookups according to those interfaces.
 */

/*  ??? (struct hostent *)   */
enum nss_status
_nss_files_gethostbyname2_r(const char * const restrict name, const int type,
                            struct hostent * const restrict hostbuf,
                            char * const restrict buf, const size_t buflen,
                            struct hostent ** const restrict hostbufp,
                            int * const restrict h_errnop)
{
    const enum nss_status status;
    struct mcdb m;
    size_t nlen;
/* GPS: if the name is already an address, fill the struct and return.
 * GPS: FIXME: should we look for HOSTALIASES environment variable
 * and parse that?  Do other get* types look for environment overrides?  Check.
 * GPS: if the name does *not* end in '.' and HOSTALIASES is set, look there
 * first and there here?  Yech.  HOSTALIASES is not documented in SuSv6.
    if (0 && getenv("HOSTALIASES") != NULL)
        return NSS_STATUS_NOTFOUND;
 */
/* GPS: look for unadorned name, then current
 * domain and then parent domain appended.  Yech. */
    m.map = _nss_mcdb_db_getshared(NSS_DBTYPE_HOSTS);
    if (__builtin_expect(m.map == NULL, false))
        return NSS_STATUS_UNAVAIL;
    nlen = strlen(name);
/* GPS: FIXME: must parse name str and see if it is already an address */
    if (  mcdb_findtagstart(&m, name, nlen, (unsigned char)'=')
        && mcdb_findtagnext(&m, name, nlen, (unsigned char)'=')  ) {
/* GPS: FIXME: check type match: mcdb_findtagnext() until match */

        /* TODO: decode into struct hostent */
        /* fail ERANGE only if insufficient buffer space supplied */

        mcdb_unregister(m.map);
        return NSS_STATUS_SUCCESS;
    }
    mcdb_unregister(m.map);
    return NSS_STATUS_NOTFOUND;

    return (status == NSS_STATUS_SUCCESS)
      ? NSS_STATUS_SUCCESS
      : fill_h_errnop(status, h_errnop);
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

/*  ??? (struct hostent *)   */
enum nss_status
_nss_files_gethostbyaddr_r(const void * const restrict addr,
                           const socklen_t len, const int type,
                           struct hostent * const restrict hostbuf,
                           char * const restrict buf, const size_t buflen,
                           struct hostent ** const restrict hostbufp,
                           int * const restrict h_errnop)
{
    const enum nss_status status;
    struct mcdb m;
    char hexstr[8];
    m.map = _nss_mcdb_db_getshared(NSS_DBTYPE_HOSTS);
    if (__builtin_expect(m.map == NULL, false))
        return NSS_STATUS_UNAVAIL;
/* GPS: FIXME: do we need 16 char buffer for addr? */
/* GPS: FIXME: do we need to canonicalize addr? */
    uint32_to_ascii8uphex((uint32_t)len, hexstr);
    if (  mcdb_findtagstart(&m, hexstr, sizeof(hexstr), (unsigned char)'x')
        && mcdb_findtagnext(&m, hexstr, sizeof(hexstr), (unsigned char)'x')  ) {
/* GPS: FIXME: check addr,len,type match: mcdb_findtagnext() until match */

        /* TODO: decode into struct hostent */
        /* fail ERANGE only if insufficient buffer space supplied */

        mcdb_unregister(m.map);
        return NSS_STATUS_SUCCESS;
    }
    mcdb_unregister(m.map);
    return NSS_STATUS_NOTFOUND;

    return (status == NSS_STATUS_SUCCESS)
      ? NSS_STATUS_SUCCESS
      : fill_h_errnop(status, h_errnop);
}


/* GPS: should we also provide an efficient innetgr()? */
enum nss_status
_nss_files_getnetgrent_r(char ** const restrict host,
                         char ** const restrict user,
                         char ** const restrict domain,
                         char * const restrict buf, const size_t buflen)
{
    const struct _nss_vinfo vinfo = { .decode = _nss_files_decode_buf,
                                      .u      = { .NA = 0 },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .NA = 0 } };
    const enum nss_status status =
      _nss_mcdb_getent(&nss_mcdb_st[NSS_DBTYPE_NETGROUP],
                                    NSS_DBTYPE_NETGROUP, &vinfo);
    if (status == NSS_STATUS_SUCCESS) {
        *host   = buf;
        *user   = (char *)memchr(buf, '\0', buflen) + 1;
        *domain = (char *)memchr(*user, '\0', buflen - (*user - buf)) + 1;
    }
    return status;
}


enum nss_status
_nss_files_getnetent_r(struct netent * const restrict netbuf,
                       char * const restrict buf, const size_t buflen,
                       struct netent ** const restrict netbufp)
{
    const struct _nss_vinfo vinfo = { .decode = _nss_files_decode_netent,
                                      .u      = { .netent = netbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .netent = netbufp } };
    return _nss_mcdb_getent(&nss_mcdb_st[NSS_DBTYPE_NETWORKS],
                                         NSS_DBTYPE_NETWORKS, &vinfo);
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
    const struct _nss_vinfo vinfo = { .decode = _nss_files_decode_netent,
                                      .u      = { .netent = netbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .netent = netbufp } };
    return _nss_files_get_generic(NSS_DBTYPE_NETWORKS, &kinfo, &vinfo);
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
    const struct _nss_vinfo vinfo = { .decode = _nss_files_decode_netent,
                                      .u      = { .netent = netbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .netent = netbufp } };
    uint32_to_ascii8uphex(net, hexstr);
    uint32_to_ascii8uphex((uint32_t)type, hexstr+8);
    return _nss_files_get_generic(NSS_DBTYPE_NETWORKS, &kinfo, &vinfo);
}


enum nss_status
_nss_files_getprotoent_r(struct protoent * const restrict protobuf,
                         char * const restrict buf, const size_t buflen,
                         struct protoent ** const restrict protobufp)
{
    const struct _nss_vinfo vinfo = { .decode = _nss_files_decode_protoent,
                                      .u      = { .protoent = protobuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .protoent = protobufp } };
    return _nss_mcdb_getent(&nss_mcdb_st[NSS_DBTYPE_PROTOCOLS],
                                         NSS_DBTYPE_PROTOCOLS, &vinfo);
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
    const struct _nss_vinfo vinfo = { .decode = _nss_files_decode_protoent,
                                      .u      = { .protoent = protobuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .protoent = protobufp } };
    return _nss_files_get_generic(NSS_DBTYPE_PROTOCOLS, &kinfo, &vinfo);
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
    const struct _nss_vinfo vinfo = { .decode = _nss_files_decode_protoent,
                                      .u      = { .protoent = protobuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .protoent = protobufp } };
    uint32_to_ascii8uphex((uint32_t)proto, hexstr);
    return _nss_files_get_generic(NSS_DBTYPE_PROTOCOLS, &kinfo, &vinfo);
}


enum nss_status
_nss_files_getrpcent_r(struct rpcent * const restrict rpcbuf,
                       char * const restrict buf, const size_t buflen,
                       struct rpcent ** const restrict rpcbufp)
{
    const struct _nss_vinfo vinfo = { .decode = _nss_files_decode_rpcent,
                                      .u      = { .rpcent = rpcbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .rpcent = rpcbufp } };
    return _nss_mcdb_getent(&nss_mcdb_st[NSS_DBTYPE_RPC],
                                         NSS_DBTYPE_RPC, &vinfo);
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
    const struct _nss_vinfo vinfo = { .decode = _nss_files_decode_rpcent,
                                      .u      = { .rpcent = rpcbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .rpcent = rpcbufp } };
    return _nss_files_get_generic(NSS_DBTYPE_RPC, &kinfo, &vinfo);
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
    const struct _nss_vinfo vinfo = { .decode = _nss_files_decode_rpcent,
                                      .u      = { .rpcent = rpcbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .rpcent = rpcbufp } };
    uint32_to_ascii8uphex((uint32_t)number, hexstr);
    return _nss_files_get_generic(NSS_DBTYPE_RPC, &kinfo, &vinfo);
}


enum nss_status
_nss_files_getservent_r(struct servent * const restrict servbuf,
                        char * const restrict buf, const size_t buflen,
                        struct servent ** const restrict servbufp)
{
    const struct _nss_vinfo vinfo = { .decode = _nss_files_decode_servent,
                                      .u      = { .servent = servbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .servent = servbufp } };
    if (buflen > 0) {
        *buf = '\0';
        return _nss_mcdb_getent(&nss_mcdb_st[NSS_DBTYPE_SERVICES],
                                             NSS_DBTYPE_SERVICES, &vinfo);
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
    const struct _nss_vinfo vinfo = { .decode = _nss_files_decode_servent,
                                      .u      = { .servent = servbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .servent = servbufp } };
    const size_t plen = proto != NULL ? strlen(proto) : 0;
    if (buflen > plen) {
        if (plen)  /*copy proto for later match in _nss_files_decode_servent()*/
            memcpy(buf, proto, plen+1);
        else
            *buf = '\0';
        return _nss_files_get_generic(NSS_DBTYPE_SERVICES, &kinfo, &vinfo);
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
    const struct _nss_vinfo vinfo = { .decode = _nss_files_decode_servent,
                                      .u      = { .servent = servbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .servent = servbufp } };
    const size_t plen = proto != NULL ? strlen(proto) : 0;
    if (buflen > plen) {
        if (plen)  /*copy proto for later match in _nss_files_decode_servent()*/
            memcpy(buf, proto, plen+1);
        else
            *buf = '\0';
        uint32_to_ascii8uphex((uint32_t)port, hexstr);
        return _nss_files_get_generic(NSS_DBTYPE_SERVICES, &kinfo, &vinfo);
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
    const struct _nss_vinfo vinfo = { .decode = _nss_files_decode_aliasent,
                                      .u      = { .aliasent = aliasbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .aliasent = aliasbufp } };
    return _nss_mcdb_getent(&nss_mcdb_st[NSS_DBTYPE_ALIASES],
                                         NSS_DBTYPE_ALIASES, &vinfo);
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
    const struct _nss_vinfo vinfo = { .decode = _nss_files_decode_aliasent,
                                      .u      = { .aliasent = aliasbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .aliasent = aliasbufp } };
    return _nss_files_get_generic(NSS_DBTYPE_ALIASES, &kinfo, &vinfo);
}


enum nss_status
_nss_files_getspent_r(struct spwd * const restrict spbuf,
                      char * const restrict buf, const size_t buflen,
                      struct spwd ** const restrict spbufp)
{
    const struct _nss_vinfo vinfo = { .decode = _nss_files_decode_spwd,
                                      .u      = { .spwd = spbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .spwd = spbufp } };
    return _nss_mcdb_getent(&nss_mcdb_st[NSS_DBTYPE_SHADOW],
                                         NSS_DBTYPE_SHADOW, &vinfo);
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
    const struct _nss_vinfo vinfo = { .decode = _nss_files_decode_spwd,
                                      .u      = { .spwd = spbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .spwd = spbufp } };
    return _nss_files_get_generic(NSS_DBTYPE_SHADOW, &kinfo, &vinfo);
}


enum nss_status
_nss_files_getpublickey(const char * const restrict name,
                        char * const restrict buf, const size_t buflen)
{   /*  man getpublickey() *//* Solaris */
    const struct _nss_kinfo kinfo = { .key    = name,
                                      .klen   = strlen(name),
                                      .tagc   = (unsigned char)'=' };
    const struct _nss_vinfo vinfo = { .decode = _nss_files_decode_buf,
                                      .u      = { .NA = 0 },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .NA = 0 } };
    return _nss_files_get_generic(NSS_DBTYPE_PUBLICKEY, &kinfo, &vinfo);
}


enum nss_status
_nss_files_getsecretkey(const char * const restrict name,
                        const char * const restrict decryptkey,
                        char * const restrict buf, const size_t buflen)
{   /*  man getsecretkey() *//* Solaris */
    const struct _nss_kinfo kinfo = { .key    = name,
                                      .klen   = strlen(name),
                                      .tagc   = (unsigned char)'*' };
    const struct _nss_vinfo vinfo = { .decode = _nss_files_decode_buf,
                                      .u      = { .NA = 0 },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .NA = 0 } };
    const enum nss_status status =
      _nss_files_get_generic(NSS_DBTYPE_PUBLICKEY, &kinfo, &vinfo);
    /* TODO: decrypt buf using decryptkey */
    return status;
}


enum nss_status
_nss_files_getetherent_r(struct ether_addr * const restrict etherbuf,
                         char * const restrict buf, const size_t buflen,
                         struct ether_addr ** const restrict etherbufp)
{   /*  man ether_line()  */
    const struct _nss_vinfo vinfo = { .decode = _nss_files_decode_ether_addr,
                                      .u      = { .ether_addr = etherbuf },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .ether_addr = etherbufp } };
    return _nss_mcdb_getent(&nss_mcdb_st[NSS_DBTYPE_ETHERS],
                                         NSS_DBTYPE_ETHERS, &vinfo);
}

enum nss_status
_nss_files_gethostton_r(const char * const restrict name,
                        struct ether_addr * const restrict etherbuf)
{   /*  man ether_hostton()  */
    const struct _nss_kinfo kinfo = { .key    = name,
                                      .klen   = strlen(name),
                                      .tagc   = (unsigned char)'=' };
    const struct _nss_vinfo vinfo = { .decode = _nss_files_decode_ether_addr,
                                      .u      = { .ether_addr = etherbuf },
                                      .buf    = NULL,
                                      .buflen = 0,
                                      .p      = { .NA = 0 } };
    return _nss_files_get_generic(NSS_DBTYPE_ETHERS, &kinfo, &vinfo);
}

enum nss_status
_nss_files_getntohost_r(struct ether_addr * const restrict ether,
                        char * const restrict buf, const size_t buflen)
{   /*  man ether_ntohost()  */
    char hexstr[12]; /* (6) 8-bit octets = 48-bits */
    const struct _nss_kinfo kinfo = { .key    = hexstr,
                                      .klen   = sizeof(hexstr),
                                      .tagc   = (unsigned char)'x' };
    const struct _nss_vinfo vinfo = { .decode = _nss_files_decode_buf,
                                      .u      = { .NA = 0 },
                                      .buf    = buf,
                                      .buflen = buflen,
                                      .p      = { .NA = 0 } };
    /* (assumes hexstr[] on stack is aligned to at least 4-byte boundary) */
    memcpy(hexstr, &(ether->ether_addr_octet[0]), sizeof(hexstr));
    uint32_to_ascii8uphex(ntohl(*((uint32_t *)&hexstr[0])), hexstr);
    uint32_to_ascii4uphex((uint32_t)ntohs(*((uint16_t *)&hexstr[8])), hexstr+8);
    return _nss_files_get_generic(NSS_DBTYPE_ETHERS, &kinfo, &vinfo);
}




/* GPS: should have companion routine to _nss_mcdb_db_getshared() which turns
 * around and calls mcdb_unregister(m.map), named _nss_mcdb_db_releaseshare()
 * or something similar */
/* GPS: if any refcnt were increased, elimination of thread local storage will
 * leak that reference and the count will not go to zero
 * ==> While with less potential efficiency, perhaps we should mmap per thread.
 *     No, because the maps would not be cleaned up at thread close.
 * Is there an _nss_free_held_resources() routine?
 */
/* GPS: if every call to getgrnam() and others has to obtain a lock to increment
 * and then decrement refcnt, then that's expensive.  Probably want to use
 * atomic increment and decrement instead, if possible.  Probably not possible.
 * Perhaps create a set of _unlocked routines that bypass updating refcnt.
 *   ==> lookups might take lock, lookup, decode, unlock mutex
 *   instead of lock, register, unlock, lookup, decode, lock, unregister, unlock
 * Create a routine that can be called at synchronization points?
 *   (to say "clear everything that might otherwise be lost; no current usage")
 */
/* TODO create an atexit() routine to walk static maps and munmap recursively
 * (might only do if compiled with -D_FORTIFY_SOURCE or something so that we
 *  free() memory allocated to demonstrate lack of leaking)
 */
/* TODO might make all *getent have '=' string and set tagc == '=' */
/* TODO might static struct mcdb for each database type
 * and for first use, not bother malloc a new structure
 * That would lead to race conditions in threaded program.
 * unless also protected by a mutex.
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



/* GPS: TODO
To save some code size, use dispatch table for parse functions?
  ==> (i.e. have all get*ent() call generic function which parses through
       dispatch table)
 Might see if we can use dipatch table for other
 similar types, but not all have similar prototype.  Consolidate into
 single arg container struct that can be passed as void through to the
 parsing.  However, the matching still needs to be separate, so it might
 not be worth it for other types.
 */


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


GPS: create a generic routine which all the others call
    _nss_files_get_generic()
      encapsulates getting the shared pointer to mcdb and then unregistering it.
    Might also look up in table whether or not to encode the void * key
      into uppercase hex key.  Might make it uint32_t n[2]; for 64 bits
      or uint32_t n[4] for 128 bits
    Might make a routine through which all hex (num) lookups go which takes
      key and number of octets (in multiples of 4) to convert to hex,
      and constructs _nss_kinfo and then calls _nss_files_get_generic().
      (Use switch() and fall through calling ascii8uphex)
      (If we're not inlining ascii8uphex(), then probably not worth the effort)
    qtype for query-type (lookup by number or by string)
      (convert number to uppercase hex)
      (might make part of _nss_kinfo)
  In the parent routines which call the generic, it is their responsibility
    to encode multiple info, potentially from structs, into a "key", if, say
    a struct ether_addr needs to be made into a number, or port and type
    need to be encoded together.  Might have to put the number types into
    128-bit hex if we want to be generic.

For routines that let you specify port and proto, if proto is NULL, then
have an entry in mcdb configured specifically for that query type when
when creating the mcdb.  e.g. a -1 proto means tcp (if present) otherwise
a whatever types are there.

Currently we are passing kinfo through to decode routines to enable further
lookups.  If we do not need it, and if we include key info in each entry in
mcdb, then do not pass kinfo and maybe reorder list so that vinfo is in the
same register slot that is passed through.

#endif
