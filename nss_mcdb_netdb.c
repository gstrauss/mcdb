/* _BSD_SOURCE or _SVID_SOURCE for struct rpcent on Linux */
#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif

#include "nss_mcdb_netdb.h"
#include "nss_mcdb.h"
#include "mcdb.h"
#include "uint32.h"
#include "code_attributes.h"

#include <errno.h>
#include <string.h>

#include <netdb.h>
#include <sys/socket.h>    /* AF_INET */
#include <arpa/inet.h>     /* inet_pton() */

/*
 * man hosts         (Internet RFC 952)
 *     hostname(1) hostname(7) resolver(3) resolver(5) host.conf resolv.conf
 *     gethostbyname gethostbyaddr sethostent gethostent endhostent
 *     gethostname getdomainname
 *     /etc/hosts
 *
 * man protocols(5)  (POSIX.1-2001)
 *     getprotobyname getprotobynumber setprotoent getprotoent endprotoent
 *     /etc/protocols
 * man networks      (POSIX.1-2001)
 *     getnetbyname getnetbyaddr setnetent getnetent endnetent
 *     /etc/networks
 * man services(5)   (POSIX.1-2001)
 *     getservbyname getservbyport setservent getservent endservent
 *     /etc/services
 *
 * man rpc(5)        (not standard)
 *     getrpcbyname getrpcbynumber setrpcent getrpcent endrpcent rpcinfo rpcbind
 *     /etc/rpc
 *
 * man netgroup      (not standard)
 *     setnetgrent getnetgrent endnetgrent innetgr
 *     /etc/netgroup
 */

static nss_status_t
_nss_mcdb_decode_hostent(struct mcdb * restrict,
                         const struct _nss_kinfo * restrict,
                         const struct _nss_vinfo * restrict)
  __attribute_nonnull_x__((1,3))  __attribute_warn_unused_result__;

static nss_status_t
_nss_mcdb_decode_netent(struct mcdb * restrict,
                        const struct _nss_kinfo * restrict
                          __attribute_unused__,
                        const struct _nss_vinfo * restrict)
  __attribute_nonnull_x__((1,3))  __attribute_warn_unused_result__;

static nss_status_t
_nss_mcdb_decode_protoent(struct mcdb * restrict,
                          const struct _nss_kinfo * restrict
                            __attribute_unused__,
                          const struct _nss_vinfo * restrict)
  __attribute_nonnull_x__((1,3))  __attribute_warn_unused_result__;

static nss_status_t
_nss_mcdb_decode_rpcent(struct mcdb * restrict,
                        const struct _nss_kinfo * restrict
                          __attribute_unused__,
                        const struct _nss_vinfo * restrict)
  __attribute_nonnull_x__((1,3))  __attribute_warn_unused_result__;

static nss_status_t
_nss_mcdb_decode_servent(struct mcdb * restrict,
                         const struct _nss_kinfo * restrict,
                         const struct _nss_vinfo * restrict)
  __attribute_nonnull_x__((1,3))  __attribute_warn_unused_result__;


static nss_status_t  __attribute_noinline__
gethost_fill_h_errnop(const nss_status_t status, int * const restrict h_errnop)
  __attribute_cold__  __attribute_nonnull__;

static nss_status_t
gethost_query(const uint32_t type,
              const struct _nss_kinfo * restrict kinfo,
              const struct _nss_vinfo * restrict vinfo,
              int * const restrict h_errnop)
  __attribute_nonnull__;

static nss_status_t
gethost_filladdr(const void * const restrict addr, const int type,
                 const struct _nss_kinfo * restrict kinfo,
                 const struct _nss_vinfo * restrict vinfo,
                 int * const restrict h_errnop)
  __attribute_nonnull__;


void _nss_mcdb_sethostent(void)  { _nss_mcdb_setent(NSS_DBTYPE_HOSTS);     }
void _nss_mcdb_endhostent(void)  { _nss_mcdb_endent(NSS_DBTYPE_HOSTS);     }
void _nss_mcdb_setnetgrent(void) { _nss_mcdb_setent(NSS_DBTYPE_NETGROUP);  }
void _nss_mcdb_endnetgrent(void) { _nss_mcdb_endent(NSS_DBTYPE_NETGROUP);  }
void _nss_mcdb_setnetent(void)   { _nss_mcdb_setent(NSS_DBTYPE_NETWORKS);  }
void _nss_mcdb_endnetent(void)   { _nss_mcdb_endent(NSS_DBTYPE_NETWORKS);  }
void _nss_mcdb_setprotoent(void) { _nss_mcdb_setent(NSS_DBTYPE_PROTOCOLS); }
void _nss_mcdb_endprotoent(void) { _nss_mcdb_endent(NSS_DBTYPE_PROTOCOLS); }
void _nss_mcdb_setrpcent(void)   { _nss_mcdb_setent(NSS_DBTYPE_RPC);       }
void _nss_mcdb_endrpcent(void)   { _nss_mcdb_endent(NSS_DBTYPE_RPC);       }
void _nss_mcdb_setservent(void)  { _nss_mcdb_setent(NSS_DBTYPE_SERVICES);  }
void _nss_mcdb_endservent(void)  { _nss_mcdb_endent(NSS_DBTYPE_SERVICES);  }


/* POSIX.1-2001 marks gethostbyaddr() and gethostbyname() obsolescent.
 * See man getnameinfo(), getaddrinfo(), freeaddrinfo(), gai_strerror()
 * However, note that getaddrinfo() allocates memory for its results 
 * (linked list of struct addrinfo) that must be free()d with freeaddrinfo(). */

nss_status_t
_nss_mcdb_gethostent_r(struct hostent * const restrict hostbuf,
                       char * const restrict buf, const size_t buflen,
                       int * const restrict errnop,
                       int * const restrict h_errnop)
{
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_hostent,
                                      .vstruct = hostbuf,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .errnop  = errnop };
    nss_status_t status;
    if (buflen > 3) {
        buf[0] = buf[1] = buf[2] = buf[3] = '\0'; /* addr type AF_UNSPEC == 0 */
        status = _nss_mcdb_getent(NSS_DBTYPE_HOSTS, &vinfo);
    }
    else {
        *errnop = errno = ERANGE;
        status = NSS_STATUS_TRYAGAIN;
    }
    return (status == NSS_STATUS_SUCCESS)
      ? NSS_STATUS_SUCCESS
      : gethost_fill_h_errnop(status, h_errnop);
}

nss_status_t
_nss_mcdb_gethostbyname2_r(const char * const restrict name, const int type,
                           struct hostent * const restrict hostbuf,
                           char * const restrict buf, const size_t buflen,
                           int * const restrict errnop,
                           int * const restrict h_errnop)
{
    const struct _nss_kinfo kinfo = { .key     = name,
                                      .klen    = strlen(name),
                                      .tagc    = (unsigned char)'=' };
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_hostent,
                                      .vstruct = hostbuf,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .errnop  = errnop };
    uint64_t addr[2] = {0,0};/* support addr sizes up to 128-bits (e.g. IPv6) */
    const int is_addr = inet_pton(type, name, &addr);
    if (is_addr == 0) /* name is not valid address for specified addr family */
        return gethost_query((uint32_t)type, &kinfo, &vinfo, h_errnop);
    else if (is_addr > 0) /* name is valid address for specified addr family */
        return gethost_filladdr(&addr, type, &kinfo, &vinfo, h_errnop);
    else /* invalid address family EAFNOSUPPORT => NSS_STATUS_RETURN */
        return gethost_fill_h_errnop(NSS_STATUS_RETURN, h_errnop);
}

nss_status_t
_nss_mcdb_gethostbyname_r(const char * const restrict name,
                          struct hostent * const restrict hostbuf,
                          char * const restrict buf, const size_t buflen,
                          int * const restrict errnop,
                          int * const restrict h_errnop)
{
    return _nss_mcdb_gethostbyname2_r(name, AF_INET, hostbuf,
                                      buf, buflen, errnop, h_errnop);
}

nss_status_t
_nss_mcdb_gethostbyaddr_r(const void * const restrict addr,
                          const socklen_t len, const int type,
                          struct hostent * const restrict hostbuf,
                          char * const restrict buf, const size_t buflen,
                          int * const restrict errnop,
                          int * const restrict h_errnop)
{
    char hexstr[32];  /* support 128-bit IPv6 addresses */
    const struct _nss_kinfo kinfo = { .key     = hexstr,
                                      .klen    = len<<1,
                                      .tagc    = (unsigned char)'x' };
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_hostent,
                                      .vstruct = hostbuf,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .errnop  = errnop };
    uint32_t u[4];  /* align data to uint32_t for uphex conversion routines */
    switch (len) {
      case 4:  /* e.g. AF_INET4 */
               memcpy(&u, addr, 4);
               uint32_to_ascii8uphex(u[0], hexstr);
               break;
      case 16: /* e.g. AF_INET6 */
               memcpy(&u, addr, 16);
               uint32_to_ascii8uphex(u[0], hexstr);
               uint32_to_ascii8uphex(u[1], hexstr+8);
               uint32_to_ascii8uphex(u[2], hexstr+16);
               uint32_to_ascii8uphex(u[3], hexstr+24);
               break;
      default: *errnop = errno = ENOENT;
               return NSS_STATUS_UNAVAIL; /* other types not implemented */
    }

    return gethost_query((uint32_t)type, &kinfo, &vinfo, h_errnop);
}


nss_status_t
_nss_mcdb_getnetgrent_r(char ** const restrict host,
                        char ** const restrict user,
                        char ** const restrict domain,
                        char * const restrict buf, const size_t buflen,
                        int * const restrict errnop)
{
    /* man setnetgrent() documents param const char *netgroup and subsequent
     * queries are limited to that netgroup.  When implementing setnetgrent()
     * and innetgr() might detect if query in progress and return next entry.
     * (This routine might be a departure from typical get*ent() routines.)
     * (depends how we structure keys/values in mcdb)
     * (might store current netgroup in thread-local storage) */
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_buf,
                                      .vstruct = NULL,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .errnop  = errnop };
    const nss_status_t status =
      _nss_mcdb_getent(NSS_DBTYPE_NETGROUP, &vinfo);
    if (status == NSS_STATUS_SUCCESS) {
        /* success ensures valid data (so that memchr will not return NULL) */
        *host   = buf;
        *user   = (char *)memchr(buf,   '\0', buflen) + 1;
        *domain = (char *)memchr(*user, '\0', buflen - (*user - buf)) + 1;
    }
    return status;
}


nss_status_t
_nss_mcdb_getnetent_r(struct netent * const restrict netbuf,
                      char * const restrict buf, const size_t buflen,
                      int * const restrict errnop,
                      int * const restrict h_errnop)
{
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_netent,
                                      .vstruct = netbuf,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .errnop  = errnop };
    const nss_status_t status =
      _nss_mcdb_getent(NSS_DBTYPE_NETWORKS, &vinfo);
    return (status == NSS_STATUS_SUCCESS)
      ? NSS_STATUS_SUCCESS
      : gethost_fill_h_errnop(status, h_errnop);
}

nss_status_t
_nss_mcdb_getnetbyname_r(const char * const restrict name,
                         struct netent * const restrict netbuf,
                         char * const restrict buf, const size_t buflen,
                         int * const restrict errnop,
                         int * const restrict h_errnop)
{
    const struct _nss_kinfo kinfo = { .key     = name,
                                      .klen    = strlen(name),
                                      .tagc    = (unsigned char)'=' };
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_netent,
                                      .vstruct = netbuf,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .errnop  = errnop };
    const nss_status_t status =
      _nss_mcdb_get_generic(NSS_DBTYPE_NETWORKS, &kinfo, &vinfo);
    return (status == NSS_STATUS_SUCCESS)
      ? NSS_STATUS_SUCCESS
      : gethost_fill_h_errnop(status, h_errnop);
}

nss_status_t
_nss_mcdb_getnetbyaddr_r(const uint32_t net, const int type,
                         struct netent * const restrict netbuf,
                         char * const restrict buf, const size_t buflen,
                         int * const restrict errnop,
                         int * const restrict h_errnop)
{
    char hexstr[16];
    const struct _nss_kinfo kinfo = { .key     = hexstr,
                                      .klen    = sizeof(hexstr),
                                      .tagc    = (unsigned char)'x' };
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_netent,
                                      .vstruct = netbuf,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .errnop  = errnop };
    nss_status_t status;
    uint32_to_ascii8uphex(net, hexstr);
    uint32_to_ascii8uphex((uint32_t)type, hexstr+8);
    status = _nss_mcdb_get_generic(NSS_DBTYPE_NETWORKS, &kinfo, &vinfo);
    return (status == NSS_STATUS_SUCCESS)
      ? NSS_STATUS_SUCCESS
      : gethost_fill_h_errnop(status, h_errnop);
}


nss_status_t
_nss_mcdb_getprotoent_r(struct protoent * const restrict protobuf,
                        char * const restrict buf, const size_t buflen,
                        int * const restrict errnop)
{
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_protoent,
                                      .vstruct = protobuf,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .errnop  = errnop };
    return _nss_mcdb_getent(NSS_DBTYPE_PROTOCOLS, &vinfo);
}

nss_status_t
_nss_mcdb_getprotobyname_r(const char * const restrict name,
                           struct protoent * const restrict protobuf,
                           char * const restrict buf, const size_t buflen,
                           int * const restrict errnop)
{
    const struct _nss_kinfo kinfo = { .key     = name,
                                      .klen    = strlen(name),
                                      .tagc    = (unsigned char)'=' };
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_protoent,
                                      .vstruct = protobuf,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .errnop  = errnop };
    return _nss_mcdb_get_generic(NSS_DBTYPE_PROTOCOLS, &kinfo, &vinfo);
}

nss_status_t
_nss_mcdb_getprotobynumber_r(const int proto,
                             struct protoent * const restrict protobuf,
                             char * const restrict buf, const size_t buflen,
                             int * const restrict errnop)
{
    char hexstr[8];
    const struct _nss_kinfo kinfo = { .key     = hexstr,
                                      .klen    = sizeof(hexstr),
                                      .tagc    = (unsigned char)'x' };
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_protoent,
                                      .vstruct = protobuf,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .errnop  = errnop };
    uint32_to_ascii8uphex((uint32_t)proto, hexstr);
    return _nss_mcdb_get_generic(NSS_DBTYPE_PROTOCOLS, &kinfo, &vinfo);
}


nss_status_t
_nss_mcdb_getrpcent_r(struct rpcent * const restrict rpcbuf,
                      char * const restrict buf, const size_t buflen,
                      int * const restrict errnop)
{
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_rpcent,
                                      .vstruct = rpcbuf,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .errnop  = errnop };
    return _nss_mcdb_getent(NSS_DBTYPE_RPC, &vinfo);
}

nss_status_t
_nss_mcdb_getrpcbyname_r(const char * const restrict name,
                         struct rpcent * const restrict rpcbuf,
                         char * const restrict buf, const size_t buflen,
                         int * const restrict errnop)
{
    const struct _nss_kinfo kinfo = { .key     = name,
                                      .klen    = strlen(name),
                                      .tagc    = (unsigned char)'=' };
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_rpcent,
                                      .vstruct = rpcbuf,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .errnop  = errnop };
    return _nss_mcdb_get_generic(NSS_DBTYPE_RPC, &kinfo, &vinfo);
}

nss_status_t
_nss_mcdb_getrpcbynumber_r(const int number,
                           struct rpcent * const restrict rpcbuf,
                           char * const restrict buf, const size_t buflen,
                           int * const restrict errnop)
{
    char hexstr[8];
    const struct _nss_kinfo kinfo = { .key     = hexstr,
                                      .klen    = sizeof(hexstr),
                                      .tagc    = (unsigned char)'x' };
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_rpcent,
                                      .vstruct = rpcbuf,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .errnop  = errnop };
    uint32_to_ascii8uphex((uint32_t)number, hexstr);
    return _nss_mcdb_get_generic(NSS_DBTYPE_RPC, &kinfo, &vinfo);
}


nss_status_t
_nss_mcdb_getservent_r(struct servent * const restrict servbuf,
                       char * const restrict buf, const size_t buflen,
                       int * const restrict errnop)
{
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_servent,
                                      .vstruct = servbuf,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .errnop  = errnop };
    if (buflen > 0) {
        *buf = '\0';
        return _nss_mcdb_getent(NSS_DBTYPE_SERVICES, &vinfo);
    }
    else {
        *errnop = errno = ERANGE;
        return NSS_STATUS_TRYAGAIN;
    }
}

nss_status_t
_nss_mcdb_getservbyname_r(const char * const restrict name,
                          const char * const restrict proto,
                          struct servent * const restrict servbuf,
                          char * const restrict buf, const size_t buflen,
                          int * const restrict errnop)
{
    const struct _nss_kinfo kinfo = { .key     = name,
                                      .klen    = strlen(name),
                                      .tagc    = (unsigned char)'=' };
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_servent,
                                      .vstruct = servbuf,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .errnop  = errnop };
    const size_t plen = proto != NULL ? strlen(proto) : 0;
    if (buflen > plen) {
        if (plen)  /*copy proto for later match in _nss_mcdb_decode_servent()*/
            memcpy(buf, proto, plen+1);
        else
            *buf = '\0';
        return _nss_mcdb_get_generic(NSS_DBTYPE_SERVICES, &kinfo, &vinfo);
    }
    else {
        *errnop = errno = ERANGE;
        return NSS_STATUS_TRYAGAIN;
    }
}

nss_status_t
_nss_mcdb_getservbyport_r(const int port, const char * const restrict proto,
                          struct servent * const restrict servbuf,
                          char * const restrict buf, const size_t buflen,
                          int * const restrict errnop)
{
    char hexstr[8];
    const struct _nss_kinfo kinfo = { .key     = hexstr,
                                      .klen    = sizeof(hexstr),
                                      .tagc    = (unsigned char)'x' };
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_servent,
                                      .vstruct = servbuf,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .errnop  = errnop };
    const size_t plen = proto != NULL ? strlen(proto) : 0;
    if (buflen > plen) {
        if (plen)  /*copy proto for later match in _nss_mcdb_decode_servent()*/
            memcpy(buf, proto, plen+1);
        else
            *buf = '\0';
        uint32_to_ascii8uphex((uint32_t)port, hexstr);
        return _nss_mcdb_get_generic(NSS_DBTYPE_SERVICES, &kinfo, &vinfo);
    }
    else {
        *errnop = errno = ERANGE;
        return NSS_STATUS_TRYAGAIN;
    }
}


static nss_status_t  __attribute_noinline__
gethost_fill_h_errnop(const nss_status_t status, int * const restrict h_errnop)
{
    switch (status) {
      case NSS_STATUS_TRYAGAIN: *h_errnop = TRY_AGAIN; break;
      case NSS_STATUS_UNAVAIL:  *h_errnop = NO_RECOVERY; break;
      case NSS_STATUS_NOTFOUND: *h_errnop = HOST_NOT_FOUND; break;
      case NSS_STATUS_SUCCESS:  break;
      case NSS_STATUS_RETURN:   *h_errnop = NO_RECOVERY; break;
    }
    return status;
}

static nss_status_t
gethost_query(const uint32_t type,
              const struct _nss_kinfo * const restrict kinfo,
              const struct _nss_vinfo * const restrict vinfo,
              int * const restrict h_errnop)
{
    nss_status_t status;
    if (vinfo->buflen >= 4) {
        /*copy type for later match in _nss_mcdb_decode_hostent()*/
        char * const restrict buf = vinfo->buf;
        buf[0] = (type              ) >> 24;
        buf[1] = (type & 0x00FF0000u) >> 16;
        buf[2] = (type & 0x0000FF00u) >>  8;
        buf[3] = (type & 0x000000FFu);
        status = _nss_mcdb_get_generic(NSS_DBTYPE_HOSTS, kinfo, vinfo);
    }
    else {
        *vinfo->errnop = errno = ERANGE;
        status = NSS_STATUS_TRYAGAIN;
    }
    return (status == NSS_STATUS_SUCCESS)
      ? NSS_STATUS_SUCCESS
      : gethost_fill_h_errnop(status, h_errnop);
}

static nss_status_t
gethost_filladdr(const void * const restrict addr, const int type,
                 const struct _nss_kinfo * const restrict kinfo,
                 const struct _nss_vinfo * const restrict vinfo,
                 int * const restrict h_errnop)
{
    struct hostent * const restrict hostbuf = (struct hostent *)vinfo->vstruct;
    const size_t buflen = vinfo->buflen;
    char * const restrict buf = vinfo->buf;
    char * const aligned = (char *)((uintptr_t)(buf+7) & ~0x7);
    /* supports only AF_INET and AF_INET6
     * (if expanding, might create static array of addr sizes indexed by AF_*)*/
    int h_length;
    switch (type) {
      case AF_INET:  h_length = sizeof(struct in_addr);  break;
      case AF_INET6: h_length = sizeof(struct in6_addr); break;
      default: *vinfo->errnop = errno = ENOENT;
               return gethost_fill_h_errnop(NSS_STATUS_UNAVAIL, h_errnop);
    }          /* other addr types not implemented */
    /* (pointers in struct hostent must be aligned; align to 8 bytes) */
    /* (align+(char **h_aliases)+(char **h_addr_list)+h_length+addrstr+'\0') */
    if ((aligned - buf) + 8 + 8 + h_length + kinfo->klen + 1 >= buflen) {
        *vinfo->errnop = errno = ERANGE;
        return gethost_fill_h_errnop(NSS_STATUS_TRYAGAIN, h_errnop);
    }
    hostbuf->h_name      = memcpy(aligned+16+h_length,kinfo->key,kinfo->klen+1);
    hostbuf->h_aliases   = (char **)aligned;
    hostbuf->h_addrtype  = type;
    hostbuf->h_length    = h_length;
    hostbuf->h_addr_list = (char **)(aligned+8);
    hostbuf->h_aliases[0]   = NULL;
    hostbuf->h_addr_list[0] = memcpy(aligned+16, addr, h_length);
    return NSS_STATUS_SUCCESS;
}


/* Deserializing netdb entries shares much similar code -- some duplicated --
 * but each entry differs slightly so that factoring to common routine would
 * result in excess work.  Therefore, common template was copied and modified */


static nss_status_t
_nss_mcdb_decode_hostent(struct mcdb * const restrict m,
                         const struct _nss_kinfo * const restrict kinfo,
                         const struct _nss_vinfo * const restrict vinfo)
{
    const char * restrict dptr = (char *)mcdb_dataptr(m);
    struct hostent * const he = (struct hostent *)vinfo->vstruct;
    char *buf = vinfo->buf;
    const uint32_t type = (buf[0]<<24) | (buf[1]<<16) | (buf[2]<<8) | buf[3];

    /* match type (e.g. AF_INET, AF_INET6), if not zero (AF_UNSPEC) */
    if (type != 0) {
        while (type != uint32_from_ascii8uphex(dptr+NSS_H_ADDRTYPE)) {
            if (mcdb_findtagnext(m, kinfo->key, kinfo->klen, kinfo->tagc))
                dptr = (char *)mcdb_dataptr(m);
            else {
                *vinfo->errnop = errno = ENOENT;
                return NSS_STATUS_NOTFOUND;
            }
        }
    }

    /* parse fixed format record header to get offsets into strings */
    const uintptr_t idx_he_mem_str=uint16_from_ascii4uphex(dptr+NSS_HE_MEM_STR);
    const uintptr_t idx_he_lst_str=uint16_from_ascii4uphex(dptr+NSS_HE_LST_STR);
    const uintptr_t idx_he_mem    =uint16_from_ascii4uphex(dptr+NSS_HE_MEM);
    const size_t he_mem_num       =uint16_from_ascii4uphex(dptr+NSS_HE_MEM_NUM);
    const size_t he_lst_num       =uint16_from_ascii4uphex(dptr+NSS_HE_LST_NUM);
    char ** const he_mem =
      (char **)(((uintptr_t)(buf+idx_he_mem+0x7u)) & ~0x7u); /* 8-byte align */
    char ** const he_lst = he_mem + he_mem_num + 1;          /* 8-byte align */
    he->h_aliases  = he_mem;
    he->h_addr_list= he_lst;
    he->h_addrtype = (int)         uint32_from_ascii8uphex(dptr+NSS_H_ADDRTYPE);
    he->h_length   = (int)         uint32_from_ascii8uphex(dptr+NSS_H_LENGTH);
    /* populate he string pointers */
    he->h_name    = buf;
    /* fill buf, (char **) he_mem (allow 8-byte ptrs), and terminate strings.
     * scan for ' ' instead of precalculating array because names should
     * be short and adding an extra 4 chars per name to store size takes
     * more space and might take just as long to parse as scan for ' '
     * (assume data consistent, he_mem_num correct) */
    if (((char *)he_mem)-buf+((he_mem_num+1+he_lst_num+1)<<3) <= vinfo->buflen){
        memcpy(buf, dptr+NSS_HE_HDRSZ, mcdb_datalen(m)-NSS_HE_HDRSZ);
        /* terminate strings; replace ' ' separator in data with '\0'. */
        buf[idx_he_mem_str-1] = '\0';        /* terminate h_name */
        buf[idx_he_lst_str-1] = '\0';        /* terminate final he_mem string */
        buf[idx_he_mem-1]     = '\0';        /* terminate final he_lst string */
        he_mem[0] = (buf += idx_he_mem_str); /* begin of he_mem strings */
        for (size_t i=1; i<he_mem_num; ++i) {/*(i=1; assigned first str above)*/
            while (*++buf != ' ')
                ;
            *buf = '\0';
            he_mem[i] = ++buf;
        }
        he_mem[he_mem_num] = NULL;         /* terminate (char **) he_mem array*/
        he_lst[0] = buf = vinfo->buf+idx_he_lst_str; /*begin of he_lst strings*/
        for (size_t i=1; i<he_lst_num; ++i) {/*(i=1; assigned first str above)*/
            while (*++buf != ' ')
                ;
            *buf = '\0';
            he_lst[i] = ++buf;
        }
        he_lst[he_lst_num] = NULL;         /* terminate (char **) he_lst array*/
        return NSS_STATUS_SUCCESS;
    }
    else {
        *vinfo->errnop = errno = ERANGE;
        return NSS_STATUS_TRYAGAIN;
    }
}


static nss_status_t
_nss_mcdb_decode_netent(struct mcdb * const restrict m,
                        const struct _nss_kinfo * const restrict kinfo,
                        const struct _nss_vinfo * const restrict vinfo)
{
    const char * const restrict dptr = (char *)mcdb_dataptr(m);
    struct netent * const ne = (struct netent *)vinfo->vstruct;
    char *buf = vinfo->buf;
    /* parse fixed format record header to get offsets into strings */
    const uintptr_t idx_ne_mem_str=uint16_from_ascii4uphex(dptr+NSS_NE_MEM_STR);
    const uintptr_t idx_ne_mem    =uint16_from_ascii4uphex(dptr+NSS_NE_MEM);
    const size_t ne_mem_num       =uint16_from_ascii4uphex(dptr+NSS_NE_MEM_NUM);
    char ** const restrict ne_mem =
      (char **)(((uintptr_t)(buf+idx_ne_mem+0x7u)) & ~0x7u); /* 8-byte align */
    ne->n_aliases = ne_mem;
    ne->n_addrtype= (int)          uint32_from_ascii8uphex(dptr+NSS_N_ADDRTYPE);
    ne->n_net     =                uint32_from_ascii8uphex(dptr+NSS_N_NET);
    /* populate ne string pointers */
    ne->n_name    = buf;
    /* fill buf, (char **) ne_mem (allow 8-byte ptrs), and terminate strings.
     * scan for ' ' instead of precalculating array because names should
     * be short and adding an extra 4 chars per name to store size takes
     * more space and might take just as long to parse as scan for ' '
     * (assume data consistent, ne_mem_num correct) */
    if (((char *)ne_mem)-buf+((ne_mem_num+1)<<3) <= vinfo->buflen) {
        memcpy(buf, dptr+NSS_NE_HDRSZ, mcdb_datalen(m)-NSS_NE_HDRSZ);
        /* terminate strings; replace ' ' separator in data with '\0'. */
        buf[idx_ne_mem_str-1] = '\0';        /* terminate n_name */
        buf[idx_ne_mem-1]     = '\0';        /* terminate final ne_mem string */
        ne_mem[0] = (buf += idx_ne_mem_str); /* begin of ne_mem strings */
        for (size_t i=1; i<ne_mem_num; ++i) {/*(i=1; assigned first str above)*/
            while (*++buf != ' ')
                ;
            *buf = '\0';
            ne_mem[i] = ++buf;
        }
        ne_mem[ne_mem_num] = NULL;         /* terminate (char **) ne_mem array*/
        return NSS_STATUS_SUCCESS;
    }
    else {
        *vinfo->errnop = errno = ERANGE;
        return NSS_STATUS_TRYAGAIN;
    }
}


static nss_status_t
_nss_mcdb_decode_protoent(struct mcdb * const restrict m,
                          const struct _nss_kinfo * const restrict kinfo,
                          const struct _nss_vinfo * const restrict vinfo)
{
    const char * const restrict dptr = (char *)mcdb_dataptr(m);
    struct protoent * const pe = (struct protoent *)vinfo->vstruct;
    char *buf = vinfo->buf;
    /* parse fixed format pecord header to get offsets into strings */
    const uintptr_t idx_pe_mem_str=uint16_from_ascii4uphex(dptr+NSS_PE_MEM_STR);
    const uintptr_t idx_pe_mem    =uint16_from_ascii4uphex(dptr+NSS_PE_MEM);
    const size_t pe_mem_num       =uint16_from_ascii4uphex(dptr+NSS_PE_MEM_NUM);
    char ** const restrict pe_mem =
      (char **)(((uintptr_t)(buf+idx_pe_mem+0x7u)) & ~0x7u); /* 8-byte align */
    pe->p_aliases = pe_mem;
    pe->p_proto   =                uint32_from_ascii8uphex(dptr+NSS_P_PROTO);
    /* populate pe string pointers */
    pe->p_name    = buf;
    /* fill buf, (char **) pe_mem (allow 8-byte ptrs), and terminate strings.
     * scan for ' ' instead of precalculating array because names should
     * be short and adding an extra 4 chars per name to store size takes
     * more space and might take just as long to parse as scan for ' '
     * (assume data consistent, pe_mem_num correct) */
    if (((char *)pe_mem)-buf+((pe_mem_num+1)<<3) <= vinfo->buflen) {
        memcpy(buf, dptr+NSS_PE_HDRSZ, mcdb_datalen(m)-NSS_PE_HDRSZ);
        /* terminate strings; replace ' ' separator in data with '\0'. */
        buf[idx_pe_mem_str-1] = '\0';        /* terminate p_name */
        buf[idx_pe_mem-1]     = '\0';        /* terminate final pe_mem string */
        pe_mem[0] = (buf += idx_pe_mem_str); /* begin of pe_mem strings */
        for (size_t i=1; i<pe_mem_num; ++i) {/*(i=1; assigned first str above)*/
            while (*++buf != ' ')
                ;
            *buf = '\0';
            pe_mem[i] = ++buf;
        }
        pe_mem[pe_mem_num] = NULL;         /* terminate (char **) pe_mem array*/
        return NSS_STATUS_SUCCESS;
    }
    else {
        *vinfo->errnop = errno = ERANGE;
        return NSS_STATUS_TRYAGAIN;
    }
}


static nss_status_t
_nss_mcdb_decode_rpcent(struct mcdb * const restrict m,
                        const struct _nss_kinfo * const restrict kinfo,
                        const struct _nss_vinfo * const restrict vinfo)
{
    const char * const restrict dptr = (char *)mcdb_dataptr(m);
    struct rpcent * const re = (struct rpcent *)vinfo->vstruct;
    char *buf = vinfo->buf;
    /* parse fixed format record header to get offsets into strings */
    const uintptr_t idx_re_mem_str=uint16_from_ascii4uphex(dptr+NSS_RE_MEM_STR);
    const uintptr_t idx_re_mem    =uint16_from_ascii4uphex(dptr+NSS_RE_MEM);
    const size_t re_mem_num       =uint16_from_ascii4uphex(dptr+NSS_RE_MEM_NUM);
    char ** const restrict re_mem =
      (char **)(((uintptr_t)(buf+idx_re_mem+0x7u)) & ~0x7u); /* 8-byte align */
    re->r_aliases = re_mem;
    re->r_number  =                uint32_from_ascii8uphex(dptr+NSS_R_NUMBER);
    /* populate re string pointers */
    re->r_name    = buf;
    /* fill buf, (char **) re_mem (allow 8-byte ptrs), and terminate strings.
     * scan for ' ' instead of precalculating array because names should
     * be short and adding an extra 4 chars per name to store size takes
     * more space and might take just as long to parse as scan for ' '
     * (assume data consistent, re_mem_num correct) */
    if (((char *)re_mem)-buf+((re_mem_num+1)<<3) <= vinfo->buflen) {
        memcpy(buf, dptr+NSS_RE_HDRSZ, mcdb_datalen(m)-NSS_RE_HDRSZ);
        /* terminate strings; replace ' ' separator in data with '\0'. */
        buf[idx_re_mem_str-1] = '\0';        /* terminate r_name */
        buf[idx_re_mem-1]     = '\0';        /* terminate final re_mem string */
        re_mem[0] = (buf += idx_re_mem_str); /* begin of re_mem strings */
        for (size_t i=1; i<re_mem_num; ++i) {/*(i=1; assigned first str above)*/
            while (*++buf != ' ')
                ;
            *buf = '\0';
            re_mem[i] = ++buf;
        }
        re_mem[re_mem_num] = NULL;         /* terminate (char **) re_mem array*/
        return NSS_STATUS_SUCCESS;
    }
    else {
        *vinfo->errnop = errno = ERANGE;
        return NSS_STATUS_TRYAGAIN;
    }
}


static nss_status_t
_nss_mcdb_decode_servent(struct mcdb * const restrict m,
                         const struct _nss_kinfo * const restrict kinfo,
                         const struct _nss_vinfo * const restrict vinfo)
{
    const char * restrict dptr = (char *)mcdb_dataptr(m);
    struct servent * const se = (struct servent *)vinfo->vstruct;
    char *buf = vinfo->buf; /* contains proto string to match (if not "") */

    /* match proto string (stored right after header), if not empty string */
    /* (future: could possibly be further optimized for "tcp" and "udp"
     * (future: might add unique tag char db ents for tcp/udp by name/number) */
    if (*buf != '\0') {
        const size_t protolen = strlen(buf);
        while (dptr[NSS_SE_HDRSZ] != *buf  /* e.g. "tcp" vs "udp" */
               || memcmp(dptr+NSS_SE_HDRSZ, buf, protolen) != 0
               || dptr[NSS_SE_HDRSZ+protolen] != ' ') {
            if (mcdb_findtagnext(m, kinfo->key, kinfo->klen, kinfo->tagc))
                dptr = (char *)mcdb_dataptr(m);
            else {
                *vinfo->errnop = errno = ENOENT;
                return NSS_STATUS_NOTFOUND;
            }
        }
    }

    /* parse fixed format record header to get offsets into strings */
    const uintptr_t idx_s_name    =uint16_from_ascii4uphex(dptr+NSS_S_NAME);
    const uintptr_t idx_se_mem_str=uint16_from_ascii4uphex(dptr+NSS_SE_MEM_STR);
    const uintptr_t idx_se_mem    =uint16_from_ascii4uphex(dptr+NSS_SE_MEM);
    const size_t se_mem_num       =uint16_from_ascii4uphex(dptr+NSS_SE_MEM_NUM);
    char ** const restrict se_mem =
      (char **)(((uintptr_t)(buf+idx_se_mem+0x7u)) & ~0x7u); /* 8-byte align */
    se->s_aliases = se_mem;
    se->s_port    =                uint32_from_ascii8uphex(dptr+NSS_S_PORT);
    /* populate se string pointers */
    se->s_proto   = buf;
    se->s_name    = buf+idx_s_name;
    /* fill buf, (char **) se_mem (allow 8-byte ptrs), and terminate strings.
     * scan for ' ' instead of precalculating array because names should
     * be short and adding an extra 4 chars per name to store size takes
     * more space and might take just as long to parse as scan for ' '
     * (assume data consistent, se_mem_num correct) */
    if (((char *)se_mem)-buf+((se_mem_num+1)<<3) <= vinfo->buflen) {
        memcpy(buf, dptr+NSS_SE_HDRSZ, mcdb_datalen(m)-NSS_SE_HDRSZ);
        /* terminate strings; replace ' ' separator in data with '\0'. */
        buf[idx_s_name-1]     = '\0';        /* terminate s_proto */
        buf[idx_se_mem_str-1] = '\0';        /* terminate s_name */
        buf[idx_se_mem-1]     = '\0';        /* terminate final se_mem string */
        se_mem[0] = (buf += idx_se_mem_str); /* begin of se_mem strings */
        for (size_t i=1; i<se_mem_num; ++i) {/*(i=1; assigned first str above)*/
            while (*++buf != ' ')
                ;
            *buf = '\0';
            se_mem[i] = ++buf;
        }
        se_mem[se_mem_num] = NULL;         /* terminate (char **) se_mem array*/
        return NSS_STATUS_SUCCESS;
    }
    else {
        *vinfo->errnop = errno = ERANGE;
        return NSS_STATUS_TRYAGAIN;
    }
}
