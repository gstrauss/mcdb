/*
 * nss_mcdbctl - command line tool to create mcdb of nsswitch.conf databases
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
/* _BSD_SOURCE or _SVID_SOURCE for struct rpcent on Linux */
#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
/* _DARWIN_C_SOURCE for struct rpcent on Darwin */
#define PLASMA_FEATURE_ENABLE_BSD_SOURCE_TO_DARWIN_C_SOURCE
/* large file support needed for stat(),fstat() input file > 2 GB */
#define PLASMA_FEATURE_ENABLE_LARGEFILE

#include "nss_mcdb_make.h"
#include "nss_mcdb_acct.h"
#include "nss_mcdb_acct_make.h"
#include "nss_mcdb_authn.h"
#include "nss_mcdb_authn_make.h"
#if 0  /* implemented, but not enabling by default; little benefit */
#include "nss_mcdb_misc.h"
#include "nss_mcdb_misc_make.h"
#endif
#include "nss_mcdb_netdb.h"
#include "nss_mcdb_netdb_make.h"
#include "../mcdb_makefn.h"
#include "../nointr.h"
#include "../plasma/plasma_stdtypes.h"

#include <sys/stat.h>  /* stat(), fchmod(), umask() */
#include <limits.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>    /* malloc() free() */
#include <stdio.h>     /* rename() */
#include <string.h>    /* memcpy() strlen() */
#include <unistd.h>    /* sysconf() unlink() */

/* Note: blank line is required to denote end of mcdb input 
 * Ensure blank line is written after w.wbuf is flushed. */

int main(void)
{
    enum { DBUFSZ =   4096  /*   4 KB */ };

    struct nss_mcdb_make_winfo w = { .wbuf   = { NULL,NULL,0,0 },
                                     .data   = malloc(DBUFSZ),
                                     .datasz = DBUFSZ };
    struct nss_mcdb_make_wbuf * const wbuf = &w.wbuf;

    const long sc_getpw_r_size_max = sysconf(_SC_GETPW_R_SIZE_MAX);
    const long sc_getgr_r_size_max = sysconf(_SC_GETGR_R_SIZE_MAX);
    const long sc_host_name_max    = sysconf(_SC_HOST_NAME_MAX);

    struct fdb_st {
      const char * const restrict file;
      const char * const restrict mcdbfile;
      size_t datasz;
      bool (*parse)(struct nss_mcdb_make_winfo * restrict,
                    char * restrict, size_t);
      bool (*encode)(struct nss_mcdb_make_winfo * restrict,
                     const void *);
      bool (*flush)(struct nss_mcdb_make_winfo * restrict);
    };

    /* database parse routines and max buffer size required for entry from db
     * (HDRSZ + 1 KB buffer for db that do not specify max buf size) */
    const struct fdb_st fdb[] = {
        { "/etc/shadow",
          "/etc/mcdb/shadow.mcdb",
          NSS_SP_HDRSZ+(size_t)sc_getpw_r_size_max,
          nss_mcdb_authn_make_shadow_parse,
          nss_mcdb_authn_make_spwd_encode,
          NULL },
        { "/etc/passwd",
          "/etc/mcdb/passwd.mcdb",
          NSS_PW_HDRSZ+(size_t)sc_getpw_r_size_max,
          nss_mcdb_acct_make_passwd_parse,
          nss_mcdb_acct_make_passwd_encode,
          NULL },
        { "/etc/group",
          "/etc/mcdb/group.mcdb",
          NSS_GR_HDRSZ+(size_t)sc_getgr_r_size_max,
          nss_mcdb_acct_make_group_parse,
          nss_mcdb_acct_make_group_encode,
          nss_mcdb_acct_make_group_flush },
      #if 0  /* implemented, but not enabling by default; little benefit */
        { "/etc/ethers",
          "/etc/mcdb/ethers.mcdb",
          NSS_EA_HDRSZ+(size_t)sc_host_name_max,
          nss_mcdb_misc_make_ethers_parse,
          nss_mcdb_misc_make_ether_addr_encode,
          NULL },
        { "/etc/aliases",
          "/etc/mcdb/aliases.mcdb",
          NSS_AE_HDRSZ+1024,
          nss_mcdb_misc_make_aliases_parse,
          nss_mcdb_misc_make_aliasent_encode,
          NULL },
      #endif
        { "/etc/hosts",
          "/etc/mcdb/hosts.mcdb",
          NSS_HE_HDRSZ+1024,
          nss_mcdb_netdb_make_hosts_parse,
          nss_mcdb_netdb_make_hostent_encode,
          NULL },
        { "/etc/netgroup",
          "/etc/mcdb/netgroup.mcdb",
          NSS_NG_HDRSZ+1024,
          nss_mcdb_netdb_make_netgroup_parse,
          nss_mcdb_netdb_make_netgrent_encode,
          NULL },
        { "/etc/networks",
          "/etc/mcdb/networks.mcdb",
          NSS_NE_HDRSZ+1024,
          nss_mcdb_netdb_make_networks_parse,
          nss_mcdb_netdb_make_netent_encode,
          NULL },
        { "/etc/protocols",
          "/etc/mcdb/protocols.mcdb",
          NSS_PE_HDRSZ+1024,
          nss_mcdb_netdb_make_protocols_parse,
          nss_mcdb_netdb_make_protoent_encode,
          NULL },
        { "/etc/rpc",
          "/etc/mcdb/rpc.mcdb",
          NSS_RE_HDRSZ+1024,
          nss_mcdb_netdb_make_rpc_parse,
          nss_mcdb_netdb_make_rpcent_encode,
          NULL },
        { "/etc/services",
          "/etc/mcdb/services.mcdb",
          NSS_SE_HDRSZ+1024,
          nss_mcdb_netdb_make_services_parse,
          nss_mcdb_netdb_make_servent_encode,
          NULL }
    };

    struct mcdb_make m;
    struct stat st;
    time_t mtime;
    const time_t mtime_nsswitch =
      (stat("/etc/nsswitch.conf", &st) == 0) ? st.st_mtime : 0;
    bool rc = false;

    if (w.data == NULL) {
        free(w.data);
        return -1;
    }

    /* Arbitrarily limit mcdb line to 32K
     * (32K limit means that integer overflow not possible for int-sized things)
     * (on Linux: _SC_GETPW_R_SIZE_MAX is 1K, _SC_GETGR_R_SIZE_MAX is 1K) */
    if (   sc_getpw_r_size_max <= 0 || SHRT_MAX < sc_getpw_r_size_max
        || sc_getgr_r_size_max <= 0 || SHRT_MAX < sc_getgr_r_size_max
        || sc_host_name_max    <= 0 || SHRT_MAX < sc_host_name_max   ) {
        free(w.data);
        return -1;  /* should not happen */
    }

    /* initialize struct mcdb_make for writing .mcdb  */
    memset((wbuf->m = &m), '\0', sizeof(struct mcdb_make));
    m.fn_malloc = malloc;
    m.fn_free   = free;

    /* parse databases */
    /* (parse /etc/shadow (fdb[0]) only if root, else begin with fdb[1]) */
    for (int i = (0==geteuid() ? 0 : 1);
             i < (int)(sizeof(fdb)/sizeof(struct fdb_st)); ++i) {
        w.datasz = fdb[i].datasz;
        w.encode = fdb[i].encode;
        w.flush  = fdb[i].flush;
        assert(w.datasz <= DBUFSZ);

        /* preserve permission modes if previous mcdb exists; else read-only
         * (since mcdb is *constant* -- not modified -- after creation) */
        rc = false;
        if (stat(fdb[i].file, &st) != 0) {
            if (errno == ENOENT)
                continue; /* skip dbs that do not exist; leave existing mcdb */
            else
                break;
        }
        mtime = st.st_mtime;
        if (stat(fdb[i].mcdbfile, &st) != 0) {
            st.st_mtime = 0;
            st.st_mode = (mode_t)((0 != strcmp(fdb[i].file, "/etc/shadow"))
              ? (S_IRUSR | S_IRGRP | S_IROTH)    /* default read-only */
              : (S_IRUSR));  /* default root read-only for /etc/shadow */
            if (errno != ENOENT)
                break;
        }
        if (mtime < st.st_mtime && mtime_nsswitch < st.st_mtime) {
            rc = true;
            continue;  /* dbfile up-to-date (redo if time matches) */
        }
        if (0 != mcdb_makefn_start(wbuf->m, fdb[i].mcdbfile, malloc, free))
            break;
        wbuf->m->st_mode = st.st_mode;
        rc =   nss_mcdb_make_dbfile(&w, fdb[i].file, fdb[i].parse)
            && mcdb_makefn_finish(wbuf->m, true) == 0;
        mcdb_makefn_cleanup(wbuf->m);
        if (!rc)
            break;
    }

    free(w.data);
    free(wbuf->buf);

    return !rc;
}



/*
 * Sample code that can be used to dump existing databases before modifying
 * /etc/nsswitch.conf to use mcdb, and then compared to mcdb databases.
 *
 * nss_mcdb_acct_make_passwd_ents()
 * nss_mcdb_acct_make_group_ents()
 * nss_mcdb_acct_make_shadow_ents()
 * nss_mcdb_misc_make_aliases_ents()
 * nss_mcdb_netdb_make_hosts_ents()
 * nss_mcdb_netdb_make_networks_ents()
 * nss_mcdb_netdb_make_protocols_ents()
 * nss_mcdb_netdb_make_rpc_ents()
 * nss_mcdb_netdb_make_services_ents()
 */

#define db_set_get_end_ent(dbgr, dbname, type, tag, stayopen)              \
                                                                           \
bool                                                                       \
nss_mcdb_##dbgr##_make_##dbname##_ents(                                    \
  struct nss_mcdb_make_winfo * const restrict w)                           \
{                                                                          \
    struct type * restrict ent;                                            \
                                                                           \
    errno = 0;                                                             \
    set##tag##ent(stayopen);                                               \
    if (errno != 0)                                                        \
        return false;                                                      \
                                                                           \
    errno = 0;                                                             \
    while ( __builtin_expect( (ent = get##tag##ent()) != NULL, 1)          \
         && __builtin_expect( errno == 0, 1)) {                            \
        if (__builtin_expect(!nss_mcdb_##dbgr##_make_##type##_encode(w,ent),0))\
            return false;                                                  \
    }                                                                      \
    if (errno != 0 && errno != ENOENT)                                     \
        return false;                                                      \
                                                                           \
    errno = 0;                                                             \
    end##tag##ent();                                                       \
    if (errno != 0)                                                        \
        return false;                                                      \
                                                                           \
    return true;                                                           \
}                                                                          \

#if 0

#ifndef _XOPEN_SOURCE /* setpwent() getpwent() endpwent() */
#define _XOPEN_SOURCE 600
#endif
/* _BSD_SOURCE or _SVID_SOURCE for struct ether_addr, struct rpcent on Linux */
#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif
/* _ALL_SOURCE for struct rpcent on AIX */
#ifdef _AIX
#ifndef _ALL_SOURCE
#define _ALL_SOURCE
#endif
#endif

#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <shadow.h>         /* not present on AIX */
#include <aliases.h>        /* not portable; see nss_mcdb_misc.h */
#include <netinet/ether.h>  /* not portable; see nss_mcdb_misc.h */
#include <netdb.h>
#ifdef __sun
#include <rpc/rpcent.h>
#endif

db_set_get_end_ent(acct,  passwd,    passwd,   pw,    /**/)
db_set_get_end_ent(acct,  group,     group,    gr,    /**/)
db_set_get_end_ent(acct,  shadow,    spwd,     sp,    /**/)
db_set_get_end_ent(misc,  aliases,   aliasent, alias, /**/)
db_set_get_end_ent(netdb, hosts,     hostent,  host,     1)
db_set_get_end_ent(netdb, networks,  netent,   net,      1)
db_set_get_end_ent(netdb, protocols, protoent, proto,    1)
db_set_get_end_ent(netdb, rpc,       rpcent,   rpc,      1)
db_set_get_end_ent(netdb, services,  servent,  serv,     1)

#endif

/* Note: /etc/ethers has no equivalent setetherent, getetherent, endetherent */
/* Note: /etc/netgroup has no standard routines to walk all netgroups */

#undef db_set_get_end_ent
