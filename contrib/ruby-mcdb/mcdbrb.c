/*
 * ruby-mcdb - Ruby interface to create and read mcdb constant databases
 *
 * Copyright (c) 2011, Glue Logic LLC. All rights reserved. code()gluelogic.com
 *
 *  This file is part of ruby-mcdb.
 *
 *  ruby-mcdb is free software: you can redistribute it and/or modify it
 *  under the terms of the GNU Lesser General Public License as published
 *  by the Free Software Foundation, either version 2.1 of the License, or
 *  (at your option) any later version.
 *
 *  ruby-mcdb is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with ruby-mcdb.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * mcdb is originally based upon the Public Domain cdb-0.75 by Dan Bernstein
 *
 *
 * The Ruby online doc was instrumental to me while writing this extension.
 * Thank you!
 *   http://www.ruby-doc.org/docs/ProgrammingRuby/html/index.html
 *   http://www.ruby-doc.org/docs/ProgrammingRuby/html/ext_ruby.html
 *   http://rubycentral.com/pickaxe/builtins.html
 */

#include <ruby.h>

#ifndef RSTRING_LEN
#define RSTRING_LEN(x) RSTRING(x)->len
#endif
#ifndef RSTRING_PTR
#define RSTRING_PTR(x) RSTRING(x)->ptr
#endif

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include <mcdb/mcdb.h>
#include <mcdb/mcdb_make.h>
#include <mcdb/mcdb_makefn.h>

static ID mcdbrb_id_each;

/* option: use rb_tainted_str_new() instead of rb_str_new() for mcdb strings? */
#define mcdbrb_str_new(p,len) rb_tainted_str_new((const char *)(p), (len))

/* efficiency for common case; short-cut argument processing in common case
 * (throws exception if wrong type (unlikely) or if mcdb is closed)
 * (struct mcdb, mcdb_make both have 'map' struct element that can be tested)
 */
#define mcdbrb_Data_Cast_Struct(obj,type) \
  (   __builtin_expect( (!SPECIAL_CONST_P(obj)), 1) \
   && __builtin_expect( (BUILTIN_TYPE(obj) == T_DATA), 1) \
   && __builtin_expect(((void *)((type *)DATA_PTR(obj))->map != MAP_FAILED), 1)\
   ? (type *)DATA_PTR(obj) \
   : (type *)mcdbrb_raise_error(obj))

#define mcdbrb_require_T_STRING(obj) \
  if (   __builtin_expect( (!SPECIAL_CONST_P(obj)), 1) \
      && __builtin_expect( (BUILTIN_TYPE(obj) != T_STRING), 0)) \
      Check_Type((obj), T_STRING)

#define mcdbrb_convert_T_STRING(obj) \
  if (   __builtin_expect( (!SPECIAL_CONST_P(obj)), 1) \
      && __builtin_expect( (BUILTIN_TYPE(obj) != T_STRING), 0)) \
      StringValue(obj)

#define mcdbrb_check_open(m) \
  if (__builtin_expect( ((void *)m->map == MAP_FAILED), 0)) \
      rb_raise(rb_eRuntimeError, "closed MCDB file");

__attribute_cold__
__attribute_noinline__
static void *
mcdbrb_raise_error(const VALUE obj)
{  /*(one of these should raise an exception if this routine is called)*/
   Check_Type(obj, T_DATA);
   rb_raise(rb_eRuntimeError, "closed MCDB file");
   return NULL;
}

/*
 * class MCDB
 */

static VALUE
mcdbrb_has_key (const VALUE obj, VALUE key)
{
    struct mcdb * const restrict m = mcdbrb_Data_Cast_Struct(obj, struct mcdb);
    mcdbrb_convert_T_STRING(key);
    return mcdb_find(m, RSTRING_PTR(key), (uint32_t)RSTRING_LEN(key))
      ? Qtrue
      : Qfalse;
}

static VALUE
mcdbrb_get (const int argc, VALUE * const restrict argv, const VALUE obj)
{
    struct mcdb * const restrict m = mcdbrb_Data_Cast_Struct(obj, struct mcdb);
    const char *key; uint32_t klen;
    VALUE v_key, ifnone;
    if (argc != 1 && argc != 2)
        rb_scan_args(argc, argv, "11", &v_key, &ifnone);
    mcdbrb_convert_T_STRING(argv[0]);
    key  = RSTRING_PTR(argv[0]);
    klen = (uint32_t)RSTRING_LEN(argv[0]);
    if (mcdb_find(m, key, klen))
        return mcdbrb_str_new(mcdb_dataptr(m), mcdb_datalen(m));
    else if (rb_block_given_p())
        return rb_yield(argv[0]);/*key not found; call provided block with key*/
    else if (argc == 1)
      #ifdef RUBY_1_8_x   /*(see ruby-mcdb/extconf.rb)*/
        rb_raise(rb_eIndexError, "key not found");
      #else
        rb_raise(rb_eKeyError, "key not found");
      #endif
    return argv[1]; /* ifnone */
}

static VALUE
mcdbrb_findkey (const VALUE obj, VALUE v_key)
{
    /* mcdb 'find' is the traditional name, but Ruby Enumerable provides
     * a 'find' with different functionality (and different arguments),
     * so the interface to Ruby for the traditional mcdb find is 'findkey' */
    struct mcdb * const restrict m = mcdbrb_Data_Cast_Struct(obj, struct mcdb);
    const char *key; uint32_t klen;
    mcdbrb_convert_T_STRING(v_key);
    key  = RSTRING_PTR(v_key);
    klen = (uint32_t)RSTRING_LEN(v_key);
    return mcdb_find(m, key, klen)
      ? mcdbrb_str_new(mcdb_dataptr(m), mcdb_datalen(m))
      : Qnil;
}

static VALUE
mcdbrb_findnext (const VALUE obj)
{
    struct mcdb * const restrict m = mcdbrb_Data_Cast_Struct(obj, struct mcdb);
    mcdbrb_check_open(m);
    return m->loop != 0
      ? mcdb_findnext(m, (const char *)mcdb_keyptr(m), mcdb_keylen(m))
          ? mcdbrb_str_new(mcdb_dataptr(m), mcdb_datalen(m))
          : Qnil
      : (rb_raise(rb_eArgError,
                  "findnext() called without first calling findkey()"), Qnil);
}

static VALUE
mcdbrb_findall (const VALUE obj, VALUE v_key)
{
    struct mcdb * const restrict m = mcdbrb_Data_Cast_Struct(obj, struct mcdb);
    const char *key; uint32_t klen;
    const VALUE ary = rb_ary_new();
    mcdbrb_convert_T_STRING(v_key);
    key  = RSTRING_PTR(v_key);
    klen = (uint32_t)RSTRING_LEN(v_key);
    if (mcdb_findstart(m, key, klen)) {
        while (mcdb_findnext(m, key, klen))
            rb_ary_push(ary, mcdbrb_str_new(mcdb_dataptr(m), mcdb_datalen(m)));
    }
    return ary;
}

static VALUE
mcdbrb_getseq (const int argc, VALUE * const restrict argv, const VALUE obj)
{
    const char *key; uint32_t klen; int seq = 0; bool r = false;
    struct mcdb * const restrict m = mcdbrb_Data_Cast_Struct(obj, struct mcdb);
    VALUE v_key, v_seq;
    if (argc == 1)
        seq = 0;
    else if (argc == 2)
        seq = RTEST(argv[1]) ? NUM2INT(argv[1]) : 0;
    else
        rb_scan_args(argc, argv, "11", &v_key, &v_seq);
    mcdbrb_convert_T_STRING(argv[0]);
    key  = RSTRING_PTR(argv[0]);
    klen = (uint32_t)RSTRING_LEN(argv[0]);
    if (mcdb_findstart(m, key, klen)) {
        while ((r = mcdb_findnext(m, key, klen)) && seq--)
            ;
    }
    return r
      ? mcdbrb_str_new(mcdb_dataptr(m), mcdb_datalen(m))
      : Qnil;
}

static VALUE
mcdbrb_empty_p (const VALUE obj)
{
    struct mcdb * const restrict m = mcdbrb_Data_Cast_Struct(obj, struct mcdb);
    struct mcdb_iter iter;
    mcdb_iter_init(&iter, m);
    return !mcdb_iter(&iter) ? Qtrue : Qfalse;
}

static VALUE
mcdbrb_size (const VALUE obj)
{
    struct mcdb * const restrict m = mcdbrb_Data_Cast_Struct(obj, struct mcdb);
    return UINT2NUM(mcdb_numrecs(m));
}

static VALUE
mcdbrb_count_i (const VALUE y, const VALUE v_count,
                const int argc, const VALUE * const restrict argv)
{
  #ifdef RUBY_1_8_x   /*(see ruby-mcdb/extconf.rb)*/
    if (RTEST(rb_yield(y)))
        ++*(VALUE *)v_count;
  #else
    if (RTEST(rb_yield_values2(argc, argv)))
        ++*(VALUE *)v_count;
  #endif
    return Qnil;
}

static VALUE
mcdbrb_count (const int argc, VALUE * const restrict argv, const VALUE obj)
{
    VALUE v_count = 0;
    if (argc != 0 && argc != 1)
        rb_scan_args(argc, argv, "01", &v_count);
    if (rb_block_given_p()) {
        rb_block_call(obj,mcdbrb_id_each,0,0,mcdbrb_count_i,(VALUE)&v_count);
        return UINT2NUM((uint32_t)v_count);
    }
    else if (argc == 1) {
        struct mcdb * const restrict m=mcdbrb_Data_Cast_Struct(obj,struct mcdb);
        const char *key; uint32_t klen, count = 0;
        mcdbrb_convert_T_STRING(argv[0]);
        key  = RSTRING_PTR(argv[0]);
        klen = (uint32_t)RSTRING_LEN(argv[0]);
        if (mcdb_findstart(m, key, klen)) {
            while (mcdb_findnext(m, key, klen))
                ++count;
        }
        return UINT2NUM(count);
    }
    else
        return mcdbrb_size(obj);
}

static VALUE
mcdbrb_each_pair (const VALUE obj)
{
    struct mcdb * const m = mcdbrb_Data_Cast_Struct(obj, struct mcdb);
    struct mcdb_iter iter;
    mcdb_iter_init(&iter, m);
    RETURN_ENUMERATOR(obj, 0, 0);
    mcdbrb_check_open(m);     /* m must not be 'restrict' ptr */
    while (mcdb_iter(&iter)) {
        rb_yield(rb_assoc_new(mcdbrb_str_new(mcdb_iter_keyptr(&iter),
                                             mcdb_iter_keylen(&iter)),
                              mcdbrb_str_new(mcdb_iter_dataptr(&iter),
                                             mcdb_iter_datalen(&iter))));
        mcdbrb_check_open(m); /* m must not be 'restrict' ptr */
    }
    return Qnil;
}

static VALUE
mcdbrb_each_key (const VALUE obj)
{
    struct mcdb * const m = mcdbrb_Data_Cast_Struct(obj, struct mcdb);
    struct mcdb_iter iter;
    mcdb_iter_init(&iter, m);
    RETURN_ENUMERATOR(obj, 0, 0);
    mcdbrb_check_open(m);     /* m must not be 'restrict' ptr */
    while (mcdb_iter(&iter)) {
        rb_yield(mcdbrb_str_new(mcdb_iter_keyptr(&iter),
                                mcdb_iter_keylen(&iter)));
        mcdbrb_check_open(m); /* m must not be 'restrict' ptr */
    }
    return Qnil;
}

static VALUE
mcdbrb_each_value (const VALUE obj)
{
    struct mcdb * const m = mcdbrb_Data_Cast_Struct(obj, struct mcdb);
    struct mcdb_iter iter;
    mcdb_iter_init(&iter, m);
    RETURN_ENUMERATOR(obj, 0, 0);
    mcdbrb_check_open(m);     /* m must not be 'restrict' ptr */
    while (mcdb_iter(&iter)) {
        rb_yield(mcdbrb_str_new(mcdb_iter_dataptr(&iter),
                                mcdb_iter_datalen(&iter)));
        mcdbrb_check_open(m); /* m must not be 'restrict' ptr */
    }
    return Qnil;
}

static VALUE
mcdbrb_each (const int argc, VALUE * const argv, const VALUE obj)
{
    if (argc == 0)
        return mcdbrb_each_pair(obj);
    else if (argc == 1) {
        struct mcdb * const m = mcdbrb_Data_Cast_Struct(obj, struct mcdb);
        char *key; uint32_t klen;
        mcdbrb_convert_T_STRING(argv[0]);
        key  = RSTRING_PTR(argv[0]);
        klen = (uint32_t)RSTRING_LEN(argv[0]);
        RETURN_ENUMERATOR(obj, argc, argv); /*(not sure if argc,argv needed)*/
        mcdbrb_check_open(m);         /* m must not be 'restrict' ptr */
        if (mcdb_findstart(m, key, klen)) {
            while (mcdb_findnext(m, key, klen)) {
                rb_yield(mcdbrb_str_new(mcdb_dataptr(m), mcdb_datalen(m)));
                mcdbrb_check_open(m); /* m must not be 'restrict' ptr */
            }
        }
        return Qnil;
    }
    else {
        VALUE v_key;
        rb_scan_args(argc, argv, "01", &v_key);
        return Qnil;
    }
}

static VALUE
mcdbrb_values_at (const int argc, VALUE * const restrict argv, const VALUE obj)
{
    struct mcdb * const restrict m = mcdbrb_Data_Cast_Struct(obj, struct mcdb);
    char *key; uint32_t klen;
    VALUE v;
    const VALUE ary = rb_ary_new2(argc);
    int i;
    for (i = 0; i < argc; ++i) {
        v = argv[i];
        mcdbrb_convert_T_STRING(v);
        key  = RSTRING_PTR(v);
        klen = (uint32_t)RSTRING_LEN(v);
        if (mcdb_findstart(m, key, klen)) {
            while (mcdb_findnext(m, key, klen))
                rb_ary_push(ary, mcdbrb_str_new(mcdb_dataptr(m),
                                                mcdb_datalen(m)));
        }
    }
    return ary;
}

static VALUE
mcdbrb_select_i (const VALUE obj, const bool match)
{
    struct mcdb * const m = mcdbrb_Data_Cast_Struct(obj, struct mcdb);
    const VALUE ary = rb_ary_new();
    VALUE assoc, v;
    struct mcdb_iter iter;
    mcdb_iter_init(&iter, m);
    RETURN_ENUMERATOR(obj, 0, 0);
    mcdbrb_check_open(m);     /* m must not be 'restrict' ptr */
    while (mcdb_iter(&iter)) {
        assoc = rb_assoc_new(mcdbrb_str_new(mcdb_iter_keyptr(&iter),
                                            mcdb_iter_keylen(&iter)),
                             mcdbrb_str_new(mcdb_iter_dataptr(&iter),
                                            mcdb_iter_datalen(&iter)));
        v = rb_yield(assoc);
        if (RTEST(v) == match)
            rb_ary_push(ary, assoc);
        mcdbrb_check_open(m); /* m must not be 'restrict' ptr */
    }
    return ary;
}

static VALUE
mcdbrb_select (const VALUE obj)
{
    return mcdbrb_select_i(obj, true);
}

static VALUE
mcdbrb_reject (const VALUE obj)
{
    return mcdbrb_select_i(obj, false);
}

static VALUE
mcdbrb_to_a (const VALUE obj)
{
    struct mcdb * const restrict m = mcdbrb_Data_Cast_Struct(obj, struct mcdb);
    const VALUE ary = rb_ary_new2(mcdb_numrecs(m));
    struct mcdb_iter iter;
    mcdb_iter_init(&iter, m);
    while (mcdb_iter(&iter))
        rb_ary_push(ary,
                    rb_assoc_new(mcdbrb_str_new(mcdb_iter_keyptr(&iter),
                                                mcdb_iter_keylen(&iter)),
                                 mcdbrb_str_new(mcdb_iter_dataptr(&iter),
                                                mcdb_iter_datalen(&iter))));
    return ary;
}

static VALUE
mcdbrb_to_hash (const VALUE obj)
{
    struct mcdb * const restrict m = mcdbrb_Data_Cast_Struct(obj, struct mcdb);
    const VALUE hash = rb_hash_new();
    struct mcdb_iter iter;
    mcdb_iter_init(&iter, m);
    /* Note: ignores multi-value keys; last value seen wins for any given key */
    while (mcdb_iter(&iter))
        rb_hash_aset(hash, mcdbrb_str_new(mcdb_iter_keyptr(&iter),
                                          mcdb_iter_keylen(&iter)),
                           mcdbrb_str_new(mcdb_iter_dataptr(&iter),
                                          mcdb_iter_datalen(&iter)));
    return hash;
}

static VALUE
mcdbrb_invert (const VALUE obj)
{
    struct mcdb * const restrict m = mcdbrb_Data_Cast_Struct(obj, struct mcdb);
    const VALUE hash = rb_hash_new();
    struct mcdb_iter iter;
    mcdb_iter_init(&iter, m);
    /* Note: ignores multi-key values; last key seen wins for any given value */
    while (mcdb_iter(&iter))
        rb_hash_aset(hash, mcdbrb_str_new(mcdb_iter_dataptr(&iter),
                                          mcdb_iter_datalen(&iter)),
                           mcdbrb_str_new(mcdb_iter_keyptr(&iter),
                                          mcdb_iter_keylen(&iter)));
    return hash;
}

static VALUE
mcdbrb_index (const VALUE obj, VALUE v_data)
{
    struct mcdb * const restrict m = mcdbrb_Data_Cast_Struct(obj, struct mcdb);
    const char *data; uint32_t dlen;
    struct mcdb_iter iter;
    mcdbrb_convert_T_STRING(v_data);
    data = RSTRING_PTR(v_data);
    dlen = (uint32_t)RSTRING_LEN(v_data);
    mcdb_iter_init(&iter, m);
    while (mcdb_iter(&iter)) {
        if (mcdb_iter_datalen(&iter) == dlen
            && memcmp(mcdb_iter_dataptr(&iter), data, dlen) == 0)
            return mcdbrb_str_new(mcdb_iter_keyptr(&iter),
                                  mcdb_iter_keylen(&iter));
    }
    return Qnil;
}

static VALUE
mcdbrb_keys (const VALUE obj)
{
    struct mcdb * const restrict m = mcdbrb_Data_Cast_Struct(obj, struct mcdb);
    const VALUE ary = rb_ary_new2(mcdb_numrecs(m));
    struct mcdb_iter iter;
    mcdb_iter_init(&iter, m);
    while (mcdb_iter(&iter))
        rb_ary_push(ary, mcdbrb_str_new(mcdb_iter_keyptr(&iter),
                                        mcdb_iter_keylen(&iter)));
    return ary;
}

static VALUE
mcdbrb_values (const VALUE obj)
{
    struct mcdb * const restrict m = mcdbrb_Data_Cast_Struct(obj, struct mcdb);
    const VALUE ary = rb_ary_new2(mcdb_numrecs(m));
    struct mcdb_iter iter;
    mcdb_iter_init(&iter, m);
    while (mcdb_iter(&iter))
        rb_ary_push(ary, mcdbrb_str_new(mcdb_iter_dataptr(&iter),
                                        mcdb_iter_datalen(&iter)));
    return ary;
}

static VALUE
mcdbrb_has_value (const VALUE obj, VALUE value)
{
    struct mcdb * const restrict m = mcdbrb_Data_Cast_Struct(obj, struct mcdb);
    struct mcdb_iter iter;
    char *vptr;
    uint32_t vlen;
    mcdbrb_convert_T_STRING(value);
    vptr = RSTRING_PTR(value);
    vlen = (uint32_t)RSTRING_LEN(value);
    mcdb_iter_init(&iter, m);
    while (mcdb_iter(&iter))
        if (vlen == mcdb_iter_datalen(&iter)
            && memcmp(mcdb_iter_dataptr(&iter), vptr, vlen) == 0)
            return Qtrue;
    return Qfalse;
}

static VALUE
mcdbrb_close (const VALUE obj)
{
    struct mcdb * const restrict m = mcdbrb_Data_Cast_Struct(obj, struct mcdb);
    if (m->map && (void *)m->map != MAP_FAILED) {
        mcdb_mmap_destroy(m->map);
        m->map = (struct mcdb_mmap *)MAP_FAILED;
    }
    return Qnil;
}

static void
mcdbrb_dtor (struct mcdb * const restrict m)
{
    if (m->map && (void *)m->map != MAP_FAILED)
        mcdb_mmap_destroy(m->map);
    xfree(m);
}

static VALUE
mcdbrb_open (const VALUE klass, VALUE fname)
{
    struct mcdb * restrict m;
    char *fnptr;
    mcdbrb_convert_T_STRING(fname);
    if ((fnptr = RSTRING_PTR(fname))[RSTRING_LEN(fname)] != '\0') {
        StringValueCStr(fname);
        fnptr = RSTRING_PTR(fname);
    }
    m = xmalloc(sizeof(struct mcdb));
    if (m == NULL) rb_sys_fail(0);
    m->loop = 0;
    m->map = mcdb_mmap_create(NULL,NULL,fnptr,(void *(*)(size_t))xmalloc,xfree);
    if (m->map != NULL) {
        const VALUE v = Data_Wrap_Struct(klass, 0, mcdbrb_dtor, m);
        return rb_block_given_p()
          ? rb_ensure(rb_yield, v, mcdbrb_close, v)
          : v;
    }
    else {
        mcdbrb_dtor(m);
        rb_sys_fail(0);
        return Qnil;
    }
}


/*
 * class MCDBmake 
 */

static VALUE
mcdbrb_make_add (const VALUE obj, VALUE key, VALUE data)
{
    struct mcdb_make * const restrict mk =
      mcdbrb_Data_Cast_Struct(obj, struct mcdb_make);
    mcdbrb_convert_T_STRING(key);
    mcdbrb_convert_T_STRING(data);
    return mcdb_make_add(mk, RSTRING_PTR(key),  RSTRING_LEN(key),
                             RSTRING_PTR(data), RSTRING_LEN(data)) != -1
      ? Qnil
      : (rb_sys_fail(0), Qnil);
}

static VALUE
mcdbrb_make_finish (const int argc, const VALUE * const restrict argv,
                    const VALUE obj)
{
    struct mcdb_make * const restrict mk =
      mcdbrb_Data_Cast_Struct(obj, struct mcdb_make);
    VALUE v_fsync;
    bool do_fsync = true;
    if (argc == 0)
        do_fsync = true;
    else if (argc == 1)
        do_fsync = RTEST(argv[0]);
    else
        rb_scan_args(argc, argv, "01", &v_fsync);
    return (0 == mcdb_make_finish(mk) && 0 == mcdb_makefn_finish(mk, do_fsync))
      ? Qnil
      : (rb_sys_fail(0), Qnil);
}

static VALUE
mcdbrb_make_cancel (const VALUE obj)
{
    /* mcdbrb_Data_Cast_Struct(), but without exception if mcdb already closed*/
    struct mcdb_make * restrict mk;
    Check_Type(obj, T_DATA);
    mk = (struct mcdb_make *)DATA_PTR(obj);
    mcdb_make_destroy(mk);
    mcdb_makefn_cleanup(mk);
    return Qnil;
}

static void
mcdbrb_make_dtor (struct mcdb_make * const restrict mk)
{
    mcdb_make_destroy(mk);
    mcdb_makefn_cleanup(mk);
    xfree(mk);
}

static VALUE
mcdbrb_make_open (const int argc, VALUE * const restrict argv,
                  const VALUE klass)
{
    struct mcdb_make * restrict mk;
    char *fnptr;
    int st_mode = ~0;
    VALUE fname, v_mode;
    if (argc == 1)
        st_mode = ~0;
    else if (argc == 2)
        st_mode = RTEST(argv[1]) ? NUM2INT(argv[1]) : ~0;
    else
        rb_scan_args(argc, argv, "11", &fname, &v_mode);
    fname = argv[0];
    /*mcdbrb_convert_T_STRING(fname);*/
    SafeStringValue(fname); /* check taint before open()ing for writing */
    if ((fnptr = RSTRING_PTR(fname))[RSTRING_LEN(fname)] != '\0') {
        StringValueCStr(fname);
        fnptr = RSTRING_PTR(fname);
    }

    mk = xmalloc(sizeof(struct mcdb_make));
    if (mk == NULL) rb_sys_fail(0);

    if (mcdb_makefn_start(mk, fnptr, (void *(*)(size_t))xmalloc, xfree) == 0
        && mcdb_make_start(mk, mk->fd, (void *(*)(size_t))xmalloc, xfree) == 0){
        const VALUE v = Data_Wrap_Struct(klass, 0, mcdbrb_make_dtor, mk);
        if (st_mode != ~0)
            mk->st_mode = (mode_t)st_mode;
        return rb_block_given_p()
          ? rb_rescue(rb_yield, v, mcdbrb_make_cancel, v)
          : v;
    }
    else {
        mcdbrb_make_dtor(mk);
        rb_sys_fail(0);
        return Qnil;
    }
}

/*
 * Module Initialization
 */

void
Init_mcdb (void)
{
    const VALUE rb_cMCDB     = rb_define_class("MCDB",     rb_cObject);
    const VALUE rb_cMCDBmake = rb_define_class("MCDBmake", rb_cObject);

    /* http://www.ruby-doc.org/core/classes/Enumerable.html */
    /* http://rubycentral.com/pickaxe/ref_m_enumerable.html */
    rb_include_module(rb_cMCDB, rb_mEnumerable);

    /* mcdb methods inherit from Enumerable (above)
     * and also provide some Hash and dbm methods (e.g. from dbm, gdbm) */
    /* http://www.ruby-doc.org/core/classes/Hash.html */
    /* http://rubycentral.com/pickaxe/ref_c_hash.html */

    rb_define_singleton_method(rb_cMCDB, "open", mcdbrb_open, 1);
    rb_define_method(rb_cMCDB, "[]",         mcdbrb_findkey,     1);
    rb_define_method(rb_cMCDB, "findkey",    mcdbrb_findkey,     1);
    rb_define_method(rb_cMCDB, "findnext",   mcdbrb_findnext,    0);
    rb_define_method(rb_cMCDB, "findall",    mcdbrb_findall,     1);
    rb_define_method(rb_cMCDB, "fetch",      mcdbrb_get,        -1);
    rb_define_method(rb_cMCDB, "get",        mcdbrb_get,        -1);
    rb_define_method(rb_cMCDB, "getseq",     mcdbrb_getseq,     -1);
    rb_define_method(rb_cMCDB, "key?",       mcdbrb_has_key,     1);
    rb_define_method(rb_cMCDB, "has_key?",   mcdbrb_has_key,     1);
    rb_define_method(rb_cMCDB, "include?",   mcdbrb_has_key,     1);
    rb_define_method(rb_cMCDB, "member?",    mcdbrb_has_key,     1);
    rb_define_method(rb_cMCDB, "value?",     mcdbrb_has_value,   1);
    rb_define_method(rb_cMCDB, "has_value?", mcdbrb_has_value,   1);
    rb_define_method(rb_cMCDB, "empty?",     mcdbrb_empty_p,     0);
    rb_define_method(rb_cMCDB, "size",       mcdbrb_size,        0);
    rb_define_method(rb_cMCDB, "length",     mcdbrb_size,        0);
    rb_define_method(rb_cMCDB, "count",      mcdbrb_count,      -1);
    rb_define_method(rb_cMCDB, "each",       mcdbrb_each,       -1);
    rb_define_method(rb_cMCDB, "each_entry", mcdbrb_each_pair,   0);
    rb_define_method(rb_cMCDB, "each_pair",  mcdbrb_each_pair,   0);
    rb_define_method(rb_cMCDB, "each_key",   mcdbrb_each_key,    0);
    rb_define_method(rb_cMCDB, "each_value", mcdbrb_each_value,  0);
    rb_define_method(rb_cMCDB, "values_at",  mcdbrb_values_at,  -1);
    rb_define_method(rb_cMCDB, "select",     mcdbrb_select,      0);
    rb_define_method(rb_cMCDB, "reject",     mcdbrb_reject,      0);
    rb_define_method(rb_cMCDB, "invert",     mcdbrb_invert,      0);
    rb_define_method(rb_cMCDB, "to_hash",    mcdbrb_to_hash,     0);
    rb_define_method(rb_cMCDB, "to_a",       mcdbrb_to_a,        0);
    rb_define_method(rb_cMCDB, "entries",    mcdbrb_to_a,        0);
    rb_define_method(rb_cMCDB, "pairs",      mcdbrb_to_a,        0);
    rb_define_method(rb_cMCDB, "keys",       mcdbrb_keys,        0);
    rb_define_method(rb_cMCDB, "values",     mcdbrb_values,      0);
    rb_define_method(rb_cMCDB, "index",      mcdbrb_index,       1);
    rb_define_method(rb_cMCDB, "close",      mcdbrb_close,       0);

    /* Note: use 'index' method to iterate to find key to given value
     * (not providing dbm 'key' method to do same; 'key' too close to 'key?')
     * Similarly, use 'select' instead of 'find_all' */
    /* Note: not providing alloc_func (rb_define_alloc_func) or "initialize"
     * I will do so if someone explains to me how they would be more useful
     * to mcdb than the mcdb "open" method. */

    rb_define_singleton_method(rb_cMCDBmake, "open", mcdbrb_make_open, -1);
    rb_define_method(rb_cMCDBmake, "[]=",    mcdbrb_make_add,    2);
    rb_define_method(rb_cMCDBmake, "add",    mcdbrb_make_add,    2);
    rb_define_method(rb_cMCDBmake, "close",  mcdbrb_make_finish,-1);
    rb_define_method(rb_cMCDBmake, "finish", mcdbrb_make_finish,-1);
    rb_define_method(rb_cMCDBmake, "cancel", mcdbrb_make_cancel, 0);

    mcdbrb_id_each = rb_intern("each");
}
