/* 
 * Copyright (C) 2011, Glue Logic LLC. All rights reserved. code()gluelogic.com
 * 
 * This library is free software; you can redistribute it and/or modify
 * it under the same terms as Perl itself, either Perl version 5.12.4 or,
 * at your option, any later version of Perl 5 you may have available.
 */

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <mcdb/mcdb.h>
#include <mcdb/mcdb_make.h>
#include <mcdb/mcdb_makefn.h>

struct mcdbxs_read {
    struct mcdb m;
    struct mcdb_iter iter;
    bool values;            /* flag for values() processing */
};

static SV *
mcdbxs_svcp (const unsigned char * const restrict p, const STRLEN len)
{
    /* copy and append '\0'-termination */
    SV * const restrict sv = newSVpvn((const char *)p, len+1);
    SvCUR_set(sv, len);
    (SvPVX(sv))[len] = '\0';
    return sv;
}

static bool
mcdbxs_is_iterkey (struct mcdb_iter * const restrict iter,
                   const char * const restrict kp, const STRLEN klen)
{
    return (iter->ptr
            && klen == mcdb_iter_keylen(iter)
            && 0 == memcmp(mcdb_iter_keyptr(iter), kp, klen))
           || ((iter->ptr = NULL), false);
}

static SV *
mcdbxs_nextkey (struct mcdbxs_read * const restrict this)
{
    struct mcdb_iter * const restrict iter = &this->iter;
    /* ? sometimes NEXTKEY gets called before FIRSTKEY if hash gets re-tied ? */
    if (!iter->ptr)
        mcdb_iter_init(iter, &this->m);
    return mcdb_iter(iter)
      ? mcdbxs_svcp(mcdb_iter_keyptr(iter), mcdb_iter_keylen(iter))
      : (SV *)(iter->ptr = NULL);
}

static void *
mcdbxs_malloc (size_t sz)
{
    void * restrict x;
    return Newx(x, sz, char);
}

static void
mcdbxs_free (void *p)
{
    Safefree(p);
}

static void
mcdbxs_make_destroy (struct mcdb_make * const restrict mk)
{
    mcdb_make_destroy(mk);
    mcdb_makefn_cleanup(mk);
    Safefree(mk);
}


MODULE = MCDB_File		PACKAGE = MCDB_File	PREFIX = mcdbxs_

PROTOTYPES: DISABLED

struct mcdbxs_read *
mcdbxs_TIEHASH(CLASS, filename)
    char * CLASS;
    char * filename;
  CODE:
    Newx(RETVAL, 1, struct mcdbxs_read);
    RETVAL->m.map = RETVAL->iter.map =
      mcdb_mmap_create(NULL, NULL, filename, mcdbxs_malloc, mcdbxs_free);
    if (RETVAL->iter.map) {
        RETVAL->iter.ptr  = NULL;
        RETVAL->iter.eod  = NULL;
        RETVAL->values    = false;
    }
    else {
        Safefree(RETVAL);
        XSRETURN_NO;
    }
  OUTPUT:
    RETVAL

U32
mcdbxs_SCALAR(this)
    struct mcdbxs_read * this;
  CODE:
    RETVAL = mcdb_numrecs(&this->m);
  OUTPUT:
    RETVAL

int
mcdbxs_EXISTS(this, k)
    struct mcdbxs_read * this;
    SV * k;
  PREINIT:
    STRLEN klen;
    char *kp;
  INIT:
    if (!SvOK(k)) XSRETURN_NO;
  CODE:
    kp = SvPV(k, klen);
    RETVAL = mcdb_find(&this->m, kp, klen);
  OUTPUT:
    RETVAL

SV *
mcdbxs_find(this, k)
    struct mcdbxs_read * this;
    SV * k;
  PREINIT:
    STRLEN klen;
    char *kp;
  INIT:
    if (!SvOK(k)) XSRETURN_UNDEF;
  CODE:
    kp = SvPV(k, klen);
    if (mcdb_find(&this->m, kp, klen))
        RETVAL = mcdbxs_svcp(mcdb_dataptr(&this->m), mcdb_datalen(&this->m));
    else
        XSRETURN_UNDEF;
  OUTPUT:
    RETVAL

SV *
mcdbxs_FETCH(this, k)
    struct mcdbxs_read * this;
    SV * k;
  PREINIT:
    STRLEN klen;
    char *kp;
  INIT:
    if (!SvOK(k)) XSRETURN_UNDEF;
  CODE:
    kp = SvPV(k, klen);
    if (mcdbxs_is_iterkey(&this->iter, kp, klen)) {
        RETVAL = mcdbxs_svcp(mcdb_iter_dataptr(&this->iter),
                             mcdb_iter_datalen(&this->iter));
        if (this->values && !mcdb_iter(&this->iter)) {
            this->values = false; this->iter.ptr = this->iter.eod = NULL;
        }
    }
    else if (mcdb_find(&this->m, kp, klen)) {
        RETVAL = mcdbxs_svcp(mcdb_dataptr(&this->m), mcdb_datalen(&this->m));
        /* messiness required to detect Perl calling FETCH for values()
         * after having obtained all FIRSTKEY/NEXTKEY; needed to support
         * (duplicated) keys with multiple values, permitted in mcdb */
        if (this->iter.eod && MCDB_HEADER_SZ == mcdb_datapos(&this->m)-klen-8) {
            this->values = true;
            mcdb_iter_init(&this->iter, &this->m);
            if (!mcdb_iter(&this->iter) || !mcdb_iter(&this->iter)) {
                this->values = false; this->iter.ptr = this->iter.eod = NULL;
            }
        }
    }
    else
        XSRETURN_UNDEF;
    OUTPUT:
    RETVAL

SV *
mcdbxs_FIRSTKEY(this)
    struct mcdbxs_read * this;
  CODE:
    this->iter.ptr = NULL;      /* should already be NULL */
    RETVAL = mcdbxs_nextkey(this);
    if (!RETVAL) XSRETURN_UNDEF;/* empty database */
  OUTPUT:
    RETVAL

SV *
mcdbxs_NEXTKEY(this, k)
    struct mcdbxs_read * this;
    SV * k;
  INIT:
    if (!SvOK(k)) XSRETURN_UNDEF;
  CODE:
    RETVAL = mcdbxs_nextkey(this);
    if (!RETVAL) XSRETURN_UNDEF;
  OUTPUT:
    RETVAL

void
mcdbxs_DESTROY(sv)
    SV * sv;
  PREINIT:
    struct mcdbxs_read *this;
  CODE:
    if (sv_isobject(sv) && SvTYPE(SvRV(sv)) == SVt_PVMG) {
        this = (struct mcdbxs_read *)SvIV(SvRV(sv));
        mcdb_mmap_destroy(this->m.map);
        Safefree(this);
    }

AV *
mcdbxs_multi_get(this, k)
    struct mcdbxs_read * this;
    SV * k;
  PREINIT:
    STRLEN klen;
    char *kp;
  INIT:
    if (!SvOK(k)) XSRETURN_UNDEF;
  CODE:
    kp = SvPV(k, klen);
    RETVAL = newAV();   /* might be redundant (see .c generated from .xs) */
    sv_2mortal((SV *)RETVAL);
    if (mcdb_findstart(&this->m, kp, klen))
        while (mcdb_findnext(&this->m, kp, klen))
            av_push(RETVAL,
                    mcdbxs_svcp(mcdb_dataptr(&this->m),mcdb_datalen(&this->m)));
  OUTPUT:
    RETVAL


MODULE = MCDB_File	PACKAGE = MCDB_File::Make	PREFIX = mcdbxs_make_

# /*(MCDB_FILE::Make::insert instead of STORE to support multi-insert)*/
# /*(mcdb supports multiple records with same key, so a STORE method might be
#  * misleading if caller incorrectly assumes value replaced for repeated key)*/
void
mcdbxs_make_insert(mk, ...)
    struct mcdb_make * mk;
  PREINIT:
    SV *k, *v;
    char *kp, *vp;
    STRLEN klen, vlen;
    int x;
  PPCODE:
    for (x = 1; x+1 < items; x += 2) {
        k = ST(x); v = ST(x+1);
        if (SvOK(k) && SvOK(v)) {
            kp = SvPV(k, klen); vp = SvPV(v, vlen);
            if (mcdb_make_add(mk, kp, klen, vp, vlen) != 0)
                croak("MCDB_File::Make::insert: %s", Strerror(errno));
        }
        else
            croak("MCDB_File::Make::insert: invalid argument");
    }

struct mcdb_make *
mcdbxs_make_new(CLASS, fname, ...)
    char * CLASS;
    char * fname;
  PREINIT:
    struct mcdb_make *mk;
  CODE:
    RETVAL = Newx(mk, 1, struct mcdb_make);
    if (mcdb_makefn_start(mk, fname, mcdbxs_malloc, mcdbxs_free) == 0
        && mcdb_make_start(mk, mk->fd, mcdbxs_malloc, mcdbxs_free) == 0) {
        if (items >= 3)
            mk->st_mode = SvIV(ST(2)); /* optional mcdb perm mode */
    }
    else {
        mcdbxs_make_destroy(mk);
        XSRETURN_UNDEF;
    }
  OUTPUT:
    RETVAL

void
mcdbxs_make_DESTROY(sv)
    SV * sv;
  CODE:
    if (sv_isobject(sv) && SvTYPE(SvRV(sv)) == SVt_PVMG)
        mcdbxs_make_destroy((struct mcdb_make *)SvIV(SvRV(sv)));

void
mcdbxs_make_finish(mk, ...)
    struct mcdb_make * mk;
  PREINIT:
    bool do_fsync = true;
  CODE:
    if (items >= 2)
        do_fsync = SvIV(ST(1)) != 0; /* optional control fsync */
    if (mcdb_make_finish(mk) != 0 || mcdb_makefn_finish(mk, do_fsync) != 0) {
        /*mcdb_make_destroy(mk);*//* already called in mcdb_make_finish() */
        mcdb_makefn_cleanup(mk);
        croak("MCDB_File::Make::finish: %s", Strerror(errno));
    }
