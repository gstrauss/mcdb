#ifndef INCLUDED_NSS_MCDB_MAKE_H
#define INCLUDED_NSS_MCDB_MAKE_H

#include "mcdb_make.h"
#include "code_attributes.h"

#include <stdbool.h>

struct nss_mcdb_make_wbuf {
  struct mcdb_make * restrict m;
  char * const restrict buf;
  size_t offset;
  size_t bufsz;
  int fd;
};

struct nss_mcdb_make_winfo {
  struct nss_mcdb_make_wbuf wbuf;
  bool (*encode)(struct nss_mcdb_make_winfo * restrict, const void * restrict);
  bool (*flush)(struct nss_mcdb_make_winfo * restrict);
  char * restrict data;
  size_t datasz;
  size_t dlen;
  void * restrict extra;
  const char * restrict key;
  size_t klen;
  char tagc;
};


/*
 * Note: mcdb *_make_* routines are not thread-safe
 * (no need for thread-safety; mcdb is typically created from a single stream)
 */


bool
nss_mcdb_make_mcdbctl_write(struct nss_mcdb_make_winfo * restrict)
  __attribute_nonnull__;

bool
nss_mcdb_make_dbfile( struct nss_mcdb_make_winfo * restrict,
                      const char * restrict,
                      bool (*)(struct nss_mcdb_make_winfo * restrict,
                               char * restrict) )
  __attribute_nonnull__;


#define TOKEN_WSDELIM_BEGIN(p) \
 while (*(p)==' ' || *(p)=='\t') ++(p)

#define TOKEN_WSDELIM_END(p) \
 while (*(p)!=' ' && *(p)!='\t' && *(p)!='#' && *(p)!='\n' && *(p)!='\0') ++(p)

#define TOKEN_COLONDELIM_END(p) \
 while (*(p)!=':' && *(p)!='\n' && *(p)!='\0') ++(p)

#define TOKEN_COLONHASHDELIM_END(p) \
 while (*(p)!=':' && *(p)!='#' && *(p)!='\n' && *(p)!='\0') ++(p)

#define TOKEN_COMMADELIM_END(p) \
 while (*(p)!=',' && *(p)!='\n' && *(p)!='\0') ++(p)

#define TOKEN_COMMAHASHDELIM_END(p) \
 while (*(p)!=',' && *(p)!='#' && *(p)!='\n' && *(p)!='\0') ++(p)


#endif
