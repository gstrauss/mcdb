/* _BSD_SOURCE or _SVID_SOURCE for struct rpcent on Linux */
#define _BSD_SOURCE

#include "nss_mcdb_netdb.h"
#include "nss_mcdb.h"
#include "mcdb.h"
#include "mcdb_uint32.h"
#include "mcdb_attribute.h"

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

static enum nss_status
_nss_mcdb_decode_hostent(struct mcdb * restrict,
                         const struct _nss_kinfo * restrict,
                         const struct _nss_vinfo * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;

static enum nss_status
_nss_mcdb_decode_netent(struct mcdb * restrict,
                        const struct _nss_kinfo * restrict,
                        const struct _nss_vinfo * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;

static enum nss_status
_nss_mcdb_decode_protoent(struct mcdb * restrict,
                          const struct _nss_kinfo * restrict,
                          const struct _nss_vinfo * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;

static enum nss_status
_nss_mcdb_decode_rpcent(struct mcdb * restrict,
                        const struct _nss_kinfo * restrict,
                        const struct _nss_vinfo * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;

static enum nss_status
_nss_mcdb_decode_servent(struct mcdb * restrict,
                         const struct _nss_kinfo * restrict,
                         const struct _nss_vinfo * restrict)
  __attribute_nonnull__  __attribute_warn_unused_result__;


static enum nss_status  __attribute_noinline__
gethost_fill_h_errnop(enum nss_status status, int * const restrict h_errnop)
  __attribute_cold__  __attribute_nonnull__;

static enum nss_status
gethost_query(const int type,
              const struct _nss_kinfo * restrict kinfo,
              const struct _nss_vinfo * restrict vinfo,
              int * const restrict h_errnop)
  __attribute_nonnull__;

static enum nss_status
gethost_filladdr(const void * const restrict addr, const int type,
                 const struct _nss_kinfo * restrict kinfo,
                 const struct _nss_vinfo * restrict vinfo,
                 int * const restrict h_errnop)
  __attribute_nonnull__;


void _nss_files_sethostent(void)  { _nss_mcdb_setent(NSS_DBTYPE_HOSTS);     }
void _nss_files_endhostent(void)  { _nss_mcdb_endent(NSS_DBTYPE_HOSTS);     }
void _nss_files_setnetgrent(void) { _nss_mcdb_setent(NSS_DBTYPE_NETGROUP);  }
void _nss_files_endnetgrent(void) { _nss_mcdb_endent(NSS_DBTYPE_NETGROUP);  }
void _nss_files_setnetent(void)   { _nss_mcdb_setent(NSS_DBTYPE_NETWORKS);  }
void _nss_files_endnetent(void)   { _nss_mcdb_endent(NSS_DBTYPE_NETWORKS);  }
void _nss_files_setprotoent(void) { _nss_mcdb_setent(NSS_DBTYPE_PROTOCOLS); }
void _nss_files_endprotoent(void) { _nss_mcdb_endent(NSS_DBTYPE_PROTOCOLS); }
void _nss_files_setrpcent(void)   { _nss_mcdb_setent(NSS_DBTYPE_RPC);       }
void _nss_files_endrpcent(void)   { _nss_mcdb_endent(NSS_DBTYPE_RPC);       }
void _nss_files_setservent(void)  { _nss_mcdb_setent(NSS_DBTYPE_SERVICES);  }
void _nss_files_endservent(void)  { _nss_mcdb_endent(NSS_DBTYPE_SERVICES);  }


/* POSIX.1-2001 marks gethostbyaddr() and gethostbyname() obsolescent.
 * See man getnameinfo(), getaddrinfo(), freeaddrinfo(), gai_strerror()
 * However, note that getaddrinfo() allocates memory for its results 
 * (linked list of struct addrinfo) that must be free()d with freeaddrinfo(). */

enum nss_status
_nss_files_gethostent_r(struct hostent * const restrict hostbuf,
                        char * const restrict buf, const size_t buflen,
                        struct hostent ** const restrict hostbufp,
                        int * const restrict h_errnop)
{
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_hostent,
                                      .vstruct = hostbuf,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .vstructp= hostbufp };
    enum nss_status status;
    *hostbufp = NULL;
    status = _nss_mcdb_getent(NSS_DBTYPE_HOSTS, &vinfo);
    return (status == NSS_STATUS_SUCCESS)
      ? NSS_STATUS_SUCCESS
      : gethost_fill_h_errnop(status, h_errnop);
}

enum nss_status
_nss_files_gethostbyname2_r(const char * const restrict name, const int type,
                            struct hostent * const restrict hostbuf,
                            char * const restrict buf, const size_t buflen,
                            struct hostent ** const restrict hostbufp,
                            int * const restrict h_errnop)
{
    const struct _nss_kinfo kinfo = { .key     = name,
                                      .klen    = strlen(name),
                                      .tagc    = (unsigned char)'=' };
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_hostent,
                                      .vstruct = hostbuf,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .vstructp= hostbufp };
    uint64_t addr[2] = {0,0};/* support addr sizes up to 128-bits (e.g. IPv6) */
    const int is_addr = inet_pton(type, name, &addr);
    *hostbufp = NULL;
    if (is_addr == 0) /* name is not valid address for specified addr family */
        return gethost_query(type, &kinfo, &vinfo, h_errnop);
    else if (is_addr > 0) /* name is valid address for specified addr family */
        return gethost_filladdr(&addr, type, &kinfo, &vinfo, h_errnop);
    else
        return gethost_fill_h_errnop(NSS_STATUS_RETURN, h_errnop);
}

enum nss_status
_nss_files_gethostbyname_r(const char * const restrict name,
                           struct hostent * const restrict hostbuf,
                           char * const restrict buf, const size_t buflen,
                           struct hostent ** const restrict hostbufp,
                           int * const restrict h_errnop)
{
    return _nss_files_gethostbyname2_r(name, AF_INET, hostbuf,
                                       buf, buflen, hostbufp, h_errnop);
}

enum nss_status
_nss_files_gethostbyaddr_r(const void * const restrict addr,
                           const socklen_t len, const int type,
                           struct hostent * const restrict hostbuf,
                           char * const restrict buf, const size_t buflen,
                           struct hostent ** const restrict hostbufp,
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
                                      .vstructp= hostbufp };
    uint32_t u[4];  /* align data to uint32_t for uphex conversion routines */
    *hostbufp = NULL;
    switch (len) {
      case 4:  /* e.g. AF_INET4 */
               memcpy(&u, addr, 4);
               mcdb_uint32_to_ascii8uphex(u[0], hexstr);
               break;
      case 16: /* e.g. AF_INET6 */
               memcpy(&u, addr, 16);
               mcdb_uint32_to_ascii8uphex(u[0], hexstr);
               mcdb_uint32_to_ascii8uphex(u[1], hexstr+8);
               mcdb_uint32_to_ascii8uphex(u[2], hexstr+16);
               mcdb_uint32_to_ascii8uphex(u[3], hexstr+24);
               break;
      default: return NSS_STATUS_UNAVAIL; /* other types not implemented */
    }

    return gethost_query(type, &kinfo, &vinfo, h_errnop);
}


enum nss_status
_nss_files_getnetgrent_r(char ** const restrict host,
                         char ** const restrict user,
                         char ** const restrict domain,
                         char * const restrict buf, const size_t buflen)
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
                                      .vstructp= NULL };
    const enum nss_status status =
      _nss_mcdb_getent(NSS_DBTYPE_NETGROUP, &vinfo);
    if (status == NSS_STATUS_SUCCESS) {
        /* success ensures valid data (so that memchr will not return NULL) */
        *host   = buf;
        *user   = (char *)memchr(buf,   '\0', buflen) + 1;
        *domain = (char *)memchr(*user, '\0', buflen - (*user - buf)) + 1;
    }
    return status;
}


enum nss_status
_nss_files_getnetent_r(struct netent * const restrict netbuf,
                       char * const restrict buf, const size_t buflen,
                       struct netent ** const restrict netbufp)
{
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_netent,
                                      .vstruct = netbuf,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .vstructp= netbufp };
    *netbufp = NULL;
    return _nss_mcdb_getent(NSS_DBTYPE_NETWORKS, &vinfo);
}

enum nss_status
_nss_files_getnetbyname_r(const char * const restrict name,
                          struct netent * const restrict netbuf,
                          char * const restrict buf, const size_t buflen,
                          struct netent ** const restrict netbufp)
{
    const struct _nss_kinfo kinfo = { .key     = name,
                                      .klen    = strlen(name),
                                      .tagc    = (unsigned char)'=' };
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_netent,
                                      .vstruct = netbuf,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .vstructp= netbufp };
    *netbufp = NULL;
    return _nss_mcdb_get_generic(NSS_DBTYPE_NETWORKS, &kinfo, &vinfo);
}

enum nss_status
_nss_files_getnetbyaddr_r(const uint32_t net, const int type,
                          struct netent * const restrict netbuf,
                          char * const restrict buf, const size_t buflen,
                          struct netent ** const restrict netbufp)
{
    char hexstr[16];
    const struct _nss_kinfo kinfo = { .key     = hexstr,
                                      .klen    = sizeof(hexstr),
                                      .tagc    = (unsigned char)'x' };
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_netent,
                                      .vstruct = netbuf,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .vstructp= netbufp };
    *netbufp = NULL;
    mcdb_uint32_to_ascii8uphex(net, hexstr);
    mcdb_uint32_to_ascii8uphex((uint32_t)type, hexstr+8);
    return _nss_mcdb_get_generic(NSS_DBTYPE_NETWORKS, &kinfo, &vinfo);
}


enum nss_status
_nss_files_getprotoent_r(struct protoent * const restrict protobuf,
                         char * const restrict buf, const size_t buflen,
                         struct protoent ** const restrict protobufp)
{
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_protoent,
                                      .vstruct = protobuf,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .vstructp= protobufp };
    *protobufp = NULL;
    return _nss_mcdb_getent(NSS_DBTYPE_PROTOCOLS, &vinfo);
}

enum nss_status
_nss_files_getprotobyname_r(const char * const restrict name,
                            struct protoent * const restrict protobuf,
                            char * const restrict buf, const size_t buflen,
                            struct protoent ** const restrict protobufp)
{
    const struct _nss_kinfo kinfo = { .key     = name,
                                      .klen    = strlen(name),
                                      .tagc    = (unsigned char)'=' };
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_protoent,
                                      .vstruct = protobuf,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .vstructp= protobufp };
    *protobufp = NULL;
    return _nss_mcdb_get_generic(NSS_DBTYPE_PROTOCOLS, &kinfo, &vinfo);
}

enum nss_status
_nss_files_getprotobynumber_r(const int proto,
                              struct protoent * const restrict protobuf,
                              char * const restrict buf, const size_t buflen,
                              struct protoent ** const restrict protobufp)
{
    char hexstr[8];
    const struct _nss_kinfo kinfo = { .key     = hexstr,
                                      .klen    = sizeof(hexstr),
                                      .tagc    = (unsigned char)'x' };
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_protoent,
                                      .vstruct = protobuf,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .vstructp= protobufp };
    *protobufp = NULL;
    mcdb_uint32_to_ascii8uphex((uint32_t)proto, hexstr);
    return _nss_mcdb_get_generic(NSS_DBTYPE_PROTOCOLS, &kinfo, &vinfo);
}


enum nss_status
_nss_files_getrpcent_r(struct rpcent * const restrict rpcbuf,
                       char * const restrict buf, const size_t buflen,
                       struct rpcent ** const restrict rpcbufp)
{
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_rpcent,
                                      .vstruct = rpcbuf,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .vstructp= rpcbufp };
    *rpcbufp = NULL;
    return _nss_mcdb_getent(NSS_DBTYPE_RPC, &vinfo);
}

enum nss_status
_nss_files_getrpcbyname_r(const char * const restrict name,
                          struct rpcent * const restrict rpcbuf,
                          char * const restrict buf, const size_t buflen,
                          struct rpcent ** const restrict rpcbufp)
{
    const struct _nss_kinfo kinfo = { .key     = name,
                                      .klen    = strlen(name),
                                      .tagc    = (unsigned char)'=' };
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_rpcent,
                                      .vstruct = rpcbuf,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .vstructp= rpcbufp };
    *rpcbufp = NULL;
    return _nss_mcdb_get_generic(NSS_DBTYPE_RPC, &kinfo, &vinfo);
}

enum nss_status
_nss_files_getrpcbynumber_r(const int number,
                            struct rpcent * const restrict rpcbuf,
                            char * const restrict buf, const size_t buflen,
                            struct rpcent ** const restrict rpcbufp)
{
    char hexstr[8];
    const struct _nss_kinfo kinfo = { .key     = hexstr,
                                      .klen    = sizeof(hexstr),
                                      .tagc    = (unsigned char)'x' };
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_rpcent,
                                      .vstruct = rpcbuf,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .vstructp= rpcbufp };
    *rpcbufp = NULL;
    mcdb_uint32_to_ascii8uphex((uint32_t)number, hexstr);
    return _nss_mcdb_get_generic(NSS_DBTYPE_RPC, &kinfo, &vinfo);
}


enum nss_status
_nss_files_getservent_r(struct servent * const restrict servbuf,
                        char * const restrict buf, const size_t buflen,
                        struct servent ** const restrict servbufp)
{
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_servent,
                                      .vstruct = servbuf,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .vstructp= servbufp };
    *servbufp = NULL;
    if (buflen > 0) {
        *buf = '\0';
        return _nss_mcdb_getent(NSS_DBTYPE_SERVICES, &vinfo);
    }
    else {
        errno = ERANGE;
        return NSS_STATUS_TRYAGAIN;
    }
}

enum nss_status
_nss_files_getservbyname_r(const char * const restrict name,
                           const char * const restrict proto,
                           struct servent * const restrict servbuf,
                           char * const restrict buf, const size_t buflen,
                           struct servent ** const restrict servbufp)
{
    const struct _nss_kinfo kinfo = { .key     = name,
                                      .klen    = strlen(name),
                                      .tagc    = (unsigned char)'=' };
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_servent,
                                      .vstruct = servbuf,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .vstructp= servbufp };
    const size_t plen = proto != NULL ? strlen(proto) : 0;
    *servbufp = NULL;
    if (buflen > plen) {
        if (plen)  /*copy proto for later match in _nss_mcdb_decode_servent()*/
            memcpy(buf, proto, plen+1);
        else
            *buf = '\0';
        return _nss_mcdb_get_generic(NSS_DBTYPE_SERVICES, &kinfo, &vinfo);
    }
    else {
        errno = ERANGE;
        return NSS_STATUS_TRYAGAIN;
    }
}

enum nss_status
_nss_files_getservbyport_r(const int port, const char * const restrict proto,
                           struct servent * const restrict servbuf,
                           char * const restrict buf, const size_t buflen,
                           struct servent ** const restrict servbufp)
{
    char hexstr[8];
    const struct _nss_kinfo kinfo = { .key     = hexstr,
                                      .klen    = sizeof(hexstr),
                                      .tagc    = (unsigned char)'x' };
    const struct _nss_vinfo vinfo = { .decode  = _nss_mcdb_decode_servent,
                                      .vstruct = servbuf,
                                      .buf     = buf,
                                      .buflen  = buflen,
                                      .vstructp= servbufp };
    const size_t plen = proto != NULL ? strlen(proto) : 0;
    *servbufp = NULL;
    if (buflen > plen) {
        if (plen)  /*copy proto for later match in _nss_mcdb_decode_servent()*/
            memcpy(buf, proto, plen+1);
        else
            *buf = '\0';
        mcdb_uint32_to_ascii8uphex((uint32_t)port, hexstr);
        return _nss_mcdb_get_generic(NSS_DBTYPE_SERVICES, &kinfo, &vinfo);
    }
    else {
        errno = ERANGE;
        return NSS_STATUS_TRYAGAIN;
    }
}


static enum nss_status  __attribute_noinline__
gethost_fill_h_errnop(enum nss_status status, int * const restrict h_errnop)
{
    switch (status) {
      case NSS_STATUS_TRYAGAIN: *h_errnop = TRY_AGAIN; break;
      case NSS_STATUS_UNAVAIL:  *h_errnop = NO_RECOVERY; break;
      case NSS_STATUS_NOTFOUND: *h_errnop = HOST_NOT_FOUND; break;
      case NSS_STATUS_SUCCESS:  break;
      case NSS_STATUS_RETURN:   *h_errnop = TRY_AGAIN; break;
    }
    return status;
}

static enum nss_status
gethost_query(const int type,
              const struct _nss_kinfo * restrict kinfo,
              const struct _nss_vinfo * restrict vinfo,
              int * const restrict h_errnop)
{
    enum nss_status status;
    if (vinfo->buflen >= 4) {
        /*copy type for later match in _nss_mcdb_decode_hostent()*/
        char * const restrict buf = vinfo->buf;
        buf[0] = (((uint32_t)type)              ) >> 24;
        buf[1] = (((uint32_t)type) & 0x00FF0000u) >> 16;
        buf[2] = (((uint32_t)type) & 0x0000FF00u) >>  8;
        buf[3] = (((uint32_t)type) & 0x000000FFu);
        status = _nss_mcdb_get_generic(NSS_DBTYPE_HOSTS, kinfo, vinfo);
    }
    else {
        errno = ERANGE;
        status = NSS_STATUS_TRYAGAIN;
    }
    return (status == NSS_STATUS_SUCCESS)
      ? NSS_STATUS_SUCCESS
      : gethost_fill_h_errnop(status, h_errnop);
}

static enum nss_status
gethost_filladdr(const void * const restrict addr, const int type,
                 const struct _nss_kinfo * restrict kinfo,
                 const struct _nss_vinfo * restrict vinfo,
                 int * const restrict h_errnop)
{
    struct hostent * const restrict hostbuf = (struct hostent *)vinfo->vstruct;
    struct hostent ** const restrict hostbufp=(struct hostent**)vinfo->vstructp;
    const size_t buflen = vinfo->buflen;
    char * const restrict buf = vinfo->buf;
    char * const aligned = (char *)((uintptr_t)(buf+7) & ~0x7);
    /* supports only AF_INET and AF_INET6
     * (future: create static array of addr sizes indexed by AF_*) */
    int h_length;
    switch (type) {
      case AF_INET:  h_length = sizeof(struct in_addr);  break;
      case AF_INET6: h_length = sizeof(struct in6_addr); break;
      default: return gethost_fill_h_errnop(NSS_STATUS_RETURN, h_errnop);
    }          /* other addr types not implemented */
    /* (pointers in struct hostent must be aligned; align to 8 bytes) */
    /* (align+(char **h_aliases)+(char **h_addr_list)+h_length+addrstr+'\0') */
    if ((aligned - buf) + 8 + 8 + h_length + kinfo->klen + 1 >= buflen)
        return gethost_fill_h_errnop(NSS_STATUS_TRYAGAIN, h_errnop);
    hostbuf->h_name      = memcpy(aligned+16+h_length,kinfo->key,kinfo->klen+1);
    hostbuf->h_aliases   = (char **)aligned;
    hostbuf->h_addrtype  = type;
    hostbuf->h_length    = h_length;
    hostbuf->h_addr_list = (char **)(aligned+8);
    hostbuf->h_aliases[0]   = NULL;
    hostbuf->h_addr_list[0] = memcpy(aligned+16, addr, h_length);
    *hostbufp = hostbuf;
    return NSS_STATUS_SUCCESS;
}


static enum nss_status
_nss_mcdb_decode_hostent(struct mcdb * restrict m,
                         const struct _nss_kinfo * restrict kinfo,
                         const struct _nss_vinfo * restrict vinfo)
{
    /* TODO: check type match from buf: mcdb_findtagnext() until match
     * buf[0-3] = 0 and take first match */
    /* creation from /etc/hosts needs to read file and bunch together multiple
     * IPs for 'host' records that have multiple IPs since keys have to be
     * unique in mcdb.  Be sure to support IPv4 and IPv6  ==> Perl! :) */
    return NSS_STATUS_SUCCESS;
}

static enum nss_status
_nss_mcdb_decode_netent(struct mcdb * restrict m,
                        const struct _nss_kinfo * restrict kinfo,
                        const struct _nss_vinfo * restrict vinfo)
{
    /* TODO */
    return NSS_STATUS_SUCCESS;
}

static enum nss_status
_nss_mcdb_decode_protoent(struct mcdb * restrict m,
                          const struct _nss_kinfo * restrict kinfo,
                          const struct _nss_vinfo * restrict vinfo)
{
    /* TODO */
    /* creation from /etc/protocols
     * '#' to end of line is ignored
     * empty lines are ignored
     * any number of spaces or tabs separates fields
     * protocol number aliase(es)(optional)
     */
    return NSS_STATUS_SUCCESS;
}

static enum nss_status
_nss_mcdb_decode_rpcent(struct mcdb * restrict m,
                        const struct _nss_kinfo * restrict kinfo,
                        const struct _nss_vinfo * restrict vinfo)
{
    /* TODO */
    return NSS_STATUS_SUCCESS;
}

static enum nss_status
_nss_mcdb_decode_servent(struct mcdb * restrict m,
                         const struct _nss_kinfo * restrict kinfo,
                         const struct _nss_vinfo * restrict vinfo)
{
    /* TODO: check proto in buf != "" and mcdb_findtagnext() until match 
     *   (buf = "" and take first match) */
    return NSS_STATUS_SUCCESS;
}
