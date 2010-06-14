#ifndef INCLUDED_NSS_MCDB_MAKE_H
#define INCLUDED_NSS_MCDB_MAKE_H

#include "mcdb.h"
#include "code_attributes.h"

#include <stdbool.h>

struct _nss_wbuf {
  struct mcdb_make * const restrict m;
  char * const restrict buf;
  size_t offset;
  size_t bufsz;
  int fd;
};

struct _nss_winfo {
  struct _nss_wbuf w;
  bool (*encode)(struct _nss_winfo * restrict, void * restrict);
  char * restrict data;
  size_t datasz;
  size_t dlen;
  void * restrict extra;
  const char * restrict key;
  size_t klen;
  char tagc;
};

#endif
