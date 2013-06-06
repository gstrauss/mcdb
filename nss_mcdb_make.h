/*
 * nss_mcdb_make - common routines to create mcdb of nsswitch.conf databases
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

#ifndef INCLUDED_NSS_MCDB_MAKE_H
#define INCLUDED_NSS_MCDB_MAKE_H

#include "plasma/plasma_feature.h"
#include "plasma/plasma_attr.h"
#include "plasma/plasma_stdtypes.h"
#include "mcdb_make.h"

struct nss_mcdb_make_wbuf {
  struct mcdb_make * restrict m;
  char * const restrict buf;
  size_t offset;
  size_t bufsz;
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

#define TOKEN_POUNDCOMMENT_SKIP(p) \
 { do { ++p; } while (*p!='\n' && *p!='\0'); if (*p!='\0') continue; break; }


#endif
