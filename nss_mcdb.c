/*
 * nss_mcdb - common routines to query mcdb of nsswitch.conf databases
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

#include "nss_mcdb.h"
#include "mcdb.h"
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

/* Note various thread-safety optimizations are taken below since these routines
 * retain mmap of mcdb files once opened.  Since the mcdb files are not munmap'd
 * there are no race conditions having to do with opening and closing the mcdb.
 * (Opening each mcdb is protect by a mutex.)
 *
 * shadow.mcdb is kept mmap'd, like all other mcdb.  If this is an issue, then
 * modify setspent(), endspent(), and getspnam() to use custom routines instead
 * of the generic routines.  The shadow routines would then mcdb_mmap_create(),
 * use mcdb, and mcdb_mmap_destroy() every getspnam() or {set,get,end}spent().
 * (and -would not- use the static storage (e.g. no use of _nss_mcdb_mmap_st[]))
 */

/* compile-time setting for security
 * /etc/mcdb/ is recommended so that .mcdb are on same partition as flat files
 * (implies that if flat files visible, then likely so are .mcdb equivalents) */
#ifndef NSS_MCDB_DBPATH
#define NSS_MCDB_DBPATH "/etc/mcdb/"
#endif

/* NOTE: path to db must match up to enum nss_dbtype index */
static const char * const restrict _nss_dbnames[] = {
    NSS_MCDB_DBPATH"aliases.mcdb",
    NSS_MCDB_DBPATH"ethers.mcdb",
    NSS_MCDB_DBPATH"group.mcdb",
    NSS_MCDB_DBPATH"hosts.mcdb",
    NSS_MCDB_DBPATH"netgroup.mcdb",
    NSS_MCDB_DBPATH"networks.mcdb",
    NSS_MCDB_DBPATH"passwd.mcdb",
    NSS_MCDB_DBPATH"protocols.mcdb",
    NSS_MCDB_DBPATH"publickey.mcdb",
    NSS_MCDB_DBPATH"rpc.mcdb",
    NSS_MCDB_DBPATH"services.mcdb",
    NSS_MCDB_DBPATH"shadow.mcdb"
};

#define _nss_num_dbs (sizeof(_nss_dbnames)/sizeof(char *))

static struct mcdb_mmap _nss_mcdb_mmap_st[_nss_num_dbs];
static struct mcdb_mmap *_nss_mcdb_mmap[_nss_num_dbs];

/* set*ent(), get*ent(), end*ent() are not thread-safe */
/* (use thread-local storage of static struct mcdb array to increase safety) */

static __thread struct mcdb _nss_mcdb_st[_nss_num_dbs];

#ifdef _FORTIFY_SOURCE
static void _nss_mcdb_atexit(void)
{
    struct mcdb_mmap * restrict map;
    for (uintptr_t i = 0; i < _nss_num_dbs; ++i) {
        map = &_nss_mcdb_mmap_st[i];
        if (   _nss_mcdb_mmap_st[i].ptr != NULL
            || _nss_mcdb_mmap_st[i].fname != NULL  ) {
            mcdb_mmap_destroy(&_nss_mcdb_mmap_st[i]);
            _nss_mcdb_mmap_st[i].ptr = NULL;
            _nss_mcdb_mmap_st[i].fname = NULL;
        }
    }
}
#endif

/* custom free for mcdb_mmap_* routines to not free initial static storage */
static void
_nss_mcdb_mmap_fn_free(void * const v)
{
    /* do not free initial static storage */
    if (   v >=(void *)&_nss_mcdb_mmap_st[0]
        && v < (void *)&_nss_mcdb_mmap_st[_nss_num_dbs]  )
        return;

    /* otherwise free() allocated memory */
    free(v);
}

static bool  __attribute_noinline__
_nss_mcdb_db_openshared(const enum nss_dbtype dbtype)
  __attribute_cold__  __attribute_warn_unused_result__;
static bool  __attribute_noinline__
_nss_mcdb_db_openshared(const enum nss_dbtype dbtype)
{
  #ifdef _THREAD_SAFE
    static pthread_mutex_t _nss_mcdb_global_mutex = PTHREAD_MUTEX_INITIALIZER;
  #endif
    struct mcdb_mmap * const restrict map = &_nss_mcdb_mmap_st[dbtype];
    bool rc;

    if (pthread_mutex_lock(&_nss_mcdb_global_mutex) != 0)
        return false;
    if (__builtin_expect( _nss_mcdb_mmap[dbtype] != 0, false)) {
        /* init'ed by another thread while waiting for mutex */
        pthread_mutex_unlock(&_nss_mcdb_global_mutex);
        return true;
    }

  #ifdef _FORTIFY_SOURCE
    {   static bool atexit_once = true;
        if (atexit_once) { atexit_once = false; atexit(_nss_mcdb_atexit); }   }
  #endif

    /* pass full path in fname instead of separate dirname and basename
     * (not using openat(), fstatat() where someone might close dfd on us)
     * use static storage for initial struct mcdb_mmap for each dbtype
     * and therefore pass custom _nss_mcdb_mmap_fn_free() to not free statics */
    if ((rc = (NULL != mcdb_mmap_create(map, NULL, _nss_dbnames[dbtype],
                                        malloc, _nss_mcdb_mmap_fn_free)))) {
        /*(ought to be preceded by StoreStore memory barrier)*/
        _nss_mcdb_mmap[dbtype] = map;
    }

    pthread_mutex_unlock(&_nss_mcdb_global_mutex);
    return rc;
}

/* Flag to skip mcdb stat() check for protocols, rpc, services databases.
 * Default: enabled
 * (similar to set*ent(int stayopen) flag in glibc for netdb databases)
 * (protocols, rpc, services are unlikely to change frequently in most configs)
 * (hosts and networks database lookups differ: they perform mcdb stat() check)
 * (future: might provide accessor routine to enable/disable)
 * (FreeBSD provides setpassent() and setgroupent() API with stayopen flag) */
static bool _nss_mcdb_stayopen = true;

/* release shared mcdb_mmap */
#define _nss_mcdb_db_relshared(map,flags) \
  mcdb_mmap_thread_registration(&(map),(flags))

/* get shared mcdb_mmap */
static struct mcdb_mmap *
_nss_mcdb_db_getshared(const enum nss_dbtype dbtype,
                       const enum mcdb_flags mcdb_flags)
  __attribute_nonnull__  __attribute_warn_unused_result__;
static struct mcdb_mmap *
_nss_mcdb_db_getshared(const enum nss_dbtype dbtype,
                       const enum mcdb_flags mcdb_flags)
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
          default: (void) mcdb_mmap_refresh_threadsafe(&_nss_mcdb_mmap[dbtype]);
                   break;
        }
    }
    else if (!_nss_mcdb_db_openshared(dbtype))
        return NULL;

    return mcdb_mmap_thread_registration(&_nss_mcdb_mmap[dbtype], mcdb_flags)
      ? _nss_mcdb_mmap[dbtype]
      : NULL;  /* (fails if obtaining mutex fails, i.e. EAGAIN) */
}

nss_status_t  __attribute_noinline__  /*(skip inline into _nss_mcdb_getent)*/
nss_mcdb_setent(const enum nss_dbtype dbtype)
{
    struct mcdb * const restrict m = &_nss_mcdb_st[dbtype];
    const enum mcdb_flags mcdb_flags = MCDB_REGISTER_USE_INCR;
    if (m->map != NULL
        || (m->map = _nss_mcdb_db_getshared(dbtype, mcdb_flags)) != NULL) {
        m->hpos = MCDB_HEADER_SZ;
        return NSS_STATUS_SUCCESS;
    }
    return NSS_STATUS_UNAVAIL;
}

nss_status_t
nss_mcdb_endent(const enum nss_dbtype dbtype)
{
    struct mcdb * const restrict m = &_nss_mcdb_st[dbtype];
    struct mcdb_mmap *map = m->map;
    const enum mcdb_flags mcdb_flags =
        MCDB_REGISTER_USE_DECR | MCDB_REGISTER_MUNMAP_SKIP;
    m->map = NULL;  /* set thread-local ptr NULL even though munmap skipped */
    return (map == NULL || _nss_mcdb_db_relshared(map, mcdb_flags))
      ? NSS_STATUS_SUCCESS
      : NSS_STATUS_UNAVAIL; /* (fails if obtaining mutex fails, i.e. EAGAIN) */
}

/* mcdb get*ent() walks db returning successive keys with '=' tag char */
nss_status_t
nss_mcdb_getent(const enum nss_dbtype dbtype,
                 const struct nss_mcdb_vinfo * const restrict v)
{
    struct mcdb * const restrict m = &_nss_mcdb_st[dbtype];
    unsigned char *map;
    uintptr_t eod;
    uint32_t klen;
    if (__builtin_expect(m->map == NULL, false)
        && nss_mcdb_setent(dbtype) != NSS_STATUS_SUCCESS) {
        *v->errnop = errno;
        return NSS_STATUS_UNAVAIL;
    }
    map = m->map->ptr;
    eod = uint64_strunpack_bigendian_aligned_macro(map) - MCDB_PAD_MASK;
    while (m->hpos < eod) {
        unsigned char * const restrict p = map + m->hpos;
        klen    = uint32_strunpack_bigendian_macro(p);
        m->dlen = uint32_strunpack_bigendian_macro(p+4);
        m->hpos = (m->dpos = (m->kpos = m->hpos + 8) + klen) + m->dlen;
        if (p[8] == (unsigned char)'=')
            /* valid data in mcdb_datapos() mcdb_datalen() mcdb_dataptr() */
            return v->decode(m, v);
    }
    *v->errnop = errno = ENOENT;
    return NSS_STATUS_NOTFOUND;
}

nss_status_t
nss_mcdb_get_generic(const enum nss_dbtype dbtype,
                     const struct nss_mcdb_vinfo * const restrict v)
{
    struct mcdb m;
    nss_status_t status;
    const enum mcdb_flags mcdb_flags_lock =
      MCDB_REGISTER_USE_INCR | MCDB_REGISTER_MUTEX_LOCK_HOLD;

    /* Queries to mcdb are quick, so attempt to reduce locking overhead.
     * Set mcdb_flags to get and release mcdb_global_mutex once instead of
     * grab/release mutex to register, then grab/release mutex to unregister */

    m.map = _nss_mcdb_db_getshared(dbtype, mcdb_flags_lock);
    if (__builtin_expect(m.map == NULL, false)) {
        *v->errnop = errno;
        return NSS_STATUS_UNAVAIL;
    }

    if (  __builtin_expect( mcdb_findtagstart(&m, v->key, v->klen, v->tagc), 1)
        && __builtin_expect( mcdb_findtagnext(&m, v->key, v->klen, v->tagc), 1))
        status = v->decode(&m, v);
    else {
        status = NSS_STATUS_NOTFOUND;
        *v->errnop = errno = ENOENT;
    }

    /* set*ent(),get*ent(),end*end() session not open/reused in current thread*/
    if (_nss_mcdb_st[dbtype].map == NULL) {
        const enum mcdb_flags mcdb_flags_unlock =
            MCDB_REGISTER_USE_DECR
          | MCDB_REGISTER_MUNMAP_SKIP
          | MCDB_REGISTER_MUTEX_UNLOCK_HOLD;
        _nss_mcdb_db_relshared(m.map, mcdb_flags_unlock);
    }

    return status;
}

nss_status_t
nss_mcdb_buf_decode(struct mcdb * const restrict m,
                    const struct nss_mcdb_vinfo * const restrict v)
{   /* generic; simply copy data into target buffer and NIL terminate string */
    const size_t dlen = mcdb_datalen(m);
    if (dlen < v->bufsz) {
        memcpy(v->buf, mcdb_dataptr(m), dlen);
        v->buf[dlen] = '\0';
        return NSS_STATUS_SUCCESS;
    }
    *v->errnop = errno = ERANGE;
    return NSS_STATUS_TRYAGAIN;
}

bool
nss_mcdb_refresh_check(const enum nss_dbtype dbtype)
{
    return (0 <= dbtype && dbtype < NSS_DBTYPE_SENTINEL)
        && mcdb_mmap_refresh_check(&_nss_mcdb_mmap_st[dbtype]);
}
