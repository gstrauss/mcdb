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

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#include "nss_mcdb.h"
#include "../plasma/plasma_membar.h"
#include "../plasma/plasma_stdtypes.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
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

/* NOTE: path to db must match up to enum nss_dbtype index
 * (static two-dimensional array instead of ptrs to reduce num DSO relocations)
 * (+14 for longest name, e.g. "protocols.mcdb") */
static const char
  _nss_dbnames[NSS_DBTYPE_SENTINEL][sizeof(NSS_MCDB_DBPATH)+14] = {
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

#define _nss_num_dbs NSS_DBTYPE_SENTINEL

static struct mcdb_mmap _nss_mcdb_mmap_st[_nss_num_dbs];
static struct mcdb_mmap *_nss_mcdb_mmap[_nss_num_dbs];

/* set*ent(), get*ent(), end*ent() are not thread-safe */
/* (use thread-local storage of static struct mcdb array to increase safety) */

#if !(defined(__APPLE__) && defined(__MACH__) \
      && defined(__GNUC__) && !defined(__clang))
static __thread struct mcdb _nss_mcdb_st[_nss_num_dbs];
#else
/* gcc 4.2.1 on Mac OSX does not support __thread thread-local storage;
 * disable in order that the rest of mcdb may compile
 * (currently harmless; nss_mcdb.c is not currently used on Mac OSX) */
static struct mcdb _nss_mcdb_st[_nss_num_dbs];
#endif

#ifdef _FORTIFY_SOURCE
static void _nss_mcdb_atexit(void)
{
    struct mcdb_mmap * restrict map;
    for (uintptr_t i = 0; i < _nss_num_dbs; ++i) {
        map = &_nss_mcdb_mmap_st[i];
        if (map->ptr || map->fname)
            mcdb_mmap_destroy_h(map);
    }
}
#endif

__attribute_cold__
__attribute_noinline__
__attribute_warn_unused_result__
static bool
_nss_mcdb_db_openshared(const enum nss_dbtype dbtype);

__attribute_noinline__
static bool
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
     * to avoid malloc allocation in short-lived programs, and to maintain
     * a reference to maps to keep them available. */
    if ((rc = (NULL != mcdb_mmap_create_h(map, NULL, _nss_dbnames[dbtype],
                                          malloc, free)))) {
        plasma_membar_StoreStore();
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
#define _nss_mcdb_db_relshared(map) \
  mcdb_mmap_thread_registration_h(&(map), MCDB_REGISTER_USE_DECR)

/* get shared mcdb_mmap */
__attribute_regparm__((1))
__attribute_warn_unused_result__
static struct mcdb_mmap *
_nss_mcdb_db_getshared(const enum nss_dbtype dbtype);

__attribute_regparm__((1))
static struct mcdb_mmap *
_nss_mcdb_db_getshared(const enum nss_dbtype dbtype)
{
    /* reuse set*ent(),get*ent(),end*end() session if open in current thread */
    if (_nss_mcdb_st[dbtype].map != NULL)
        return _nss_mcdb_st[dbtype].map;

    /* check if db is open and up-to-date, or else open db
     * (continue with open database if failure refreshing) */
    if (__builtin_expect(_nss_mcdb_mmap[dbtype] != 0, true)) {
        plasma_membar_ld_datadep(); /*(thread ld order dep access map struct)*/
        /* protocols, rpc, services unlikely to change often in most configs
         * Therefore, skip stat() check if configured (default skips stat())
         * (Implication: must restart nscd if any of these three files change)*/
        switch (dbtype) {
          case NSS_DBTYPE_PROTOCOLS:
          case NSS_DBTYPE_RPC:
          case NSS_DBTYPE_SERVICES: if (_nss_mcdb_stayopen) break;
          default:
            /*(void)mcdb_mmap_refresh_threadsafe(&_nss_mcdb_mmap[dbtype]);*/
            (void)(__builtin_expect(
               !mcdb_mmap_refresh_check_h(_nss_mcdb_mmap[dbtype]), true)
               ||  __builtin_expect(
               mcdb_mmap_reopen_threadsafe_h(&_nss_mcdb_mmap[dbtype]), true));
            break;
        }
    }
    else if (!_nss_mcdb_db_openshared(dbtype))
        return NULL;

    return mcdb_mmap_thread_registration_h(&_nss_mcdb_mmap[dbtype],
                                           MCDB_REGISTER_USE_INCR);
    /* (fails only if obtaining mutex fails, i.e. EAGAIN; should not happen) */
}

__attribute_noinline__ /*(skip _nss_mcdb_setent inline)*/
INTERNAL nss_status_t
nss_mcdb_setent(const enum nss_dbtype dbtype,
                const int stayopen  __attribute_unused__)
{
    struct mcdb * const restrict m = &_nss_mcdb_st[dbtype];
    if (m->map != NULL || (m->map = _nss_mcdb_db_getshared(dbtype)) != NULL) {
        m->hpos = (uintptr_t)(m->map->ptr + MCDB_HEADER_SZ);
        return NSS_STATUS_SUCCESS;
    }
    return NSS_STATUS_UNAVAIL;
}

INTERNAL nss_status_t
nss_mcdb_endent(const enum nss_dbtype dbtype)
{
    struct mcdb * const restrict m = &_nss_mcdb_st[dbtype];
    struct mcdb_mmap *map = m->map;
    m->map = NULL;  /* set thread-local ptr NULL */
    return (map == NULL || _nss_mcdb_db_relshared(map))
      ? NSS_STATUS_SUCCESS
      : NSS_STATUS_UNAVAIL; /* (fails if obtaining mutex fails, i.e. EAGAIN) */
}

/* mcdb get*ent() walks db returning successive keys with '=' tag char */
INTERNAL nss_status_t
nss_mcdb_getent(const enum nss_dbtype dbtype,
                const struct nss_mcdb_vinfo * const restrict v)
{
    struct mcdb_iter iter;
    struct mcdb * const m = &_nss_mcdb_st[dbtype];
    if (__builtin_expect(m->map == NULL, false)
        && nss_mcdb_setent(dbtype,0) != NSS_STATUS_SUCCESS) {
        *v->errnop = errno;
        return NSS_STATUS_UNAVAIL;
    }
    mcdb_iter_init_h(&iter, m);
    iter.ptr = (unsigned char *)m->hpos;
    while (mcdb_iter_h(&iter)) {
        if (mcdb_iter_keyptr(&iter)[0] == (unsigned char)'=') {
            m->hpos = (uintptr_t)iter.ptr;
            /* valid data for mcdb_datapos() mcdb_datalen() mcdb_dataptr() */
            m->dpos = (uintptr_t)mcdb_iter_datapos(&iter);
            m->dlen = mcdb_iter_datalen(&iter);
            return v->decode(m, v);
        }
    }
    m->hpos = (uintptr_t)iter.ptr;
    *v->errnop = errno = ENOENT;
    return NSS_STATUS_NOTFOUND;
}

INTERNAL nss_status_t
nss_mcdb_getentstart(const enum nss_dbtype dbtype,
                     const struct nss_mcdb_vinfo * const restrict v)
{
    struct mcdb * const m = &_nss_mcdb_st[dbtype];
    if (__builtin_expect( nss_mcdb_setent(dbtype,0) != NSS_STATUS_SUCCESS, 0)) {
        *v->errnop = errno;
        return NSS_STATUS_UNAVAIL;
    }
    /* query for key; subsequent calls to nss_mcdb_getentnext() reuse keyptr */
    if (  __builtin_expect( mcdb_findtagstart_h(m,v->key,v->klen,v->tagc), 1)
        && __builtin_expect( mcdb_findtagnext_h(m,v->key,v->klen,v->tagc), 1))
        return NSS_STATUS_SUCCESS;
    *v->errnop = errno = ENOENT;
    return NSS_STATUS_NOTFOUND;
}

INTERNAL nss_status_t
nss_mcdb_getnetgrentnext(const struct nss_mcdb_vinfo * const restrict v)
{
    struct mcdb * const restrict m = &_nss_mcdb_st[NSS_DBTYPE_NETGROUP];
    if (__builtin_expect(m->map == NULL, false)) { /* db must already be open */
        *v->errnop = errno;
        return NSS_STATUS_UNAVAIL;
    }
    if (m->loop != 0) {                   /* prior search must have succeeded */
        return v->decode(m, v);                /* decode previous found entry */
        /* getnetgrent iters through rules in data of provided netgroup;
         * does not iter through netgroup keys */
    }
    *v->errnop = errno = ENOENT;
    return NSS_STATUS_NOTFOUND;
}

#if 0  /* implemented, but not enabling by default; often used only with NIS+ */
INTERNAL nss_status_t
nss_mcdb_getentnext(const enum nss_dbtype dbtype,
                    const struct nss_mcdb_vinfo * const restrict v)
{
    struct mcdb * const restrict m = &_nss_mcdb_st[dbtype];
    if (__builtin_expect(m->map == NULL, false)) { /* db must already be open */
        *v->errnop = errno;
        return NSS_STATUS_UNAVAIL;
    }
    if (m->loop != 0) {                   /* prior search must have succeeded */
        nss_status_t status = v->decode(m, v); /* decode previous found entry */
        if (status != NSS_STATUS_SUCCESS)
            return status;
        /*(safe to use keyptr since mcdb is held open during getent iteration)*/
        status = mcdb_findtagnext_h(m, (char *)mcdb_keyptr(m),
                                               mcdb_keylen(m), v->tagc);
        /*(ignore status until next call; m->loop == 0 if not found)*/
        return NSS_STATUS_SUCCESS;
    }
    *v->errnop = errno = ENOENT;
    return NSS_STATUS_NOTFOUND;
}
#endif

INTERNAL nss_status_t
nss_mcdb_get_generic(const enum nss_dbtype dbtype,
                     const struct nss_mcdb_vinfo * const restrict v)
{
    struct mcdb m;
    nss_status_t status;

    m.map = _nss_mcdb_db_getshared(dbtype);
    if (__builtin_expect(m.map == NULL, false)) {
        *v->errnop = errno;
        return NSS_STATUS_UNAVAIL;
    }

    if (  __builtin_expect( mcdb_findtagstart_h(&m,v->key,v->klen,v->tagc), 1)
        && __builtin_expect( mcdb_findtagnext_h(&m,v->key,v->klen,v->tagc), 1))
        status = v->decode(&m, v);
    else {
        status = NSS_STATUS_NOTFOUND;
        *v->errnop = errno = ENOENT;
    }

    /* set*ent(),get*ent(),end*end() session not open/reused in current thread*/
    if (_nss_mcdb_st[dbtype].map == NULL)
        _nss_mcdb_db_relshared(m.map);

    return status;
}

INTERNAL nss_status_t
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
    return (0 <= (int)dbtype && dbtype < NSS_DBTYPE_SENTINEL)
        && _nss_mcdb_mmap[dbtype] != NULL
        && mcdb_mmap_refresh_check_h(_nss_mcdb_mmap[dbtype]);
}
