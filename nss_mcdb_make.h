#ifndef INCLUDED_NSS_MCDB_MAKE_H
#define INCLUDED_NSS_MCDB_MAKE_H

#include "mcdb.h"
#include "code_attributes.h"

#include <stdbool.h>

struct nss_mcdb_make_wbuf {
  struct mcdb_make * const restrict m;
  char * const restrict buf;
  size_t offset;
  size_t bufsz;
  int fd;
};

struct nss_mcdb_make_winfo {
  struct nss_mcdb_make_wbuf wbuf;
  bool (*encode)(struct nss_mcdb_make_winfo * restrict, void * restrict);
  char * restrict data;
  size_t datasz;
  size_t dlen;
  void * restrict extra;
  const char * restrict key;
  size_t klen;
  char tagc;
};

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
