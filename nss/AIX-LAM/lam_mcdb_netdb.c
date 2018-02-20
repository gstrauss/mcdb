/*
 * lam_mcdb_netdb - query mcdb of hosts, protocols, networks, services
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

/*
 *
 * AIX-specific code for AIX LAM
 *
 * (AIX Dynamic Load API)
 * http://publib.boulder.ibm.com/infocenter/aix/v6r1/index.jsp?topic=%2Fcom.ibm.aix.progcomm%2Fdoc%2Fprogcomc%2Ftcp_dlapi.htm
 *
 * (AIX LAM == AIX Loadable Authentication Module Programming Interface)
 * http://publib.boulder.ibm.com/infocenter/aix/v6r1/index.jsp?topic=%2Fcom.ibm.aix.kernelext%2Fdoc%2Fkernextc%2Fsec_load_mod.htm
 *
 */

/* gethostby* getnetby* getprotoby* getservby*
 * are thread safe in AIX(R) 4.3 and later */

/* _XOPEN_SOURCE=600 on AIX might be sufficient to get netdb definitions
 * (need _ALL_SOURCE for _MAXLINELEN and _HOSTBUFSIZE) */
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#include "../nss_mcdb_netdb.h"

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>

/* AIX provides struct hostent_data, netent_data, protoent_data, servent_data
 * but nss_mcdb_netdb code uses only non-standard _MAXLINELEN and _HOSTBUFSIZE
 * from netdb.h since the rest of the nss_mcdb code aims to be portable */
#ifndef _MAXLINELEN        /* AIX netdb.h: 1024 */
#define _MAXLINELEN 1024
#endif
#ifndef _HOSTBUFSIZE       /* AIX netdb.h: (BUFSIZ+1) */
#define _HOSTBUFSIZE 4097
#endif

struct ho_pvt {
  struct hostent he;
  int errnum;
  int h_errnum;
  int getent;
  char buf[NSS_HE_HDRSZ+_HOSTBUFSIZE];
};

void *
ho_pvtinit (void)
{
    struct ho_pvt * const restrict ho = malloc(sizeof(struct ho_pvt));
    if (ho)
        ho->getent = false;
    return ho;
}

void
ho_rewind (struct ho_pvt * const restrict ho)
{
    if (ho) {
        ho->getent = true;
        _nss_mcdb_sethostent(0);
    }
}

void
ho_minimize (struct ho_pvt * const restrict ho)
{
    if (ho && ho->getent)
        _nss_mcdb_endhostent();
}

void
ho_close (struct ho_pvt * const restrict ho)
{
    if (ho) {
        ho_minimize(ho);
        free(ho);
    }
}

struct hostent *
ho_next (struct ho_pvt * const restrict ho)
{
    if (!ho) { errno = EINVAL; return NULL; }
    ho->getent = true;
    return NSS_STATUS_SUCCESS
        == _nss_mcdb_gethostent_r(&ho->he, ho->buf, sizeof(ho->buf),
                                  &ho->errnum, &ho->h_errnum)
      ? &ho->he
      : (h_errno = ho->h_errnum, (struct hostent *)NULL);
}

struct hostent *
ho_byname2 (struct ho_pvt * const restrict ho,
            const char * const restrict name, const int type)
{
    if (!ho) { errno = EINVAL; return NULL; }
    return NSS_STATUS_SUCCESS
        == _nss_mcdb_gethostbyname2_r(name, type, &ho->he,
                                      ho->buf, sizeof(ho->buf),
                                      &ho->errnum, &ho->h_errnum)
      ? &ho->he
      : (h_errno = ho->h_errnum, (struct hostent *)NULL);
}

struct hostent *
ho_byname (struct ho_pvt * const restrict ho, const char * const restrict name)
{
    return ho_byname2(ho, name, AF_INET);
}

struct hostent *
ho_byaddr (struct ho_pvt * const restrict ho, const void * const restrict addr,
           const size_t len, const int type)
{
    if (!ho) { errno = EINVAL; return NULL; }
    return NSS_STATUS_SUCCESS
        == _nss_mcdb_gethostbyaddr_r(addr, (socklen_t)len, type, &ho->he,
                                     ho->buf, sizeof(ho->buf),
                                     &ho->errnum, &ho->h_errnum)
      ? &ho->he
      : (h_errno = ho->h_errnum, (struct hostent *)NULL);
}



struct nw_pvt {
  struct nwent ne;
  int errnum;
  int h_errnum;
  int getent;
  uint32_t n_net;
  char buf[NSS_NE_HDRSZ+_MAXLINELEN];
};

void *
nw_pvtinit (void)
{
    struct nw_pvt * const restrict nw = malloc(sizeof(struct nw_pvt));
    if (nw) {
        nw->getent = false;
        nw->ne.n_addr = &nw->n_net;  /* supports only AF_INET */
    }
    return nw;
}

void
nw_rewind (struct nw_pvt * const restrict nw)
{
    if (nw) {
        nw->getent = true;
        _nss_mcdb_setnetent(0);
    }
}

void
nw_minimize (struct nw_pvt * const restrict nw)
{
    if (nw && nw->getent)
        _nss_mcdb_endnetent();
}

void
nw_close (struct nw_pvt * const restrict nw)
{
    if (nw) {
        nw_minimize(nw);
        free(nw);
    }
}

struct nwent *
nw_next (struct nw_pvt * const restrict nw)
{
    if (!nw) { errno = EINVAL; return NULL; }
    nw->getent = true;
    return NSS_STATUS_SUCCESS
        == _nss_mcdb_getnetent_r((struct netent *)&nw->ne, nw->buf,
                                 sizeof(nw->buf), &nw->errnum, &nw->h_errnum)
      ? &nw->ne
      : (h_errno = nw->h_errnum, (struct nwent *)NULL);
}

struct nwent *
nw_byname (struct nw_pvt * const restrict nw,
           const char * const restrict name, const int type)/*(always AF_INET)*/
{
    if (!nw) { errno = EINVAL; return NULL; }
    return NSS_STATUS_SUCCESS
        == _nss_mcdb_getnetbyname_r(name, (struct netent *)&nw->ne, nw->buf,
                                    sizeof(nw->buf), &nw->errnum, &nw->h_errnum)
      ? &nw->ne
      : (h_errno = nw->h_errnum, (struct nwent *)NULL);
}

struct nwent *
nw_byaddr (struct nw_pvt * const restrict nw, const void * const restrict net,
           const int length, const int type)
{
    if (!nw) { errno = EINVAL; return NULL; }
    return NSS_STATUS_SUCCESS
        == _nss_mcdb_getnetbyaddr_r((uint32_t)net, type,
                                    (struct netent *)&nw->ne, nw->buf,
                                    sizeof(nw->buf), &nw->errnum, &nw->h_errnum)
      ? &nw->ne
      : (h_errno = nw->h_errnum, (struct nwent *)NULL);
}



#if 0  /* implemented, but not enabling by default; often used only with NIS+ */

struct ng_pvt {
  int errnum;
  int getent;
  char buf[_MAXLINELEN*3];
};

void *
ng_pvtinit (void)
{
    struct ng_pvt * const restrict ng = malloc(sizeof(struct ng_pvt));
    if (ng)
        ng->getent = false;
    return ng;
}

void
ng_rewind (struct ng_pvt * const restrict ng,
           const char * const restrict netgroup)
{
    if (ng) {
        ng->getent = true;
        _nss_mcdb_setnetgrent(netgroup);
    }
}

void
ng_minimize (struct ng_pvt * const restrict ng)
{
    if (ng && ng->getent)
        _nss_mcdb_endnetgrent();
}

void
ng_close (struct ng_pvt * const restrict ng)
{
    if (ng) {
        ng_minimize(ng);
        free(ng);
    }
}

int
ng_next (struct ng_pvt * const restrict ng,
         const char ** const restrict host,
         const char ** const restrict user,
         const char ** const restrict domain)
{
    if (!ng) { errno = EINVAL; return false; }
    ng->getent = true;
    return NSS_STATUS_SUCCESS
        == _nss_mcdb_getnetgrent_r(host, user, domain,
                                   ng->buf, sizeof(ng->buf), &ng->errnum);
}

int
ng_test (struct ng_pvt * const restrict ng,
         const char * const restrict netgroup,
         const char * restrict host,
         const char * restrict user,
         const char * restrict domain)
{   /* similar to innetgr() */
    if (!ng) { errno = EINVAL; return false; }
    return _nss_mcdb_innetgr(netgroup, host, user, domain,
                             ng->buf, sizeof(ng->buf), &ng->errnum);
}

#endif /* implemented, but not enabling by default; often used only with NIS+ */



struct pr_pvt {
  struct protoent pe;
  int errnum;
  int getent;
  char buf[NSS_PE_HDRSZ+_MAXLINELEN];
};

void *
pr_pvtinit (void)
{
    struct pr_pvt * const restrict pr = malloc(sizeof(struct pr_pvt));
    if (pr)
        pr->getent = false;
    return pr;
}

void
pr_rewind (struct pr_pvt * const restrict pr)
{
    if (pr) {
        pr->getent = true;
        _nss_mcdb_setprotoent(0);
    }
}

void
pr_minimize (struct pr_pvt * const restrict pr)
{
    if (pr && pr->getent)
        _nss_mcdb_endprotoent();
}

void
pr_close (struct pr_pvt * const restrict pr)
{
    if (pr) {
        pr_minimize(pr);
        free(pr);
    }
}

struct protoent *
pr_next (struct pr_pvt * const restrict pr)
{
    if (!pr) { errno = EINVAL; return NULL; }
    pr->getent = true;
    return NSS_STATUS_SUCCESS
        == _nss_mcdb_getprotoent_r(&pr->pe,pr->buf,sizeof(pr->buf),&pr->errnum)
      ? &pr->pe
      : NULL;
}

struct protoent *
pr_byname (struct pr_pvt * const restrict pr, const char * const restrict name)
{
    if (!pr) { errno = EINVAL; return NULL; }
    return NSS_STATUS_SUCCESS
        == _nss_mcdb_getprotobyname_r(name, &pr->pe,
                                      pr->buf, sizeof(pr->buf), &pr->errnum)
      ? &pr->pe
      : NULL;
}

struct protoent *
pr_bynumber (struct pr_pvt * const restrict pr, const int proto)
{
    if (!pr) { errno = EINVAL; return NULL; }
    return NSS_STATUS_SUCCESS
        == _nss_mcdb_getprotobynumber_r(proto, &pr->pe,
                                        pr->buf, sizeof(pr->buf), &pr->errnum)
      ? &pr->pe
      : NULL;
}



struct sv_pvt {
  struct servent se;
  int errnum;
  int getent;
  char buf[NSS_SE_HDRSZ+_MAXLINELEN];
};

void *
sv_pvtinit (void)
{
    struct sv_pvt * const restrict sv = malloc(sizeof(struct sv_pvt));
    if (sv)
        sv->getent = false;
    return sv;
}

void
sv_rewind (struct sv_pvt * const restrict sv)
{
    if (sv) {
        sv->getent = true;
        _nss_mcdb_setservent(0);
    }
}

void
sv_minimize (struct sv_pvt * const restrict sv)
{
    if (sv && sv->getent)
        _nss_mcdb_endservent();
}

void
sv_close (struct sv_pvt * const restrict sv)
{
    if (sv) {
        sv_minimize(sv);
        free(sv);
    }
}

struct servent *
sv_next (struct sv_pvt * const restrict sv)
{
    if (!sv) { errno = EINVAL; return NULL; }
    sv->getent = true;
    return NSS_STATUS_SUCCESS
        == _nss_mcdb_getservent_r(&sv->se,sv->buf,sizeof(sv->buf),&sv->errnum)
      ? &sv->se
      : NULL;
}

struct servent *
sv_byname (struct sv_pvt * const restrict sv,
           const char * const restrict name, const char * const restrict proto)
{
    if (!sv) { errno = EINVAL; return NULL; }
    return NSS_STATUS_SUCCESS
        == _nss_mcdb_getservbyname_r(name, proto, &sv->se,
                                     sv->buf, sizeof(sv->buf), &sv->errnum)
      ? &sv->se
      : NULL;
}

struct servent *
sv_byport (struct sv_pvt * const restrict sv,
           const int port, const char * const restrict proto)
{
    if (!sv) { errno = EINVAL; return NULL; }
    return NSS_STATUS_SUCCESS
        == _nss_mcdb_getservbyport_r(port, proto, &sv->se,
                                     sv->buf, sizeof(sv->buf), &sv->errnum)
      ? &sv->se
      : NULL;
}
