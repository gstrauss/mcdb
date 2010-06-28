#include "nss_mcdb_make.h"
#include "nss_mcdb_acct.h"
#include "nss_mcdb_acct_make.h"
#include "nss_mcdb_misc.h"
#include "nss_mcdb_misc_make.h"
#include "nss_mcdb_netdb.h"
#include "nss_mcdb_netdb_make.h"
#include "nointr.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>  /* malloc() free() */
#include <unistd.h>  /* sysconf() */

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
      char * restrict file;
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
        { "/etc/passwd",
          NSS_PW_HDRSZ+(size_t)sc_getpw_r_size_max,
          nss_mcdb_acct_make_passwd_parse,
          nss_mcdb_acct_make_passwd_encode,
          NULL },
        { "/etc/group",
          NSS_GR_HDRSZ+(size_t)sc_getgr_r_size_max,
          nss_mcdb_acct_make_group_parse,
          nss_mcdb_acct_make_group_encode,
          nss_mcdb_acct_make_group_flush },
        { "/etc/shadow",
          NSS_SP_HDRSZ+(size_t)sc_getpw_r_size_max,
          nss_mcdb_acct_make_shadow_parse,
          nss_mcdb_acct_make_spwd_encode,
          NULL },
      #if 0  /* implemented, but not enabling by default; little benefit */
        { "/etc/ethers",
          NSS_EA_HDRSZ+(size_t)sc_host_name_max,
          nss_mcdb_misc_make_ethers_parse,
          nss_mcdb_misc_make_ether_addr_encode,
          NULL },
        { "/etc/aliases",
          NSS_AE_HDRSZ+1024,
          nss_mcdb_misc_make_aliases_parse,
          nss_mcdb_misc_make_aliasent_encode,
          NULL },
      #endif
        { "/etc/hosts",
          NSS_HE_HDRSZ+1024,
          nss_mcdb_netdb_make_hosts_parse,
          nss_mcdb_netdb_make_hostent_encode,
          NULL },
        { "/etc/networks",
          NSS_NE_HDRSZ+1024,
          nss_mcdb_netdb_make_networks_parse,
          nss_mcdb_netdb_make_netent_encode,
          NULL },
        { "/etc/protocols",
          NSS_PE_HDRSZ+1024,
          nss_mcdb_netdb_make_protocols_parse,
          nss_mcdb_netdb_make_protoent_encode,
          NULL },
        { "/etc/rpc",
          NSS_RE_HDRSZ+1024,
          nss_mcdb_netdb_make_rpc_parse,
          nss_mcdb_netdb_make_rpcent_encode,
          NULL },
        { "/etc/services",
          NSS_SE_HDRSZ+1024,
          nss_mcdb_netdb_make_services_parse,
          nss_mcdb_netdb_make_servent_encode,
          NULL }
    };

    bool rc;

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

    /* <<<TODO: when wbuf->fd is changed to point to individual files, be sure
     *          to create data files for secure databases (e.g. shadow) with
     *          restrictive permissions.  There should never be a race when the
     *          file could be opened by less privileged users.
     *          Use mkstemp and see mcdb_makefmt.c:mcdb_makefmt_fdintofile()
     */

    /* parse databases */
    for (int i = 0; i < sizeof(fdb)/sizeof(struct fdb_st); ++i) {
        w.datasz = fdb[i].datasz;
        w.encode = fdb[i].encode;
        w.flush  = fdb[i].flush;
        assert(w.datasz <= DBUFSZ);

        /* generate mcdb make info */
        rc = nss_mcdb_make_dbfile_parse(&w, fdb[i].file, fdb[i].parse);
        if (!rc              /* (expect shadow passwd db to fail unless root) */
            && (fdb[i].parse != nss_mcdb_acct_make_shadow_parse || 0==getuid()))
            break;

        rc = nss_mcdb_make_dbfile_flush(&w);
        if (!rc)
            break;

        /* visual separator of 3 blank lines for testing (no other purpose) */
        /* (not catching nointr_write() return value in rc) */
        if (nointr_write(wbuf->fd, "\n\n\n", 3) == -1)
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
