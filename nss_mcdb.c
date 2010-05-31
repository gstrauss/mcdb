#ifndef _ATFILE_SOURCE /* fstatat(), openat() */
#define _ATFILE_SOURCE
#endif

#include "nss_mcdb.h"
#include "mcdb.h"
#include "nointr.h"
#include "uint32.h"
#include "code_attributes.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _THREAD_SAFE
#include <pthread.h>       /* pthread_mutex_t, pthread_mutex_{lock,unlock}() */
#else
#define pthread_mutex_lock(mutexp) 0
#define pthread_mutex_unlock(mutexp) (void)0
#endif

#ifndef O_CLOEXEC /* O_CLOEXEC available since Linux 2.6.23 */
#define O_CLOEXEC 0
#endif

/*
 * man nsswitch.conf
 *     /etc/nsswitch.conf
 *     /lib/libnss_XXXXX.so.2, e.g. /lib/libnss_files.so.2
 *     rpm -qil nss_db nss_ldap
 * The GNU C Library (manual)
 * http://www.gnu.org/s/libc/manual/html_node/Name-Service-Switch.html
 * http://www.gnu.org/s/libc/manual/html_node/NSS-Module-Function-Internals.html
 *   28.4.2 Internals of the NSS Module Functions
 */

/* compile-time setting for security */
#ifndef NSS_DBPATH
#define NSS_DBPATH "/var/db/"
#endif

/* path to db must match up to enum nss_dbtype index */
/* NOTE: str must fit in struct mcdb_mmap->fname array (currently 64 bytes) */
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


/* custom free for mcdb_mmap_create() to not free initial static storage */
static void
_nss_mcdb_mmap_free(void * const v)
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
    map->fn_free   = _nss_mcdb_mmap_free;

  #if defined(__linux) || defined(__sun)
    if (__builtin_expect(dfd <= STDERR_FILENO, false)) {
        if ((dfd=nointr_open(NSS_DBPATH,O_RDONLY|O_CLOEXEC,0)) > STDERR_FILENO){
            if (O_CLOEXEC == 0)
                (void) fcntl(dfd, F_SETFD, FD_CLOEXEC);
            map->dfd = dfd;
        }
        else {
            if (dfd != -1) /* caller must have open STDIN, STDOUT, STDERR */
                (void) nointr_close(dfd);
            pthread_mutex_unlock(&_nss_mcdb_global_mutex);
            errno = EBADF;
            return false;
        }
    }
    /* assert(sizeof(map->fname) > strlen(_nss_dbnames[dbtype])); */
    memcpy(map->fname, _nss_dbnames[dbtype], strlen(_nss_dbnames[dbtype])+1);
  #else
    if (snprintf(map->fname, sizeof(map->fname), "%s%s",
                 NSS_DBPATH, _nss_dbnames[dbtype]) >= sizeof(map->fname)) {
        pthread_mutex_unlock(&_nss_mcdb_global_mutex);
        errno = ENAMETOOLONG;
        return false;
    }
  #endif

  #if defined(__linux) || defined(__sun)
    if ((fd = nointr_openat(dfd, map->fname, O_RDONLY|O_NONBLOCK, 0)) != -1)
  #else
    if ((fd = nointr_open(map->fname, O_RDONLY|O_NONBLOCK, 0)) != -1)
  #endif
    {
        rc = mcdb_mmap_init(map, fd);
        (void) nointr_close(fd); /* close fd once mmap'ed */
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
#define _nss_mcdb_db_relshared(map,flags) mcdb_register_access(&(map),(flags))

/* get shared mcdb_mmap */
static struct mcdb_mmap *
_nss_mcdb_db_getshared(const enum nss_dbtype dbtype,
                       const enum mcdb_register_flags mcdb_flags)
  __attribute_nonnull__  __attribute_warn_unused_result__;
static struct mcdb_mmap *
_nss_mcdb_db_getshared(const enum nss_dbtype dbtype,
                       const enum mcdb_register_flags mcdb_flags)
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

    return mcdb_register_access(&_nss_mcdb_mmap[dbtype], mcdb_flags)
      ? _nss_mcdb_mmap[dbtype]
      : NULL;
}

enum nss_status  __attribute_noinline__  /*(skip inline into _nss_mcdb_getent)*/
_nss_mcdb_setent(const enum nss_dbtype dbtype)
{
    struct mcdb * const restrict m = &_nss_mcdb_st[dbtype];
    const enum mcdb_register_flags mcdb_flags = MCDB_REGISTER_USE;
    if (m->map != NULL
        || (m->map = _nss_mcdb_db_getshared(dbtype, mcdb_flags)) != NULL) {
        m->hpos = 0;
        return NSS_STATUS_SUCCESS;
    }
    return NSS_STATUS_UNAVAIL;
}

enum nss_status
_nss_mcdb_endent(const enum nss_dbtype dbtype)
{
    struct mcdb * const restrict m = &_nss_mcdb_st[dbtype];
    const enum mcdb_register_flags mcdb_flags =
        MCDB_REGISTER_DONE | MCDB_REGISTER_MUNMAP_SKIP;
    return (m->map == NULL || _nss_mcdb_db_relshared(m->map, mcdb_flags))
      ? NSS_STATUS_SUCCESS
      : NSS_STATUS_RETURN; /* (fails only if obtaining mutex fails) */
}

/* mcdb get*ent() walks db returning successive keys with '=' tag char */
enum nss_status
_nss_mcdb_getent(const enum nss_dbtype dbtype,
                 const struct _nss_vinfo * const restrict vinfo)
{
    struct mcdb * const restrict m = &_nss_mcdb_st[dbtype];
    unsigned char *map;
    uint32_t eod;
    uint32_t klen;
    if (__builtin_expect(m->map == NULL, false)
        && _nss_mcdb_setent(dbtype) != NSS_STATUS_SUCCESS) {
        *vinfo->errnop = errno;
        return NSS_STATUS_UNAVAIL;
    }
    map = m->map->ptr;
    eod = uint32_strunpack_bigendian_aligned_macro(map) - 7;
    while (m->hpos < eod) {
        unsigned char * const restrict p = map + m->hpos;
        klen    = uint32_strunpack_bigendian_macro(p);
        m->dlen = uint32_strunpack_bigendian_macro(p+4);
        m->hpos = (m->dpos = (m->kpos = m->hpos + 8) + klen) + m->dlen;
        if (p[8+klen] == (unsigned char)'=')
            /* valid data in mcdb_datapos() mcdb_datalen() mcdb_dataptr() */
            return vinfo->decode(m, NULL, vinfo);
    }
    *vinfo->errnop = errno = ENOENT;
    return NSS_STATUS_NOTFOUND;
}

enum nss_status
_nss_mcdb_get_generic(const enum nss_dbtype dbtype,
                      const struct _nss_kinfo * const restrict kinfo,
                      const struct _nss_vinfo * const restrict vinfo)
{
    struct mcdb m;
    enum nss_status status = NSS_STATUS_NOTFOUND;
    const enum mcdb_register_flags mcdb_flags_lock =
      MCDB_REGISTER_USE | MCDB_REGISTER_MUTEX_LOCK_HOLD;

    /* Queries to mcdb are quick, so attempt to reduce locking overhead.
     * Set mcdb_flags to get and release mcdb_global_mutex once instead of
     * grab/release mutex to register, then grab/release mutex to unregister */

    m.map = _nss_mcdb_db_getshared(dbtype, mcdb_flags_lock);
    if (__builtin_expect(m.map == NULL, false)) {
        *vinfo->errnop = errno;
        return NSS_STATUS_UNAVAIL;
    }

    if (  mcdb_findtagstart(&m, kinfo->key, kinfo->klen, kinfo->tagc)
        && mcdb_findtagnext(&m, kinfo->key, kinfo->klen, kinfo->tagc)  )
        status = vinfo->decode(&m, kinfo, vinfo);

    /* set*ent(),get*ent(),end*end() session not open/reused in current thread*/
    if (_nss_mcdb_st[dbtype].map == NULL) {
        const enum mcdb_register_flags mcdb_flags_unlock =
            MCDB_REGISTER_DONE
          | MCDB_REGISTER_MUNMAP_SKIP
          | MCDB_REGISTER_MUTEX_UNLOCK_HOLD;
        _nss_mcdb_db_relshared(m.map, mcdb_flags_unlock);
    }

    return status;
}

enum nss_status
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
    *vinfo->errnop = errno = ERANGE;
    return NSS_STATUS_TRYAGAIN;
}


#if 0
-----
-----
NOTES
-----
-----

TODO: split each set of nss database access routines into a separate .c file
      (separate, too, reading and creation/writing)
      (store offsets for serialized record in header file)

/* GPS: verify what nss passes in and what it wants as exit values
 * It probably always wants enum nss_status
 *
 * If gethost* buffer sizes are not large enough:
 *   Set errno = ERANGE; return NSS_STATUS_TRYAGAIN;
 */

GPS: how about a mcdb for /etc/nsswitch.conf ?
     (would need to modify glibc to support it)
     (see if glibc can be modified to use mmap and not do extra read() calls)
       (check latest versions before working on patch)
GPS: where does nscd socket get plugged in to all this?
     and should I add something to nscd.conf to implicitly use mcdb
     before its cache?

/* TODO create an atexit() routine to walk static maps and munmap each map
 * (might only do if compiled with -D_FORTIFY_SOURCE or something so that we
 *  free() memory allocated to demonstrate lack of leaking)
 * Set flag in _nss_mcdb_db_openshared() so atexit() is run only once.
 */

#endif
