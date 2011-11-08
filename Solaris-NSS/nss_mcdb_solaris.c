/*
 * nss_mcdb_solaris - Solaris NSS nsswitch database implementation using mcdb
 *
 * Copyright (c) 2011, Glue Logic LLC. All rights reserved. code()gluelogic.com
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

#include "../nss_mcdb.h"
#include "../nss_mcdb_acct.h"
#include "../nss_mcdb_netdb.h"

/*
 * Sun admits and makes no apologies for poorly documenting the NSS API.
 * Sun points to statements made over the years that the interface is private.
 * Sun states that access to old-style NSS API is handled by client side and
 * consequently the results are not cached in nscd (or that is the intention).
 *
 * Therefore, the NSSv2 API would be preferred to the old-style NSS API,
 * but only if it were sufficiently documented.
 *
 * http://hub.opensolaris.org/bin/view/Project+duckwater/finder 
 *
The finder APIs in NSS are rudimentary APIs that perform simple dlopen/dlsym lookups for function APIs called constructors.

These NSS (pre Sparks) constructor functions, when called, return a project private pointer to a fixed size structure of function pointers for a given set of NSS interfaces. These pointers are then used in undocumented project private behaviors to process backend lookups. Prior to PSARC/2005/133 usage was inconsistent and undocumented. the finder APIs were also undocumented.

The constructor names, function APIs and values are all considered project private APIs.
 */

/* Note: not implemented: ethers, projects, and other Solaris-specific dbs */
/* Note: not implemented: getipnodebyname() getipnodebyaddr() (both deprecated)
 *       Instead, use getaddrinfo() and getnameinfo() */

/* Wrap calls to nss_mcdb routines and translate the results to Solaris NSS API.
 * As you can see below from the nearly direct mapping from Solaris NSS API to
 * the (defined) Linux NSS API (based on what was known of Solaris NSS API),
 * this would be trivial to implement properly were "properly" defined by Sun.
 * Sun states:"Prior to PSARC/2005/133 usage was inconsistent and undocumented."
 * The inconsistent part is concerning, but we are talking about 20 databases
 * and so it curious why the behaviors could not be documented as-is.
 *
 * (That said, NSSv2 API is more extensible and should be preferred.
 * Now, if only writing custom modules for NSSv2 were better documented...)
 *
 * Solaris NSSv2 API
 * See <nss_common.h> and <nss_dbdefs.h> for minimal documentation.
 * http://hub.opensolaris.org/bin/view/Project+duckwater/finder
 * http://hub.opensolaris.org/bin/view/Project+sparks/architecture
 *   Sparks Architecture Documents (pdf)
 *     SPARKS (PSARC/2005/133) {Phase 1} Functional Specification
 */

static inline nss_status_t
_nss_mcdb_solaris_wrapper(const nss_status_t rv, nss_XbyY_args_t * const args)
{
    /* nss_mcdb_solaris implementation note:
     * args->erange was overloaded as errnop passed to nss_mcdb, so reset it */
    if (rv == NSS_STATUS_SUCCESS) {
        args->returnval = args->buf.result;
        args->erange = 0;
    }
    else {
        args->returnval = NULL;
        errno = args->erange;
        /* store boolean value in args->erange to indicate buffer too small */
        if ((args->erange = (args->erange==ERANGE && rv==NSS_STATUS_TRYAGAIN)))
            return NSS_STATUS_UNAVAIL;  /* avoid Solaris infinite loop */
            /* alternatively, set retry limit in nsswitch.conf
             *   e.g. passwd: mcdb [TRYAGAIN=3] */
    }
    return rv;
}



static nss_status_t
_nss_mcdb_generic_destr(nss_backend_t * const be, nss_XbyY_args_t * const args)
{
    return NSS_STATUS_SUCCESS;
}



static nss_status_t
_nss_mcdb_setpwent_be(nss_backend_t * const be, nss_XbyY_args_t * const args)
{
    return _nss_mcdb_setpwent(args->stayopen);
}

static nss_status_t
_nss_mcdb_endpwent_be(nss_backend_t * const be, nss_XbyY_args_t * const args)
{
    return _nss_mcdb_endpwent();
}

static nss_status_t
_nss_mcdb_getpwent_be(nss_backend_t * const be, nss_XbyY_args_t * const args)
{
    return _nss_mcdb_solaris_wrapper(
      _nss_mcdb_getpwent_r(args->buf.result, args->buf.buffer,
                           args->buf.buflen, &args->erange),
      args);
}

static nss_status_t
_nss_mcdb_getpwnam_be(nss_backend_t * const be, nss_XbyY_args_t * const args)
{
    return _nss_mcdb_solaris_wrapper(
      _nss_mcdb_getpwnam_r(args->key.name,
                           args->buf.result, args->buf.buffer,
                           args->buf.buflen, &args->erange),
      args);
}

static nss_status_t
_nss_mcdb_getpwuid_be(nss_backend_t * const be, nss_XbyY_args_t * const args)
{
    return _nss_mcdb_solaris_wrapper(
      _nss_mcdb_getpwuid_r(args->key.uid,
                           args->buf.result, args->buf.buffer,
                           args->buf.buflen, &args->erange),
      args);
}

static nss_backend_op_t nss_mcdb_acct_passwd_be_ops[] =
{
  (nss_backend_op_t)_nss_mcdb_generic_destr,
  (nss_backend_op_t)_nss_mcdb_endpwent_be,
  (nss_backend_op_t)_nss_mcdb_setpwent_be,
  (nss_backend_op_t)_nss_mcdb_getpwent_be,
  (nss_backend_op_t)_nss_mcdb_getpwnam_be,
  (nss_backend_op_t)_nss_mcdb_getpwuid_be
};

static nss_backend_t nss_mcdb_acct_passwd_be =
{
  .ops = nss_mcdb_acct_passwd_be_ops;
  .n_ops = sizeof(nss_mcdb_acct_passwd_be_ops) / sizeof(nss_backend_op_t);
};

nss_status_t
_nss_mcdb_passwd_constr(const char * const db_name,
                        const char * const src_name,
                        const char * const cfg_args)
{
    return &nss_mcdb_acct_passwd_be;
}



static nss_status_t
_nss_mcdb_setgrent_be(nss_backend_t * const be, nss_XbyY_args_t * const args)
{
    return _nss_mcdb_setgrent(args->stayopen);
}

static nss_status_t
_nss_mcdb_endgrent_be(nss_backend_t * const be, nss_XbyY_args_t * const args)
{
    return _nss_mcdb_endgrent();
}

static nss_status_t
_nss_mcdb_getgrent_be(nss_backend_t * const be, nss_XbyY_args_t * const args)
{
    return _nss_mcdb_solaris_wrapper(
      _nss_mcdb_getgrent_r(args->buf.result, args->buf.buffer,
                           args->buf.buflen, &args->erange),
      args);
}

static nss_status_t
_nss_mcdb_getgrnam_be(nss_backend_t * const be, nss_XbyY_args_t * const args)
{
    return _nss_mcdb_solaris_wrapper(
      _nss_mcdb_getgrnam_r(args->key.name,
                           args->buf.result, args->buf.buffer,
                           args->buf.buflen, &args->erange),
      args);
}

static nss_status_t
_nss_mcdb_getgrgid_be(nss_backend_t * const be, nss_XbyY_args_t * const args)
{
    return _nss_mcdb_solaris_wrapper(
      _nss_mcdb_getgrgid_r(args->key.gid,
                           args->buf.result, args->buf.buffer,
                           args->buf.buflen, &args->erange),
      args);
}

static nss_status_t
_nss_mcdb_getgroupsbymem_be(nss_backend_t * const be,
                            struct nss_groupsbymem * const args)
{
    int errnum;
    const nss_status_t rv =
      _nss_mcdb_initgroups_dyn(args->username, args->gid_array[0],
                               &args->numgids, &args->maxgids, &args->gid_array,
                               args->maxgids, &errnum);
    return (rv == NSS_STATUS_SUCCESS)
      ? (args->numgids != args->maxgids)
          ? NSS_STATUS_NOTFOUND  /*search for addtl groups (see nss_dbdefs.h)*/
          : NSS_STATUS_SUCCESS
      : ((errno = errnum), rv);
}

static nss_backend_op_t nss_mcdb_acct_group_be_ops[] =
{
  (nss_backend_op_t)_nss_mcdb_generic_destr,
  (nss_backend_op_t)_nss_mcdb_endgrent_be,
  (nss_backend_op_t)_nss_mcdb_setgrent_be,
  (nss_backend_op_t)_nss_mcdb_getgrent_be,
  (nss_backend_op_t)_nss_mcdb_getgrnam_be,
  (nss_backend_op_t)_nss_mcdb_getgrgid_be,
  (nss_backend_op_t)_nss_mcdb_getgroupsbymem_be
};

static nss_backend_t nss_mcdb_acct_group_be =
{
  .ops = nss_mcdb_acct_group_be_ops;
  .n_ops = sizeof(nss_mcdb_acct_group_be_ops) / sizeof(nss_backend_op_t);
};

nss_status_t
_nss_mcdb_group_constr(const char * const db_name,
                       const char * const src_name,
                       const char * const cfg_args)
{
    return &nss_mcdb_acct_group_be;
}



static nss_status_t
_nss_mcdb_sethostent_be(nss_backend_t * const be, nss_XbyY_args_t * const args)
{
    return _nss_mcdb_sethostent(args->stayopen);
}

static nss_status_t
_nss_mcdb_endhostent_be(nss_backend_t * const be, nss_XbyY_args_t * const args)
{
    return _nss_mcdb_endhostent();
}

static nss_status_t
_nss_mcdb_gethostent_be(nss_backend_t * const be, nss_XbyY_args_t * const args)
{
    return _nss_mcdb_solaris_wrapper(
      _nss_mcdb_gethostent_r(args->buf.result,
                             args->buf.buffer, args->buf.buflen,
                             &args->erange, &args->h_errno),
      args);
}

static nss_status_t
_nss_mcdb_gethostbyname_be(nss_backend_t * const be,
                           nss_XbyY_args_t * const args)
{
    return _nss_mcdb_solaris_wrapper(
      _nss_mcdb_gethostbyname2_r(args->key.name, AF_INET, args->buf.result,
                                 args->buf.buffer, args->buf.buflen,
                                 &args->erange, &args->h_errno),
      args);
}

static nss_status_t
_nss_mcdb_gethostbyaddr_be(nss_backend_t * const be,
                           nss_XbyY_args_t * const args)
{
    return _nss_mcdb_solaris_wrapper(
      _nss_mcdb_gethostbyaddr_r(args->key.hostaddr.addr,
                                args->key.hostaddr.len,
                                args->key.hostaddr.type,
                                args->buf.result,
                                args->buf.buffer, args->buf.buflen,
                                &args->erange, &args->h_errno),
      args);
}

static nss_backend_op_t nss_mcdb_netdb_hosts_be_ops[] =
{
  (nss_backend_op_t)_nss_mcdb_generic_destr,
  (nss_backend_op_t)_nss_mcdb_endhostent_be,
  (nss_backend_op_t)_nss_mcdb_sethostent_be,
  (nss_backend_op_t)_nss_mcdb_gethostent_be,
  (nss_backend_op_t)_nss_mcdb_gethostbyname_be,
  (nss_backend_op_t)_nss_mcdb_gethostbyaddr_be
};

static nss_backend_t nss_mcdb_netdb_hosts_be =
{
  .ops = nss_mcdb_netdb_hosts_be_ops;
  .n_ops = sizeof(nss_mcdb_netdb_hosts_be_ops) / sizeof(nss_backend_op_t);
};

nss_status_t
_nss_mcdb_hosts_constr(const char * const db_name,
                       const char * const src_name,
                       const char * const cfg_args)
{
    return &nss_mcdb_netdb_hosts_be;
}



static nss_status_t
_nss_mcdb_setnetent_be(nss_backend_t * const be, nss_XbyY_args_t * const args)
{
    return _nss_mcdb_setnetent(args->stayopen);
}

static nss_status_t
_nss_mcdb_endnetent_be(nss_backend_t * const be, nss_XbyY_args_t * const args)
{
    return _nss_mcdb_endnetent();
}

static nss_status_t
_nss_mcdb_getnetent_be(nss_backend_t * const be, nss_XbyY_args_t * const args)
{
    return _nss_mcdb_solaris_wrapper(
      _nss_mcdb_getnetent_r(args->buf.result,
                            args->buf.buffer, args->buf.buflen,
                            &args->erange, &args->h_errno),
      args);
}

static nss_status_t
_nss_mcdb_getnetbyname_be(nss_backend_t * const be,
                          nss_XbyY_args_t * const args)
{
    return _nss_mcdb_solaris_wrapper(
      _nss_mcdb_getnetbyname_r(args->key.name, args->buf.result,
                               args->buf.buffer, args->buf.buflen,
                               &args->erange, &args->h_errno),
      args);
}

static nss_status_t
_nss_mcdb_getnetbyaddr_be(nss_backend_t * const be,
                          nss_XbyY_args_t * const args)
{
    return _nss_mcdb_solaris_wrapper(
      _nss_mcdb_getnetbyaddr_r((uint32_t)args->key.netaddr.net,
                               args->key.netaddr.type,
                               args->buf.result,
                               args->buf.buffer, args->buf.buflen,
                               &args->erange, &args->h_errno),
      args);
}

static nss_backend_op_t nss_mcdb_netdb_networks_be_ops[] =
{
  (nss_backend_op_t)_nss_mcdb_generic_destr,
  (nss_backend_op_t)_nss_mcdb_endnetent_be,
  (nss_backend_op_t)_nss_mcdb_setnetent_be,
  (nss_backend_op_t)_nss_mcdb_getnetent_be,
  (nss_backend_op_t)_nss_mcdb_getnetbyname_be,
  (nss_backend_op_t)_nss_mcdb_getnetbyaddr_be
};

static nss_backend_t nss_mcdb_netdb_networks_be =
{
  .ops = nss_mcdb_netdb_networks_be_ops;
  .n_ops = sizeof(nss_mcdb_netdb_networks_be_ops) / sizeof(nss_backend_op_t);
};

nss_status_t
_nss_mcdb_networks_constr(const char * const db_name,
                          const char * const src_name,
                          const char * const cfg_args)
{
    return &nss_mcdb_netdb_networks_be;
}



static nss_status_t
_nss_mcdb_setprotoent_be(nss_backend_t * const be, nss_XbyY_args_t * const args)
{
    return _nss_mcdb_setprotoent(args->stayopen);
}

static nss_status_t
_nss_mcdb_endprotoent_be(nss_backend_t * const be, nss_XbyY_args_t * const args)
{
    return _nss_mcdb_endprotoent();
}

static nss_status_t
_nss_mcdb_getprotoent_be(nss_backend_t * const be, nss_XbyY_args_t * const args)
{
    return _nss_mcdb_solaris_wrapper(
      _nss_mcdb_getprotoent_r(args->buf.result, args->buf.buffer,
                              args->buf.buflen, &args->erange),
      args);
}

static nss_status_t
_nss_mcdb_getprotobyname_be(nss_backend_t * const be,
                            nss_XbyY_args_t * const args)
{
    return _nss_mcdb_solaris_wrapper(
      _nss_mcdb_getprotobyname_r(args->key.name,
                                 args->buf.result, args->buf.buffer,
                                 args->buf.buflen, &args->erange),
      args);
}

static nss_status_t
_nss_mcdb_getprotobynumber_be(nss_backend_t * const be,
                              nss_XbyY_args_t * const args)
{
    return _nss_mcdb_solaris_wrapper(
      _nss_mcdb_getprotobynumber_r(args->key.number,
                                   args->buf.result, args->buf.buffer,
                                   args->buf.buflen, &args->erange),
      args);
}

static nss_backend_op_t nss_mcdb_netdb_protocols_be_ops[] =
{
  (nss_backend_op_t)_nss_mcdb_generic_destr,
  (nss_backend_op_t)_nss_mcdb_endprotoent_be,
  (nss_backend_op_t)_nss_mcdb_setprotoent_be,
  (nss_backend_op_t)_nss_mcdb_getprotoent_be,
  (nss_backend_op_t)_nss_mcdb_getprotobyname_be,
  (nss_backend_op_t)_nss_mcdb_getprotobynumber_be
};

static nss_backend_t nss_mcdb_netdb_protocols_be =
{
  .ops = nss_mcdb_netdb_protocols_be_ops;
  .n_ops = sizeof(nss_mcdb_netdb_protocols_be_ops) / sizeof(nss_backend_op_t);
};

nss_status_t
_nss_mcdb_protocols_constr(const char * const db_name,
                           const char * const src_name,
                           const char * const cfg_args)
{
    return &nss_mcdb_netdb_protocols_be;
}



static nss_status_t
_nss_mcdb_setrpcent_be(nss_backend_t * const be, nss_XbyY_args_t * const args)
{
    return _nss_mcdb_setrpcent(args->stayopen);
}

static nss_status_t
_nss_mcdb_endrpcent_be(nss_backend_t * const be, nss_XbyY_args_t * const args)
{
    return _nss_mcdb_endrpcent();
}

static nss_status_t
_nss_mcdb_getrpcent_be(nss_backend_t * const be, nss_XbyY_args_t * const args)
{
    return _nss_mcdb_solaris_wrapper(
      _nss_mcdb_getrpcent_r(args->buf.result, args->buf.buffer,
                            args->buf.buflen, &args->erange),
      args);
}

static nss_status_t
_nss_mcdb_getrpcbyname_be(nss_backend_t * const be,
                          nss_XbyY_args_t * const args)
{
    return _nss_mcdb_solaris_wrapper(
      _nss_mcdb_getrpcbyname_r(args->key.name,
                               args->buf.result, args->buf.buffer,
                               args->buf.buflen, &args->erange),
      args);
}

static nss_status_t
_nss_mcdb_getrpcbynumber_be(nss_backend_t * const be,
                            nss_XbyY_args_t * const args)
{
    return _nss_mcdb_solaris_wrapper(
      _nss_mcdb_getrpcbynumber_r(args->key.number,
                                 args->buf.result, args->buf.buffer,
                                 args->buf.buflen, &args->erange),
      args);
}

static nss_backend_op_t nss_mcdb_netdb_rpc_be_ops[] =
{
  (nss_backend_op_t)_nss_mcdb_generic_destr,
  (nss_backend_op_t)_nss_mcdb_endrpcent_be,
  (nss_backend_op_t)_nss_mcdb_setrpcent_be,
  (nss_backend_op_t)_nss_mcdb_getrpcent_be,
  (nss_backend_op_t)_nss_mcdb_getrpcbyname_be,
  (nss_backend_op_t)_nss_mcdb_getrpcbynumber_be
};

static nss_backend_t nss_mcdb_netdb_rpc_be =
{
  .ops = nss_mcdb_netdb_rpc_be_ops;
  .n_ops = sizeof(nss_mcdb_netdb_rpc_be_ops) / sizeof(nss_backend_op_t);
};

nss_status_t
_nss_mcdb_rpc_constr(const char * const db_name,
                           const char * const src_name,
                           const char * const cfg_args)
{
    return &nss_mcdb_netdb_rpc_be;
}



static nss_status_t
_nss_mcdb_setservent_be(nss_backend_t * const be, nss_XbyY_args_t * const args)
{
    return _nss_mcdb_setservent(args->stayopen);
}

static nss_status_t
_nss_mcdb_endservent_be(nss_backend_t * const be, nss_XbyY_args_t * const args)
{
    return _nss_mcdb_endservent();
}

static nss_status_t
_nss_mcdb_getservent_be(nss_backend_t * const be, nss_XbyY_args_t * const args)
{
    return _nss_mcdb_solaris_wrapper(
      _nss_mcdb_getservent_r(args->buf.result, args->buf.buffer,
                             args->buf.buflen, &args->erange),
      args);
}

static nss_status_t
_nss_mcdb_getservbyname_be(nss_backend_t * const be,
                           nss_XbyY_args_t * const args)
{
    return _nss_mcdb_solaris_wrapper(
      _nss_mcdb_getservbyname_r(args->key.serv.serv.name, args->key.serv.proto,
                                args->buf.result, args->buf.buffer,
                                args->buf.buflen, &args->erange),
      args);
}

static nss_status_t
_nss_mcdb_getservbyport_be(nss_backend_t * const be,
                           nss_XbyY_args_t * const args)
{
    return _nss_mcdb_solaris_wrapper(
      _nss_mcdb_getservbyport_r(args->key.serv.serv.port, args->key.serv.proto,
                                args->buf.result, args->buf.buffer,
                                args->buf.buflen, &args->erange),
      args);
}

static nss_backend_op_t nss_mcdb_netdb_services_be_ops[] =
{
  (nss_backend_op_t)_nss_mcdb_generic_destr,
  (nss_backend_op_t)_nss_mcdb_endservent_be,
  (nss_backend_op_t)_nss_mcdb_setservent_be,
  (nss_backend_op_t)_nss_mcdb_getservent_be,
  (nss_backend_op_t)_nss_mcdb_getservbyname_be,
  (nss_backend_op_t)_nss_mcdb_getservbyport_be
};

static nss_backend_t nss_mcdb_netdb_services_be =
{
  .ops = nss_mcdb_netdb_services_be_ops;
  .n_ops = sizeof(nss_mcdb_netdb_services_be_ops) / sizeof(nss_backend_op_t);
};

nss_status_t
_nss_mcdb_services_constr(const char * const db_name,
                          const char * const src_name,
                          const char * const cfg_args)
{
    return &nss_mcdb_netdb_services_be;
}



/* Future: (potential performance enhancement)
 *
 * NSSv2 requires use of args->str2ent() in order that nscd can provide a
 * routine that copies "file native" string so that string can be
 *   a) cached
 *   b) passed through door to client
 *      (where client runs str2ent() to reconstitute entity)
 *
 * If there is a guarantee that (nss_XbyY_args_t *)args->str2ent() is defined,
 * then it migth be worth writing custom nss routines for Solaris which use
 *   nss_mcdb_solaris_generic_decode()
 * to parse mcdb data that was stored in "file native" format from flat files
 * (minus trailing and leading whitespace, and with comments removed).
 *
 * nss_mcdbctl would need to be modified to store data as above.
 * Note that custom decode routines would need to be written for hosts and
 * services databases since there is additional comparisons that are done to
 * find the correct entry in the database.  This could be achieved by putting
 * that extra piece of information at the beginning of the data string and
 * then, after matching, passing the remaining part of the string to
 * nss_mcdb_solaris_generic_decode().
 *
 * While fairly straightforward, the above would be some hours of work, and
 * the benefit might be dubious.  NSSv2 API documentation suggests that if
 * NSSv2 interface is not available, nscd returns TRYLOCAL and client performs
 * the nss query.  In the case of mcdb, the result is might be a fast mcdb
 * query to an efficient packed binary format that is quickly decoded to the
 * desired entity.  On the flip side, if there were a way to detect that
 * nss_mcdb were running inside nscd, then nss_mcdb could be written to hold
 * open mcdb dbs and be notified when files change (instead of stat()ing).
 */



#if 0

/*
 * Solaris NSSv2 API
 * http://hub.opensolaris.org/bin/view/Project+duckwater/finder
 * http://hub.opensolaris.org/bin/view/Project+sparks/architecture
 *   Sparks Architecture Documents (pdf)
 *     SPARKS (PSARC/2005/133) {Phase 1} Functional Specification
 *
 * Various snippets from Sparks Architecture Documents PDF:
 *
The data is an octet buffer (*not* a zero-terminated string) whose length is specified by the instr_len parameter. The data is found at the address specified by instr and the data is treated as readonly.

With the exception of passwd, shadow and group, the text form for these databases allows trailing comments and arbitrary whitespace. The corresponding str2ent routine assumes that comments, leading whitespace and trailing whitespace have been stripped (and thus assumes that entries consisting only of these have been discarded).

A backend is expected to process the request based on the args data and call a marshalling function provided by the args structure. In NSS not all backends are consistent.

In NSS2 the following clarification to this existing interface is that backends must use the provided data marshaller. Backends cannot interpose the provided data marshaller with their own internal copy.

7.2.4 Internal NSS2 Interfaces
Using nscd's new internal "policy engine finder" interfaces, the policy engine finder will search for new NSS2 packed interfaces by performing dlsym searches of the following format:
("_nss_%s_%s_%s_%s", operation, database_source_name, database_name, key ) where:
o operation is one of : "get", "put", 'setent", "getent", "endent"
o database_source_name: defined database sources such as "ldap" etc.
o database_name: defined databases such as "passwd" etc.
o key: supported key types such as "name", "uid", "gid", etc...
The new backend packed function format is defined as:
nss_status_t (*nss_backend_nop_t)(struct nss_backend *, void **, size_t *);
For existing getXbyY interfaces, if the new backend function exists, it will be used by the policy engine. Other wise, if the NSS backend operation exists it will be used.
 */

static nss_status_t
nss_mcdb_solaris_generic_decode(struct mcdb * const restrict m,
                                const struct nss_mcdb_vinfo * const restrict v)
{
    nss_XbyY_args_t * const restrict args = (nss_XbyY_args_t *)v->vstruct;
    const size_t dlen = mcdb_datalen(m); /*(mcdb limits data to INT_MAX-8)*/
    if ((int)dlen < args->buf.buflen) {  /*(therefore (int)dlen cast is ok)*/
        /* str2ent marshaller treats data as read-only; go straight from mcdb */
        const int rc = args->str2ent(mcdb_dataptr(m), (int)dlen,
                                     args->buf.result,
                                     args->buf.buffer, args->buf.buflen);
        if (rc == NSS_STR_PARSE_SUCCESS)
            return NSS_STATUS_SUCCESS;
        else if (rc != NSS_STR_PARSE_ERANGE) { /*(rc == NSS_STR_PARSE_PARSE)*/
            args->erange = EIO;
            return NSS_STATUS_UNAVAIL;
        }
        /*else rc == NSS_STR_PARSE_ERANGE; fall through*/
    }
    args->erange = ERANGE;
    return NSS_STATUS_TRYAGAIN;
}

static nss_status_t
nss_mcdb_solaris_groupsbymem_decode(struct mcdb * const restrict m,
                                    const struct nss_mcdb_vinfo *
                                      const restrict v)
{
    /* not implemented
     *
     * (need custom routine;
     *  frontent supplies args different from nss_XbyY_args_t *) */
}



nss_status_t
_nss_get_mcdb_passwd_name(struct nss_backend * const be,
                          void ** const vargs /*??*/, size_t * const sz /*??*/)
{                         /* ???what is vargs??? */    /* ???what is sz???*/
    nss_XbyY_args_t * const args = *(nss_XbyY_args_t **)vargs;  /* guessing */
    const struct nss_mcdb_vinfo v = {.decode  = nss_mcdb_solaris_generic_decode,
                                     .vstruct = args,
                                     .key     = args->key.name,
                                     .klen    = strlen(args->key.name),
                                     .tagc    = (unsigned char)'=' };
    return _nss_mcdb_solaris_wrapper(
      nss_mcdb_get_generic(NSS_DBTYPE_PASSWD, &v),
      args);
}

/*
 * ... not implemented ...
 *
 * lack of sufficient documentation suggests doing so is shooting in the dark
 *
 */

/* NSSv2 API operations table and global version symbol
 * NULL version symbol indicates minimal compliance with PSARC/2005/133 spec.
 * Defined structure indicates compliance with PSARC/2008/035 specification.
 * nss_v_api and nss_v_opt must be sorted if NSS_VERSION_SORTED is indicated. */

/* Note: must be sorted by API name if NSS_VERSION_SORTED set in nss_version_t*/
/* Note: not implemented: _nss_put_mcdb_*().  ?? Implement are return xxx ?? */
/* Note: below represents guesses at expected interface names; confirmation
 * would be nice but nm on Solaris 10u7 /lib/nss_*.so.1 did not yield results */
static nss_backend_api_t nss_mcdb_nss_v_api[] = {
  //NSS_APIENTRY_V2(_nss_endent_mcdb_group,         0, NULL, NULL),
  //NSS_APIENTRY_V2(_nss_endent_mcdb_hosts,         0, NULL, NULL),
  //NSS_APIENTRY_V2(_nss_endent_mcdb_networks,      0, NULL, NULL),
  //NSS_APIENTRY_V2(_nss_endent_mcdb_passwd,        0, NULL, NULL),
  //NSS_APIENTRY_V2(_nss_endent_mcdb_protocols,     0, NULL, NULL),
  //NSS_APIENTRY_V2(_nss_endent_mcdb_rpc,           0, NULL, NULL),
  //NSS_APIENTRY_V2(_nss_endent_mcdb_services,      0, NULL, NULL),
  ////NSS_APIENTRY_V2(_nss_get_mcdb_group_???groupmem, 0, NULL, NULL),
  //NSS_APIENTRY_V2(_nss_get_mcdb_group_gid,        0, NULL, NULL),
  //NSS_APIENTRY_V2(_nss_get_mcdb_group_name,       0, NULL, NULL),
  //NSS_APIENTRY_V2(_nss_get_mcdb_hosts_addr,       0, NULL, NULL),
  //NSS_APIENTRY_V2(_nss_get_mcdb_hosts_name,       0, NULL, NULL),
  //NSS_APIENTRY_V2(_nss_get_mcdb_networks_addr,    0, NULL, NULL),
  //NSS_APIENTRY_V2(_nss_get_mcdb_networks_name,    0, NULL, NULL),
  //NSS_APIENTRY_V2(_nss_get_mcdb_passwd_name,      0, NULL, NULL),
  //NSS_APIENTRY_V2(_nss_get_mcdb_passwd_uid,       0, NULL, NULL),
  //NSS_APIENTRY_V2(_nss_get_mcdb_protocols_name,   0, NULL, NULL),
  //NSS_APIENTRY_V2(_nss_get_mcdb_protocols_number, 0, NULL, NULL),
  //NSS_APIENTRY_V2(_nss_get_mcdb_rpc_name,         0, NULL, NULL),
  //NSS_APIENTRY_V2(_nss_get_mcdb_rpc_number,       0, NULL, NULL),
  //NSS_APIENTRY_V2(_nss_get_mcdb_services_name,    0, NULL, NULL),
  //NSS_APIENTRY_V2(_nss_get_mcdb_services_port,    0, NULL, NULL),
  //NSS_APIENTRY_V2(_nss_getent_mcdb_group,         0, NULL, NULL),
  //NSS_APIENTRY_V2(_nss_getent_mcdb_hosts,         0, NULL, NULL),
  //NSS_APIENTRY_V2(_nss_getent_mcdb_networks,      0, NULL, NULL),
  //NSS_APIENTRY_V2(_nss_getent_mcdb_passwd,        0, NULL, NULL),
  //NSS_APIENTRY_V2(_nss_getent_mcdb_protocols,     0, NULL, NULL),
  //NSS_APIENTRY_V2(_nss_getent_mcdb_rpc,           0, NULL, NULL),
  //NSS_APIENTRY_V2(_nss_getent_mcdb_services,      0, NULL, NULL),
  NSS_APIENTRY_V1(_nss_mcdb_group_constr),
  NSS_APIENTRY_V1(_nss_mcdb_hosts_constr),
  NSS_APIENTRY_V1(_nss_mcdb_networks_constr),
  NSS_APIENTRY_V1(_nss_mcdb_passwd_constr),
  NSS_APIENTRY_V1(_nss_mcdb_protocols_constr),
  NSS_APIENTRY_V1(_nss_mcdb_rpc_constr),
  NSS_APIENTRY_V1(_nss_mcdb_services_constr)
  //NSS_APIENTRY_V2(_nss_setent_mcdb_group,         0, NULL, NULL),
  //NSS_APIENTRY_V2(_nss_setent_mcdb_hosts,         0, NULL, NULL),
  //NSS_APIENTRY_V2(_nss_setent_mcdb_networks,      0, NULL, NULL),
  //NSS_APIENTRY_V2(_nss_setent_mcdb_passwd,        0, NULL, NULL),
  //NSS_APIENTRY_V2(_nss_setent_mcdb_protocols,     0, NULL, NULL),
  //NSS_APIENTRY_V2(_nss_setent_mcdb_rpc,           0, NULL, NULL),
  //NSS_APIENTRY_V2(_nss_setent_mcdb_services,      0, NULL, NULL)
};

nss_version_t _nss_mcdb_version = {
  .nss_v_version   = NSS_VERSION_2_1,
  .nss_v_sz        = sizeof(nss_version_t),
  .nss_v_api       = nss_mcdb_nss_v_api,
  .nss_v_api_cnt   = sizeof(nss_mcdb_nss_v_api)/sizeof(nss_backend_api_t),
  .nss_v_api_flags = NSS_VERSION_SORTED,
  .nss_v_opt       = NULL,
  .nss_v_opt_cnt   = 0,
  .nss_v_opt_flags = NSS_VERSION_SORTED
};

#endif
