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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <nss.h> /* NSS_STATUS_{TRYAGAIN,UNAVAIL,NOTFOUND,SUCCESS,RETURN} */

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
/* string must fit into map->fname array */
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
                 const unsigned char tagc)
  __attribute_nonnull__  __attribute_warn_unused_result__;
static enum nss_status
_nss_mcdb_getent(struct mcdb * const restrict m, const enum nss_dbtype dbtype,
                 const unsigned char tagc)
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
        if (p[8+klen] == tagc)
            return NSS_STATUS_SUCCESS;
            /* valid data in mcdb_datapos() mcdb_datalen() mcdb_dataptr() */
    }
    return NSS_STATUS_NOTFOUND;
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



#include <pwd.h>

/*  ??? (struct passwd *)   */
enum nss_status
_nss_files_getpwent_r(struct passwd * const restrict pwbuf,
                      char * const restrict buf, const size_t buflen,
                      struct passwd ** const restrict pwbufp)
{
    struct mcdb * const m = &nss_mcdb_st[NSS_DBTYPE_PASSWD];
    enum nss_status status =
      _nss_mcdb_getent(m, NSS_DBTYPE_PASSWD, (unsigned char)'=');
    if (status != NSS_STATUS_SUCCESS)
        return status;

    /* TODO: decode into struct passwd */
    /* fail ERANGE only if insufficient buffer space supplied */
    /* else return NSS_STATUS_SUCCESS */

    return status;
}

/*  ??? (struct passwd *)   */
enum nss_status
_nss_files_getpwnam_r(const char * const restrict name,
                      struct passwd * const restrict pwbuf,
                      char * const restrict buf, const size_t buflen,
                      struct passwd ** const restrict pwbufp)
{
    struct mcdb m;
    size_t nlen;
    m.map = _nss_mcdb_db_getshared(NSS_DBTYPE_PASSWD);
    if (__builtin_expect(m.map == NULL, false))
        return NSS_STATUS_UNAVAIL;
    nlen = strlen(name);
    if (  mcdb_findtagstart(&m, name, nlen, (unsigned char)'=')
        && mcdb_findtagnext(&m, name, nlen, (unsigned char)'=')  ) {

        /* TODO: decode into struct passwd */
        /* fail ERANGE only if insufficient buffer space supplied */

        mcdb_unregister(m.map);
        return NSS_STATUS_SUCCESS;
    }
    mcdb_unregister(m.map);
    return NSS_STATUS_NOTFOUND;
}

/*  ??? (struct passwd *)   */
enum nss_status
_nss_files_getpwuid_r(const uid_t uid,
                      struct passwd * const restrict pwbuf,
                      char * const restrict buf, const size_t buflen,
                      struct passwd ** const restrict pwbufp)
{
    struct mcdb m;
    char hexstr[8];
    m.map = _nss_mcdb_db_getshared(NSS_DBTYPE_PASSWD);
    if (__builtin_expect(m.map == NULL, false))
        return NSS_STATUS_UNAVAIL;
    uint32_to_ascii8uphex((uint32_t)uid, hexstr);/* TODO take from cdbauthz.c */
    if (  mcdb_findtagstart(&m, hexstr, sizeof(hexstr), (unsigned char)'x')
        && mcdb_findtagnext(&m, hexstr, sizeof(hexstr), (unsigned char)'x')  ) {

        /* TODO: decode into struct passwd */
        /* fail ERANGE only if insufficient buffer space supplied */

        mcdb_unregister(m.map);
        return NSS_STATUS_SUCCESS;
    }
    mcdb_unregister(m.map);
    return NSS_STATUS_NOTFOUND;
}


#include <grp.h>

/*  ??? (struct group *)   */
enum nss_status
_nss_files_getgrent_r(struct group * const restrict grbuf,
                      char * const restrict buf, const size_t buflen,
                      struct group ** const restrict grbufp)
{
    struct mcdb * const m = &nss_mcdb_st[NSS_DBTYPE_GROUP];
    enum nss_status status =
      _nss_mcdb_getent(m, NSS_DBTYPE_GROUP, (unsigned char)'=');
    if (status != NSS_STATUS_SUCCESS)
        return status;

    /* TODO: decode into struct group */
    /* fail ERANGE only if insufficient buffer space supplied */
    /* else return NSS_STATUS_SUCCESS */

    return status;
}

/*  ??? (struct group *)   */
enum nss_status
_nss_files_getgrnam_r(const char * const restrict name,
                      struct group * const restrict grbuf,
                      char * const restrict buf, const size_t buflen,
                      struct group ** const restrict grbufp)
{
    struct mcdb m;
    size_t nlen;
    m.map = _nss_mcdb_db_getshared(NSS_DBTYPE_GROUP);
    if (__builtin_expect(m.map == NULL, false))
        return NSS_STATUS_UNAVAIL;
    nlen = strlen(name);
    if (  mcdb_findtagstart(&m, name, nlen, (unsigned char)'=')
        && mcdb_findtagnext(&m, name, nlen, (unsigned char)'=')  ) {

        /* TODO: decode into struct group */
        /* fail ERANGE only if insufficient buffer space supplied */

        mcdb_unregister(m.map);
        return NSS_STATUS_SUCCESS;
    }
    mcdb_unregister(m.map);
    return NSS_STATUS_NOTFOUND;
}

/*  ??? (struct group *)   */
enum nss_status
_nss_files_getgrgid_r(const gid_t gid,
                      struct group * const restrict grbuf,
                      char * const restrict buf, const size_t buflen,
                      struct group ** const restrict grbufp)
{
    struct mcdb m;
    char hexstr[8];
    m.map = _nss_mcdb_db_getshared(NSS_DBTYPE_GROUP);
    if (__builtin_expect(m.map == NULL, false))
        return NSS_STATUS_UNAVAIL;
    uint32_to_ascii8uphex((uint32_t)gid, hexstr);/* TODO take from cdbauthz.c */
    if (  mcdb_findtagstart(&m, hexstr, sizeof(hexstr), (unsigned char)'x')
        && mcdb_findtagnext(&m, hexstr, sizeof(hexstr), (unsigned char)'x')  ) {

        /* TODO: decode into struct group */
        /* fail ERANGE only if insufficient buffer space supplied */

        mcdb_unregister(m.map);
        return NSS_STATUS_SUCCESS;
    }
    mcdb_unregister(m.map);
    return NSS_STATUS_NOTFOUND;
}


#include <netdb.h>

/*  ??? (struct protoent *)   */
enum nss_status
_nss_files_getprotoent_r(struct protoent * const restrict protobuf,
                         char * const restrict buf, const size_t buflen,
                         struct protoent ** const restrict protobufp)
{
    struct mcdb * const m = &nss_mcdb_st[NSS_DBTYPE_PROTOCOLS];
    enum nss_status status =
      _nss_mcdb_getent(m, NSS_DBTYPE_PROTOCOLS, (unsigned char)'=');
    if (status != NSS_STATUS_SUCCESS)
        return status;

    /* TODO: decode into struct protoent */
    /* fail ERANGE only if insufficient buffer space supplied */
    /* else return NSS_STATUS_SUCCESS */

    return status;
}

/*  ??? (struct protoent *)   */
enum nss_status
_nss_files_getprotobyname_r(const char * const restrict name,
                            struct protoent * const restrict protobuf,
                            char * const restrict buf, const size_t buflen,
                            struct protoent ** const restrict protobufp)
{
    struct mcdb m;
    size_t nlen;
    m.map = _nss_mcdb_db_getshared(NSS_DBTYPE_PROTOCOLS);
    if (__builtin_expect(m.map == NULL, false))
        return NSS_STATUS_UNAVAIL;
    nlen = strlen(name);
    if (  mcdb_findtagstart(&m, name, nlen, (unsigned char)'=')
        && mcdb_findtagnext(&m, name, nlen, (unsigned char)'=')  ) {

        /* TODO: decode into struct protoent */
        /* fail ERANGE only if insufficient buffer space supplied */

        mcdb_unregister(m.map);
        return NSS_STATUS_SUCCESS;
    }
    mcdb_unregister(m.map);
    return NSS_STATUS_NOTFOUND;
}

/*  ??? (struct protoent *)   */
enum nss_status
_nss_files_getprotobynumber_r(const int proto,
                              struct protoent * const restrict protobuf,
                              char * const restrict buf, const size_t buflen,
                              struct protoent ** const restrict protobufp)
{
    struct mcdb m;
    char hexstr[8];
    m.map = _nss_mcdb_db_getshared(NSS_DBTYPE_PROTOCOLS);
    if (__builtin_expect(m.map == NULL, false))
        return NSS_STATUS_UNAVAIL;
    uint32_to_ascii8uphex((uint32_t)proto, hexstr); /* TODO from cdbauthz.c */
    if (  mcdb_findtagstart(&m, hexstr, sizeof(hexstr), (unsigned char)'x')
        && mcdb_findtagnext(&m, hexstr, sizeof(hexstr), (unsigned char)'x')  ) {

        /* TODO: decode into struct protoent */
        /* fail ERANGE only if insufficient buffer space supplied */

        mcdb_unregister(m.map);
        return NSS_STATUS_SUCCESS;
    }
    mcdb_unregister(m.map);
    return NSS_STATUS_NOTFOUND;
}


/*  ??? (struct rpcent *)   */
enum nss_status
_nss_files_getrpcent_r(struct rpcent * const restrict rpcbuf,
                       char * const restrict buf, const size_t buflen,
                       struct rpcent ** const restrict rpcbufp)
{
    struct mcdb * const m = &nss_mcdb_st[NSS_DBTYPE_RPC];
    enum nss_status status =
      _nss_mcdb_getent(m, NSS_DBTYPE_RPC, (unsigned char)'=');
    if (status != NSS_STATUS_SUCCESS)
        return status;

    /* TODO: decode into struct rpcent */
    /* fail ERANGE only if insufficient buffer space supplied */
    /* else return NSS_STATUS_SUCCESS */

    return status;
}

/*  ??? (struct rpcent *)   */
enum nss_status
_nss_files_getrpcbyname_r(const char * const restrict name,
                          struct rpcent * const restrict rpcbuf,
                          char * const restrict buf, const size_t buflen,
                          struct rpcent ** const restrict rpcbufp)
{
    struct mcdb m;
    size_t nlen;
    m.map = _nss_mcdb_db_getshared(NSS_DBTYPE_RPC);
    if (__builtin_expect(m.map == NULL, false))
        return NSS_STATUS_UNAVAIL;
    nlen = strlen(name);
    if (  mcdb_findtagstart(&m, name, nlen, (unsigned char)'=')
        && mcdb_findtagnext(&m, name, nlen, (unsigned char)'=')  ) {

        /* TODO: decode into struct rpcent */
        /* fail ERANGE only if insufficient buffer space supplied */

        mcdb_unregister(m.map);
        return NSS_STATUS_SUCCESS;
    }
    mcdb_unregister(m.map);
    return NSS_STATUS_NOTFOUND;
}

/*  ??? (struct rpcent *)   */
enum nss_status
_nss_files_getrpcbynumber_r(const int number,
                            struct rpcent * const restrict rpcbuf,
                            char * const restrict buf, const size_t buflen,
                            struct rpcent ** const restrict rpcbufp)
{
    struct mcdb m;
    char hexstr[8];
    m.map = _nss_mcdb_db_getshared(NSS_DBTYPE_RPC);
    if (__builtin_expect(m.map == NULL, false))
        return NSS_STATUS_UNAVAIL;
    uint32_to_ascii8uphex((uint32_t)number, hexstr); /* TODO from cdbauthz.c */
    if (  mcdb_findtagstart(&m, hexstr, sizeof(hexstr), (unsigned char)'x')
        && mcdb_findtagnext(&m, hexstr, sizeof(hexstr), (unsigned char)'x')  ) {

        /* TODO: decode into struct rpcent */
        /* fail ERANGE only if insufficient buffer space supplied */

        mcdb_unregister(m.map);
        return NSS_STATUS_SUCCESS;
    }
    mcdb_unregister(m.map);
    return NSS_STATUS_NOTFOUND;
}


/*  ??? (struct servent *)   */
enum nss_status
_nss_files_getservent_r(struct servent * const restrict servbuf,
                        char * const restrict buf, const size_t buflen,
                        struct servent ** const restrict servbufp)
{
    struct mcdb * const m = &nss_mcdb_st[NSS_DBTYPE_SERVICES];
    enum nss_status status =
      _nss_mcdb_getent(m, NSS_DBTYPE_SERVICES, (unsigned char)'=');
    if (status != NSS_STATUS_SUCCESS)
        return status;

    /* TODO: decode into struct servent */
    /* fail ERANGE only if insufficient buffer space supplied */
    /* else return NSS_STATUS_SUCCESS */

    return status;
}

/*  ??? (struct servent *)   */
enum nss_status
_nss_files_getservbyname_r(const char * const restrict name,
                           const char * const restrict proto,
                           struct servent * const restrict servbuf,
                           char * const restrict buf, const size_t buflen,
                           struct servent ** const restrict servbufp)
{
    struct mcdb m;
    size_t nlen;
    m.map = _nss_mcdb_db_getshared(NSS_DBTYPE_SERVICES);
    if (__builtin_expect(m.map == NULL, false))
        return NSS_STATUS_UNAVAIL;
    nlen = strlen(name);
    if (  mcdb_findtagstart(&m, name, nlen, (unsigned char)'=')
        && mcdb_findtagnext(&m, name, nlen, (unsigned char)'=')  ) {
/* GPS: FIXME: check proto != NULL and mcdb_findtagnext() until match */

        /* TODO: decode into struct servent */
        /* fail ERANGE only if insufficient buffer space supplied */

        mcdb_unregister(m.map);
        return NSS_STATUS_SUCCESS;
    }
    mcdb_unregister(m.map);
    return NSS_STATUS_NOTFOUND;
}

/*  ??? (struct servent *)   */
enum nss_status
_nss_files_getservbyport_r(const int port, const char * const restrict proto,
                           struct servent * const restrict servbuf,
                           char * const restrict buf, const size_t buflen,
                           struct servent ** const restrict servbufp)
{
    struct mcdb m;
    char hexstr[8];
    m.map = _nss_mcdb_db_getshared(NSS_DBTYPE_SERVICES);
    if (__builtin_expect(m.map == NULL, false))
        return NSS_STATUS_UNAVAIL;
    uint32_to_ascii8uphex((uint32_t)port, hexstr); /* TODO from cdbauthz.c */
    if (  mcdb_findtagstart(&m, hexstr, sizeof(hexstr), (unsigned char)'x')
        && mcdb_findtagnext(&m, hexstr, sizeof(hexstr), (unsigned char)'x')  ) {
/* GPS: FIXME: check proto != NULL and mcdb_findtagnext() until match */

        /* TODO: decode into struct servent */
        /* fail ERANGE only if insufficient buffer space supplied */

        mcdb_unregister(m.map);
        return NSS_STATUS_SUCCESS;
    }
    mcdb_unregister(m.map);
    return NSS_STATUS_NOTFOUND;
}


#include <aliases.h>

/*  ??? (struct aliasent *)   */
enum nss_status
_nss_files_getaliasent_r(struct aliasent * const restrict aliasbuf,
                         char * const restrict buf, const size_t buflen,
                         struct aliasent ** const restrict aliasbufp)
{
    struct mcdb * const m = &nss_mcdb_st[NSS_DBTYPE_ALIASES];
    enum nss_status status =
      _nss_mcdb_getent(m, NSS_DBTYPE_ALIASES, (unsigned char)'=');
    if (status != NSS_STATUS_SUCCESS)
        return status;

    /* TODO: decode into struct aliasent */
    /* fail ERANGE only if insufficient buffer space supplied */
    /* else return NSS_STATUS_SUCCESS */

    return status;
}

/*  ??? (struct aliasent *)   */
enum nss_status
_nss_files_getaliasbyname_r(const char * const restrict name,
                            struct aliasent * const restrict aliasbuf,
                            char * const restrict buf, const size_t buflen,
                            struct aliasent ** const restrict aliasbufp)
{
    struct mcdb m;
    size_t nlen;
    m.map = _nss_mcdb_db_getshared(NSS_DBTYPE_ALIASES);
    if (__builtin_expect(m.map == NULL, false))
        return NSS_STATUS_UNAVAIL;
    nlen = strlen(name);
    if (  mcdb_findtagstart(&m, name, nlen, (unsigned char)'=')
        && mcdb_findtagnext(&m, name, nlen, (unsigned char)'=')  ) {

        /* TODO: decode into struct aliasent */
        /* fail ERANGE only if insufficient buffer space supplied */

        mcdb_unregister(m.map);
        return NSS_STATUS_SUCCESS;
    }
    mcdb_unregister(m.map);
    return NSS_STATUS_NOTFOUND;
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







#if 0
To avoid conflicts, prepend *all* types with a char indicating type
(Do not trust user input that might otherwise match a different type)

/* also netdb.h */  /* need #include <sys/socket.h> for AF_INET */
_nss_files_gethostbyaddr_r
_nss_files_gethostbyname2_r
_nss_files_gethostbyname_r
_nss_files_gethostent_r


/* also netdb.h */
_nss_files_getnetgrent_r

/* also netdb.h */
_nss_files_getnetbyaddr_r
_nss_files_getnetbyname_r
_nss_files_getnetent_r

netinet/ether.h
_nss_files_getetherent_r
_nss_files_gethostton_r  ether_hostton  netinet/ether.h  struct ether_addr
_nss_files_getntohost_r  ether_ntohost  netinet/ether.h

#include <shadow.h>
_nss_files_getspent_r  struct spwd
_nss_files_getspnam_r

_nss_files_getpublickey
_nss_files_getsecretkey

_nss_files_parse_etherent
_nss_files_parse_grent@@GLIBC_PRIVATE
_nss_files_parse_netent
_nss_files_parse_protoent
_nss_files_parse_pwent@@GLIBC_PRIVATE
_nss_files_parse_rpcent
_nss_files_parse_servent
_nss_files_parse_spent@@GLIBC_PRIVATE

_nss_netgroup_parseline

#endif
