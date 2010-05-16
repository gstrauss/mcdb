#define _ATFILE_SOURCE     /* fstatat(), openat() */

#include "mcdb.h"
#include "mcdb_uint32.h"
#include "mcdb_attribute.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
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
    if (__builtin_expect(dfd <= STDERR_FILENO, false)
        && (dfd = open(NSS_DBPATH, O_RDONLY, 0)) <= STDERR_FILENO) {
        if (dfd != -1) /* caller must have open STDIN, STDOUT, STDERR */
            while (close(dfd) != 0 && errno == EINTR)
                ;
        pthread_mutex_unlock(&nss_mcdb_global_mutex);
        return false;
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
_nss_mcdb_getent(struct mcdb * const restrict m, const unsigned char tagc)
  __attribute_nonnull__  __attribute_warn_unused_result__;
static enum nss_status
_nss_mcdb_getent(struct mcdb * const restrict m, const unsigned char tagc)
{
    unsigned char *map;
    uint32_t eod;
    uint32_t klen;
    if (m->map == NULL) /* should not happen if _nss_mscb_setent() successful */
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


/* set*ent(), get*ent(), end*ent() are not thread-safe */
/* (use thread-local storage of static struct mcdb array to increase safety) */

static __thread struct mcdb nss_mcdb_st[sizeof(nss_dbnames)/sizeof(char *)];

void _nss_files_setaliasent(void)
{ _nss_mcdb_setent(&nss_mcdb_st[NSS_DBTYPE_ALIASES],   NSS_DBTYPE_ALIASES); }
void _nss_files_setetherent(void)
{ _nss_mcdb_setent(&nss_mcdb_st[NSS_DBTYPE_ETHERS],    NSS_DBTYPE_ETHERS); }
void _nss_files_setgrent(void)
{ _nss_mcdb_setent(&nss_mcdb_st[NSS_DBTYPE_GROUP],     NSS_DBTYPE_GROUP); }
void _nss_files_sethostent(void)
{ _nss_mcdb_setent(&nss_mcdb_st[NSS_DBTYPE_HOSTS],     NSS_DBTYPE_HOSTS); }
void _nss_files_setnetgrent(void)
{ _nss_mcdb_setent(&nss_mcdb_st[NSS_DBTYPE_NETGROUP],  NSS_DBTYPE_NETGROUP); }
void _nss_files_setnetent(void)
{ _nss_mcdb_setent(&nss_mcdb_st[NSS_DBTYPE_NETWORKS],  NSS_DBTYPE_NETWORKS); }
void _nss_files_setpwent(void)
{ _nss_mcdb_setent(&nss_mcdb_st[NSS_DBTYPE_PASSWD],    NSS_DBTYPE_PASSWD); }
void _nss_files_setprotoent(void)
{ _nss_mcdb_setent(&nss_mcdb_st[NSS_DBTYPE_PROTOCOLS], NSS_DBTYPE_PROTOCOLS); }
void _nss_files_setrpcent(void)
{ _nss_mcdb_setent(&nss_mcdb_st[NSS_DBTYPE_RPC],       NSS_DBTYPE_RPC); }
void _nss_files_setservent(void)
{ _nss_mcdb_setent(&nss_mcdb_st[NSS_DBTYPE_SERVICES],  NSS_DBTYPE_SERVICES); }
void _nss_files_setspent(void)
{ _nss_mcdb_setent(&nss_mcdb_st[NSS_DBTYPE_SHADOW],    NSS_DBTYPE_SHADOW); }

void _nss_files_endaliasent(void)
{ _nss_mcdb_endent(&nss_mcdb_st[NSS_DBTYPE_ALIASES]); }
void _nss_files_endetherent(void)
{ _nss_mcdb_endent(&nss_mcdb_st[NSS_DBTYPE_ETHERS]); }
void _nss_files_endgrent(void)
{ _nss_mcdb_endent(&nss_mcdb_st[NSS_DBTYPE_GROUP]); }
void _nss_files_endhostent(void)
{ _nss_mcdb_endent(&nss_mcdb_st[NSS_DBTYPE_HOSTS]); }
void _nss_files_endnetgrent(void)
{ _nss_mcdb_endent(&nss_mcdb_st[NSS_DBTYPE_NETGROUP]); }
void _nss_files_endnetent(void)
{ _nss_mcdb_endent(&nss_mcdb_st[NSS_DBTYPE_NETWORKS]); }
void _nss_files_endpwent(void)
{ _nss_mcdb_endent(&nss_mcdb_st[NSS_DBTYPE_PASSWD]); }
void _nss_files_endprotoent(void)
{ _nss_mcdb_endent(&nss_mcdb_st[NSS_DBTYPE_PROTOCOLS]); }
void _nss_files_endrpcent(void)
{ _nss_mcdb_endent(&nss_mcdb_st[NSS_DBTYPE_RPC]); }
void _nss_files_endservent(void)
{ _nss_mcdb_endent(&nss_mcdb_st[NSS_DBTYPE_SERVICES]); }
void _nss_files_endspent(void)
{ _nss_mcdb_endent(&nss_mcdb_st[NSS_DBTYPE_SHADOW]); }


/* TODO might static struct mcdb for each database type
 * and for first use, not bother malloc a new structure
 * That would lead to race conditions in threaded program.
 * unless also protected by a mutex.
 */
/* TODO might make all *getent have '=' string and set tagc == '=' */


#if 0
To avoid conflicts, prepend *all* types with a char indicating type
(Do not trust user input that might otherwise match a different type)

_nss_files_getaliasbyname_r
_nss_files_getaliasent_r

_nss_files_getetherent_r

_nss_files_getgrent_r
_nss_files_getgrgid_r
_nss_files_getgrnam_r

_nss_files_gethostbyaddr_r
_nss_files_gethostbyname2_r
_nss_files_gethostbyname_r
_nss_files_gethostent_r

_nss_files_gethostton_r
_nss_files_getntohost_r

_nss_files_getnetbyaddr_r
_nss_files_getnetbyname_r
_nss_files_getnetent_r

_nss_files_getnetgrent_r

_nss_files_getprotobyname_r
_nss_files_getprotobynumber_r
_nss_files_getprotoent_r

_nss_files_getpwent_r
_nss_files_getpwnam_r
_nss_files_getpwuid_r

_nss_files_getrpcbyname_r
_nss_files_getrpcbynumber_r
_nss_files_getrpcent_r

_nss_files_getservbyname_r
_nss_files_getservbyport_r
_nss_files_getservent_r

_nss_files_getspent_r
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
