/* _BSD_SOURCE or _SVID_SOURCE for struct rpcent on Linux */
#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif

#include "nss_mcdb_make.h"
#include "nss_mcdb_acct.h"
#include "nss_mcdb_acct_make.h"
#include "nss_mcdb_misc.h"
#include "nss_mcdb_misc_make.h"
#include "nss_mcdb_netdb.h"
#include "nss_mcdb_netdb_make.h"
#include "nointr.h"

#include <sys/types.h>
#include <sys/stat.h>  /* stat(), fchmod(), umask() */
#include <limits.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>    /* malloc() free() mkstemp() */
#include <stdio.h>     /* rename() */
#include <string.h>    /* memcpy() strlen() */
#include <unistd.h>    /* sysconf() unlink() */

/* Note: blank line is required to denote end of mcdb input 
 * Ensure blank line is written after w.wbuf is flushed. */

int main(void)
{
    /* WBUFSZ must be >= ((largest record possible * 2) + 26) */
    enum { WBUFSZ = 524288  /* 512 KB */ };
    enum { DBUFSZ =   4096  /*   4 KB */ };

    struct nss_mcdb_make_winfo w = { .wbuf   = { NULL,
                                                 malloc(WBUFSZ), 0, WBUFSZ,
                                                 STDOUT_FILENO },
                                     .data   = malloc(DBUFSZ),
                                     .datasz = DBUFSZ };
    struct nss_mcdb_make_wbuf * const wbuf = &w.wbuf;

    const long sc_getpw_r_size_max = sysconf(_SC_GETPW_R_SIZE_MAX);
    const long sc_getgr_r_size_max = sysconf(_SC_GETGR_R_SIZE_MAX);
    const long sc_host_name_max    = sysconf(_SC_HOST_NAME_MAX);

    struct fdb_st {
      const char * const restrict file;
      const char * const restrict mcdbfile;
      const char * const restrict mcdbmakefile;
      size_t datasz;
      bool (*parse)(struct nss_mcdb_make_winfo * restrict,
                    char * restrict);
      bool (*encode)(struct nss_mcdb_make_winfo * restrict,
                     const void * restrict);
      bool (*flush)(struct nss_mcdb_make_winfo * restrict);
    };

    /* database parse routines and max buffer size required for entry from db
     * (HDRSZ + 1 KB buffer for db that do not specify max buf size) */
    const struct fdb_st fdb[] = {
        { "/etc/shadow",
          "/etc/mcdb/shadow.mcdb",
          "/etc/mcdb/shadow.mcdbmake",
          NSS_SP_HDRSZ+(size_t)sc_getpw_r_size_max,
          nss_mcdb_acct_make_shadow_parse,
          nss_mcdb_acct_make_spwd_encode,
          NULL },
        { "/etc/passwd",
          "/etc/mcdb/passwd.mcdb",
          "/etc/mcdb/passwd.mcdbmake",
          NSS_PW_HDRSZ+(size_t)sc_getpw_r_size_max,
          nss_mcdb_acct_make_passwd_parse,
          nss_mcdb_acct_make_passwd_encode,
          NULL },
        { "/etc/group",
          "/etc/mcdb/group.mcdb",
          "/etc/mcdb/group.mcdbmake",
          NSS_GR_HDRSZ+(size_t)sc_getgr_r_size_max,
          nss_mcdb_acct_make_group_parse,
          nss_mcdb_acct_make_group_encode,
          nss_mcdb_acct_make_group_flush },
      #if 0  /* implemented, but not enabling by default; little benefit */
        { "/etc/ethers",
          "/etc/mcdb/ethers.mcdb",
          "/etc/mcdb/ethers.mcdbmake",
          NSS_EA_HDRSZ+(size_t)sc_host_name_max,
          nss_mcdb_misc_make_ethers_parse,
          nss_mcdb_misc_make_ether_addr_encode,
          NULL },
        { "/etc/aliases",
          "/etc/mcdb/aliases.mcdb",
          "/etc/mcdb/aliases.mcdbmake",
          NSS_AE_HDRSZ+1024,
          nss_mcdb_misc_make_aliases_parse,
          nss_mcdb_misc_make_aliasent_encode,
          NULL },
      #endif
        { "/etc/hosts",
          "/etc/mcdb/hosts.mcdb",
          "/etc/mcdb/hosts.mcdbmake",
          NSS_HE_HDRSZ+1024,
          nss_mcdb_netdb_make_hosts_parse,
          nss_mcdb_netdb_make_hostent_encode,
          NULL },
        { "/etc/networks",
          "/etc/mcdb/networks.mcdb",
          "/etc/mcdb/networks.mcdbmake",
          NSS_NE_HDRSZ+1024,
          nss_mcdb_netdb_make_networks_parse,
          nss_mcdb_netdb_make_netent_encode,
          NULL },
        { "/etc/protocols",
          "/etc/mcdb/protocols.mcdb",
          "/etc/mcdb/protocols.mcdbmake",
          NSS_PE_HDRSZ+1024,
          nss_mcdb_netdb_make_protocols_parse,
          nss_mcdb_netdb_make_protoent_encode,
          NULL },
        { "/etc/rpc",
          "/etc/mcdb/rpc.mcdb",
          "/etc/mcdb/rpc.mcdbmake",
          NSS_RE_HDRSZ+1024,
          nss_mcdb_netdb_make_rpc_parse,
          nss_mcdb_netdb_make_rpcent_encode,
          NULL },
        { "/etc/services",
          "/etc/mcdb/services.mcdb",
          "/etc/mcdb/services.mcdbmake",
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
    size_t len;
    int fd = -1;
    bool rc = false;
    const char * restrict fname;
    char fnametmp[64];

    if (wbuf->buf == NULL || w.data == NULL) {
        free(w.data);
        free(wbuf->buf);
        return -1;
    }

    /* Arbitrarily limit mcdb line to 32K
     * (32K limit means that integer overflow not possible for int-sized things)
     * (on Linux: _SC_GETPW_R_SIZE_MAX is 1K, _SC_GETGR_R_SIZE_MAX is 1K) */
    if (   sc_getpw_r_size_max <= 0 || SHRT_MAX < sc_getpw_r_size_max
        || sc_getgr_r_size_max <= 0 || SHRT_MAX < sc_getgr_r_size_max
        || sc_host_name_max    <= 0 || SHRT_MAX < sc_host_name_max   ) {
        free(w.data);
        free(wbuf->buf);
        return -1;  /* should not happen */
    }

    /* (mcdb line must fit in WBUFSZ, including key, value, mcdb line tokens) */
    assert(DBUFSZ*2 <= WBUFSZ);

    /* initialize struct mcdb_make for writing .mcdb 
     * (comment out to write .mcdbmake input files) */
    memset((wbuf->m = &m), '\0', sizeof(struct mcdb_make));
    m.fn_malloc = malloc;
    m.fn_free   = free;

    /* parse databases */
    /* (file mgmt code is similar to mcdb_makefmt.c:mcdb_makefmt_fdintofile())*/
    /* (parse /etc/shadow (fdb[0]) only if root, else begin with fdb[1]) */
    for (int i=(0==geteuid()?0:1); i < sizeof(fdb)/sizeof(struct fdb_st); ++i) {
        fname    = wbuf->m != NULL ? fdb[i].mcdbfile : fdb[i].mcdbmakefile;
        w.datasz = fdb[i].datasz;
        w.encode = fdb[i].encode;
        w.flush  = fdb[i].flush;
        assert(w.datasz <= DBUFSZ);

        /* preserve permission modes if previous mcdb exists; else read-only
         * (since mcdb is *constant* -- not modified -- after creation) */
        rc = false;
        if (stat(fdb[i].file, &st) != 0)
            break;
        mtime = st.st_mtime;
        if (stat(fname, &st) != 0) {
            st.st_mtime = 0;
            st.st_mode = (0 != strcmp("/etc/shadow", fdb[i].file))
              ? S_IRUSR | S_IRGRP | S_IROTH    /* default read-only */
              : S_IRUSR;  /* default root read-only for /etc/shadow */
            if (errno != ENOENT)
                break;
        }
        if (mtime < st.st_mtime && mtime_nsswitch < st.st_mtime) {
            rc = true;
            continue;  /* dbfile up-to-date (redo if time matches) */
        }
        len = strlen(fname);
        if (len + 8 > sizeof(fnametmp))
            break;
        memcpy(fnametmp, fname, len);
        memcpy(fnametmp+len, ".XXXXXX", 8);
        if ((fd = mkstemp(fnametmp)) == -1)
            break;
        wbuf->fd = fd;

        rc =   nss_mcdb_make_dbfile(&w, fdb[i].file, fdb[i].parse)
            && fchmod(fd, st.st_mode) == 0
            && nointr_close(fd) == 0    /* NFS might report write errors here */
            && (fd = -2,                /* (fd=-2 so not re-closed)*/
                rename(fnametmp, fname) == 0);
        if (!rc)
            break;
        fd = -1;
    }

    if (fd != -1) {
        const int errnum = errno;
        unlink(fnametmp);
        if (fd >= 0)
            (void)nointr_close(fd);
        if (wbuf->m != NULL)
            mcdb_make_destroy(wbuf->m);
        errno = errnum;
        rc = false;
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
#define _XOPEN_SOURCE 500
#endif
/* _BSD_SOURCE or _SVID_SOURCE for struct ether_addr, struct rpcent on Linux */
#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif

#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <shadow.h>
#include <aliases.h>
#include <netinet/ether.h>
#include <netdb.h>

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

#undef db_set_get_end_ent
