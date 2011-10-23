/*
 * python-mcdb - Python interface to create and read mcdb constant databases
 *
 * Copyright (c) 2011, Glue Logic LLC. All rights reserved. code()gluelogic.com
 *
 *  This file is part of python-mcdb.
 *
 *  python-mcdb is free software: you can redistribute it and/or modify it
 *  under the terms of the GNU Lesser General Public License as published
 *  by the Free Software Foundation, either version 2.1 of the License, or
 *  (at your option) any later version.
 *
 *  python-mcdb is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with python-mcdb.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * mcdb is originally based upon the Public Domain cdb-0.75 by Dan Bernstein
 *
 *
 * mcdbpy_PyObject_to_buf() modified from Python 2.7.2 library code, portions:
 *   Copyright (c) 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010
 *   Python Software Foundation; All Rights Reserved
 *
 *
 * The Python online doc was instrumental to me while writing this extension.
 * Thank you!
 *   http://docs.python.org/extending/
 *   http://docs.python.org/c-api/
 */

/*
 * FUTURE:
 *
 * - mcdb_mmap_create() and mcdb_make_start() currently pass
 *   PyMem_Malloc and PyMem_Free for use allocating memory.  If mcdb causes
 *   performance problems in other Python threads, might simply pass malloc
 *   and free, instead, and then Py_BEGIN_ALLOW_THREADS/Py_END_ALLOW_THREADS
 *   around calls to mcdb_mmap_create() and mcdb_make_finish().  The latter
 *   (mcdb_make_finish()) is likely to take time on very large mcdb creation.
 *   (If we pass malloc and free, then set PyErr_NoMemory() if errno == ENOMEM)
 *
 * - mcdb is constant, immutable data and so there exists the possibility
 *   of returning read-only PyObject objects that directly reference the mmap
 *   instead of copying every key and value.  PyBuffer_FromMemory() looked
 *   promising, but did not immediately stringify as expected everwhere.
 *   I got: [<read-only buffer ptr 0xb766b018, size 3 at 0xb76e0860>, ... ]
 *   only when *lists* were printed.  Otherwise, it appeared to work, so if
 *   this makes a big performance difference, it might be worthwhile trying.
 *   See #if below "choose PyBytes_FromStringAndSize() or PyBuffer_FromMemory()"
 #   where #define mcdbpy_PyObject_From_mcdb(p,sz) selects one or the other.
 *
 *   The newer Py_buffer interface, PyMemoryView_FromBuffer() does not provide
 *   tp_str method, so gives <memory at 0xb77ad89c> (appararently from repr()).
 *   Here is some sample code:
 *     static PyObject *
 *     mcdbpy_read_data(const struct mcdb * const restrict m)
 *     {
 *         // (init Py_buffer to 0 or NULL, set itemsize=readonly=1, format s)
 *         Py_buffer pyb = { 0, 0, 0, 1, 1, 0, "s", 0, 0, 0, {0,0}, 0 };
 *         pyb.buf = mcdb_dataptr(m);
 *         pyb.len = (Py_ssize_t)mcdb_datalen(m);
 *         return PyMemoryView_FromBuffer(&pyb);
 *     }
 *
 *   Since the desire is that the caller need not do anything special to get
 *   the data stringified, python-mcdb continues to copy keys, data from mcdb.
 *   Maybe one day it will be worthwhile to write a custom Py_buffer class.
 */

#include <Python.h>
#include <structmember.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef __GNUC__
#define restrict __restrict
#else
#define restrict
#endif

#include <mcdb/mcdb.h>
#include <mcdb/mcdb_make.h>
#include <mcdb/mcdb_makefn.h>
#include <mcdb/uint32.h>

#define VERSION "0.01"

/*(Py_RETURN_NONE, Py_RETURN_TRUE, Py_RETURN_FALSE but without 'return')*/
#define mcdbpy_Py_None()  (Py_INCREF(Py_None),  Py_None)
#define mcdbpy_Py_True()  (Py_INCREF(Py_True),  Py_True)
#define mcdbpy_Py_False() (Py_INCREF(Py_False), Py_False)

/* choose PyBytes_FromStringAndSize() or PyBuffer_FromMemory() */
#if 1
#define mcdbpy_PyObject_From_mcdb(p,sz) \
        PyBytes_FromStringAndSize((char *)(p), (Py_ssize_t)(sz))
#else
#define mcdbpy_PyObject_From_mcdb(p,sz) \
        PyBuffer_FromMemory((p), (Py_ssize_t)(sz))
#endif


/* special-purpose routine to parse single string from PyObject * for METH_O;
 * modified from Python/getargs.c static funcs convertsimple(),convertbuffer().
 * Intended to allow METH_O methods taking single PyObject * argument instead
 * of METH_VARARGS methods that call PyArg_ParseTuple(args, "s#", &key, &klen);
 * PyArg_ParseTuple() overhead can be more expensive than rest of routine!
 * This special-purpose routine reduces, but does not remove, the overhead.
 * Note: uint32_t *len (instead of Py_ssize_t) is specific to mcdbpy code since
 * mcdb keys and data are limited to INT_MAX-8 in size (and mcdb uses uint32_t).
 * Oversized values are truncated (but should not happen). */
static bool
mcdbpy_PyObject_to_buf (PyObject * restrict arg,
                        const char ** const restrict buf,
                        uint32_t * const restrict len)
{
    /* mcdbpy_PyObject_to_buf() is modified from Python-2.7.2/Python/getargs.c
     * and is licensed under Python Software Foundation License Version 2.  See
     * Python-2.7.2/LICENSE for license, Python Software Foundation copyright.
     * http://www.python.org/ */
    const char *errmsg = "must be string or single-segment read-only buffer";
    if (PyString_Check(arg)) {
        *buf = PyString_AS_STRING(arg);
        *len = (uint32_t)PyString_GET_SIZE(arg);
        return true;
    }
  #ifdef Py_USING_UNICODE
    else if (PyUnicode_Check(arg)) {
        if ((arg = _PyUnicode_AsDefaultEncodedString(arg, NULL)) != NULL) {
            *buf = PyString_AS_STRING(arg);
            *len = (uint32_t)PyString_GET_SIZE(arg);
            return true;
        }
        errmsg = "(unicode conversion error)";
    }
  #endif
    else if (PyBuffer_Check(arg)) {  /* any buffer-like object */
        PyBufferProcs * const pb = arg->ob_type->tp_as_buffer;
        Py_buffer pyb;
        if (pb && (!pb->bf_getsegcount || pb->bf_getsegcount(arg, NULL) == 1)
            && (   (pb->bf_getbuffer
                    && pb->bf_getbuffer(arg, &pyb, PyBUF_SIMPLE) == 0)
                || (pb->bf_getreadbuffer
                    && (pyb.len = pb->bf_getreadbuffer(arg,0,&pyb.buf)) >= 0))){
            *buf = pyb.buf;
            *len = (uint32_t)pyb.len;
            return true;
        }
    }

    PyErr_SetString(PyExc_TypeError, errmsg);
    return false;
}




struct mcdbpy {
    PyObject_HEAD
    PyObject *fname;              /* attribute: 'filename' */
    struct mcdb m;
    struct mcdbpy *origin;
    struct mcdb_iter iter;
    uint32_t itertype;
};

staticforward PyTypeObject mcdbpy_Type;

static inline PyObject *
mcdbpy_read_data(const struct mcdb * const restrict m)
{
    return mcdbpy_PyObject_From_mcdb(mcdb_dataptr(m), mcdb_datalen(m));
}

static Py_ssize_t
mcdbpy_numrecs(struct mcdbpy * const restrict self)
{   /*(cast ok because 2 gibibyte numrecs mcdb limit)*/
    return (Py_ssize_t)mcdb_numrecs(&self->m);
}

static PyObject *
mcdbpy_dict_contains(struct mcdbpy * const self, PyObject * const args)
{
    const char *key; uint32_t klen;
    return mcdbpy_PyObject_to_buf(args, &key, &klen)
      ? mcdb_find(&self->m, key, klen) ? mcdbpy_Py_True() : mcdbpy_Py_False()
      : NULL;
}

static PyObject *
mcdbpy_find(struct mcdbpy * const self, PyObject * const args)
{
    const char *key; uint32_t klen;
    return mcdbpy_PyObject_to_buf(args, &key, &klen)
      ? mcdb_find(&self->m, key, klen)
          ? mcdbpy_read_data(&self->m)
          : mcdbpy_Py_None()
      : NULL;
}

static PyObject *
mcdbpy_findnext(struct mcdbpy * const self)
{
    return self->m.loop != 0
      ? mcdb_findnext(&self->m, (const char *)mcdb_keyptr(&self->m),
                                              mcdb_keylen(&self->m))
          ? mcdbpy_read_data(&self->m)
          : mcdbpy_Py_None()
      : (PyErr_SetString(PyExc_TypeError, 
                         "findnext() called without first calling find()"),
         NULL);
}

static PyObject *
mcdbpy_findall(struct mcdbpy * const self, PyObject * const args)
{
    PyObject * restrict data;
    PyObject * restrict list = NULL;
    const char *key; uint32_t klen;
    if (mcdbpy_PyObject_to_buf(args, &key, &klen)
        && (list = PyList_New(0)) != NULL
        && mcdb_findstart(&self->m, key, klen)) {
        while (mcdb_findnext(&self->m, key, klen)) {
            if ((data = mcdbpy_read_data(&self->m))
                && PyList_Append(list,data) == 0)
                Py_DECREF(data);
            else {
                Py_XDECREF(data);
                Py_DECREF(list);
                return NULL;
            }
        }
    }
    return list;
}

static PyObject *
mcdbpy_getseq(struct mcdbpy * const self, PyObject * const args)
{
    const char *key; uint32_t klen; int seq = 0; bool r = false;
    if (!PyArg_ParseTuple(args, "s#|i:getseq", &key, &klen, &seq))
        return NULL;

    if (mcdb_findstart(&self->m, key, klen))
        while ((r = mcdb_findnext(&self->m, key, klen)) && seq--)
            ;

    return r
      ? mcdbpy_read_data(&self->m)
      : mcdbpy_Py_None();
}

static PyObject *
mcdbpy_dict_get(struct mcdbpy * const self, PyObject * const args)
{
    const char *key; uint32_t klen;
    PyObject *defaultpy = Py_None;
    return PyArg_ParseTuple(args, "s#|O:get", &key, &klen, &defaultpy)
      ? mcdb_find(&self->m, key, klen)
          ? mcdbpy_read_data(&self->m)
          : (Py_INCREF(defaultpy), defaultpy)
      : NULL;
}

/* mcdb[key]
 * (PyObject_GetItem() specifies set KeyError, return NULL if key not found) */
static PyObject *
mcdbpy_dict_getitem(struct mcdbpy * const self, PyObject * const args)
{
    const char *key; uint32_t klen;
    return mcdbpy_PyObject_to_buf(args, &key, &klen)
      ? mcdb_find(&self->m, key, (size_t)klen)
          ? mcdbpy_read_data(&self->m)
          : (PyErr_SetString(PyExc_KeyError, key), NULL)
      : NULL;
}

enum { MCDBPY_ITERKEYS = 0, MCDBPY_ITERVALUES, MCDBPY_ITERITEMS };

staticforward PyObject * mcdbpy_iter_new(struct mcdbpy * const restrict self);

static PyObject *
mcdbpy_dict_iter_init(struct mcdbpy * const restrict self, const int itertype)
{
    struct mcdbpy * restrict nself;
    /* prevent second-time (double) initialization
     *   for r in m.iteritems():
     *       ...
     *   for r in m.itervalues():
     *       ...
     *   for r in m.iterkeys():     ==> caller need only do:  for r in m:
     *       ...                                                  ....
     * (There is probably a better way to handle this.
     *  If you know what it is, please tell me.)
     */
    if (self->origin)
        return (PyObject *)self;

    nself = (struct mcdbpy *)mcdbpy_iter_new(self);
    if (nself) {
        mcdb_iter_init(&nself->iter, &nself->m);
        nself->itertype = itertype;
    }
    return (PyObject *)nself;
}

static PyObject *
mcdbpy_dict_iterkeys(struct mcdbpy * const restrict self)
{
    return mcdbpy_dict_iter_init(self, MCDBPY_ITERKEYS);
}

static PyObject *
mcdbpy_dict_itervalues(struct mcdbpy * const restrict self)
{
    return mcdbpy_dict_iter_init(self, MCDBPY_ITERVALUES);
}

static PyObject *
mcdbpy_dict_iteritems(struct mcdbpy * const restrict self)
{
    return mcdbpy_dict_iter_init(self, MCDBPY_ITERITEMS);
}

static PyObject *
mcdbpy_dict_iternext(struct mcdbpy * const self)
{
    struct mcdb_iter * const restrict iter = &self->iter;
    if (mcdb_iter(iter)) {
        if (self->itertype == MCDBPY_ITERKEYS) {
            return mcdbpy_PyObject_From_mcdb(mcdb_iter_keyptr(iter),
                                             mcdb_iter_keylen(iter));
        }
        else if (self->itertype == MCDBPY_ITERVALUES) {
            return mcdbpy_PyObject_From_mcdb(mcdb_iter_dataptr(iter),
                                             mcdb_iter_datalen(iter));
        }
        else { /* if (self->itertype == MCDBPY_ITERITEMS) */
            PyObject * restrict tuple, * restrict key, * restrict data;
            key  = mcdbpy_PyObject_From_mcdb(mcdb_iter_keyptr(iter),
                                             mcdb_iter_keylen(iter));
            data = mcdbpy_PyObject_From_mcdb(mcdb_iter_dataptr(iter),
                                             mcdb_iter_datalen(iter));
            tuple= PyTuple_New(2);
            if (key && data && tuple) {
                PyTuple_SET_ITEM(tuple,0,key);
                PyTuple_SET_ITEM(tuple,1,data);
                return tuple;
            }
            else {
                Py_XDECREF(key); Py_XDECREF(data); Py_XDECREF(tuple);
                return NULL;
            }
        }
    }
    PyErr_SetNone(PyExc_StopIteration);  /* no more records */
    return NULL;
}

static PyObject *
mcdbpy_dict_keys(struct mcdbpy * const self)
{
    struct mcdb_iter iter;
    PyObject * restrict key = Py_None;  /*(not NULL; in case mcdb is empty)*/
    PyObject * const restrict r = PyList_New( mcdbpy_numrecs(self) );
    Py_ssize_t i = 0;                         /*preallocate list*/
    if (!r) return NULL;
    mcdb_iter_init(&iter, &self->m);
    while (mcdb_iter(&iter)
           && (key = mcdbpy_PyObject_From_mcdb(mcdb_iter_keyptr(&iter),
                                               mcdb_iter_keylen(&iter))))
        PyList_SET_ITEM(r, i++, key);   /* steal reference; no Py_DECREF(key) */

    if (key)
        return r;
    else {
        Py_DECREF(r);
        return NULL;
    }
}

static PyObject *
mcdbpy_dict_values(struct mcdbpy * const self)
{
    struct mcdb_iter iter;
    PyObject * restrict value = Py_None;/*(not NULL; in case mcdb empty)*/
    PyObject * const restrict r = PyList_New( mcdbpy_numrecs(self) );
    Py_ssize_t i = 0;                         /*preallocate list*/
    if (!r) return NULL;
    mcdb_iter_init(&iter, &self->m);
    while (mcdb_iter(&iter)
           && (value = mcdbpy_PyObject_From_mcdb(mcdb_iter_dataptr(&iter),
                                                 mcdb_iter_datalen(&iter))))
        PyList_SET_ITEM(r, i++, value); /*steal reference; no Py_DECREF(value)*/

    if (value)
        return r;
    else {
        Py_DECREF(r);
        return NULL;
    }
}

static PyObject *
mcdbpy_dict_items(struct mcdbpy * const self)
{
    struct mcdb_iter iter;
    PyObject * restrict key  = Py_None; /*(not NULL; in case mcdb empty)*/
    PyObject * restrict data = Py_None; /*(not NULL; in case mcdb empty)*/
    PyObject * restrict tuple= Py_None; /*(not NULL; in case mcdb empty)*/
    PyObject * const restrict r = PyList_New( mcdbpy_numrecs(self) );
    Py_ssize_t i = 0;                         /*preallocate list*/
    if (!r) return NULL;
    mcdb_iter_init(&iter, &self->m);
    while (mcdb_iter(&iter)
           && (key  = mcdbpy_PyObject_From_mcdb(mcdb_iter_keyptr(&iter),
                                                mcdb_iter_keylen(&iter)))
           && (data = mcdbpy_PyObject_From_mcdb(mcdb_iter_dataptr(&iter),
                                                mcdb_iter_datalen(&iter)))
           && (tuple=PyTuple_New(2))) { /*(?maybe preallocate tuple list?)*/
        PyTuple_SET_ITEM(tuple,0,key);  /*steal reference; no Py_DECREF(key)*/
        PyTuple_SET_ITEM(tuple,1,data); /*steal reference; no Py_DECREF(data)*/
        PyList_SET_ITEM(r, i++, tuple); /*steal reference; no Py_DECREF(tuple)*/
    }

    if (key && data && tuple)
        return r;
    else {
        if (key   && key   != Py_None) Py_DECREF(key);
        if (data  && data  != Py_None) Py_DECREF(data);
        if (tuple && tuple != Py_None) Py_DECREF(tuple);
        Py_DECREF(r);
        return NULL;
    }
}

static PyObject *
mcdbpy_repr (struct mcdbpy * const restrict self)
{
    return Py_INCREF(self->fname), self->fname;
}

static PyObject *
mcdbpy_get_size (struct mcdbpy * const self, void *closure)
{
    return Py_BuildValue("k", self->m.map->size);
}

static void
mcdbpy_dealloc(struct mcdbpy * const self)
{
    if (self->origin) /* ensure self->m.map is not destroyed before all iters */
        Py_DECREF(self->origin);
    else if (self->m.map)
        mcdb_mmap_destroy(self->m.map);
    Py_XDECREF(self->fname);
    self->ob_type->tp_free((PyObject*)self);
}

static int
mcdbpy_init_obj(struct mcdbpy * const restrict self, PyObject * const fname)
{
    if (!PyString_Check(fname)) {
        PyErr_SetString(PyExc_TypeError,"__init__(): expected filename string");
        return -1;
    }
    Py_INCREF(fname);
    self->fname   = fname;
    self->origin  = NULL;
    self->m.loop  = 0;
    /* (mcdb_mmap_create() is not wrapped in
     *  Py_BEGIN_ALLOW_THREADS/Py_END_ALLOW_THREADS
     *  since it uses PyMem_Malloc(), PyMem_Free()) */
    if (!self->m.map)
        self->m.map = mcdb_mmap_create(NULL, NULL, PyString_AS_STRING(fname),
                                       PyMem_Malloc, PyMem_Free);
    return self->m.map ? 0 : (PyErr_SetFromErrno(PyExc_IOError), -1);
}

static int
mcdbpy_init(struct mcdbpy * const restrict self,
            PyObject * const args, PyObject * const kwds)
{
    PyObject *fname;
    return PyArg_ParseTuple(args, "O:__init__", &fname)
      ? mcdbpy_init_obj(self, fname)
      : -1;
}

static PyObject *
mcdbpy_new(PyTypeObject * const type,
           PyObject * const args, PyObject * const kwds)
{
    struct mcdbpy * const restrict self =
      (struct mcdbpy *)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->fname   = NULL;
        self->m.map   = NULL;
        self->origin  = NULL;
    }
    return (PyObject *)self;
}

static PyObject *
mcdbpy_iter_new(struct mcdbpy * const restrict self)
{
    struct mcdbpy * const restrict nself =
      (struct mcdbpy *)mcdbpy_new(&mcdbpy_Type, NULL, NULL);
    if (!nself)
        return NULL;
    nself->m.map = self->m.map;/*set prior to init to trigger reuse*/
    if (mcdbpy_init_obj(nself, self->fname) == 0) {
        Py_INCREF(self);
        nself->origin = self;  /*ensure self->m.map not destroyed before iters*/
        return (PyObject *)nself;
    }
    nself->m.map = NULL;       /*never set nself->origin; undo m.map reference*/
    Py_DECREF(nself);
    return NULL;
}

static PyObject *
mcdbpy_read_ctor(PyTypeObject * const type, PyObject * const fname)
{
    struct mcdbpy * const restrict self =
      (struct mcdbpy *)mcdbpy_new(&mcdbpy_Type, NULL, NULL);
    if (self && mcdbpy_init_obj(self, fname) == 0)
        return (PyObject *)self;
    Py_XDECREF(self);
    return NULL;
}




struct mcdbpy_make {
    PyObject_HEAD
    struct mcdb_make m;
    PyObject *fname;
};

staticforward PyTypeObject mcdbpy_make_Type;

static PyObject *
mcdbpy_make_add(struct mcdbpy_make * const self, PyObject * const args)
{
    const char *key, *data; uint32_t klen, dlen;
    return PyTuple_GET_SIZE(args) == 2
        && mcdbpy_PyObject_to_buf(PyTuple_GET_ITEM(args, 0), &key,  &klen)
        && mcdbpy_PyObject_to_buf(PyTuple_GET_ITEM(args, 1), &data, &dlen)
      ? mcdb_make_add(&self->m, key, klen, data, dlen) == 0
          ? mcdbpy_Py_None()
          : PyErr_SetFromErrno(PyExc_IOError)
      : (PyArg_ParseTuple(args,"s#s#:add",&key,&klen,&data,&dlen), NULL);
        /*re-process to set error strings*/
}

static PyObject *
mcdbpy_make_finish(struct mcdbpy_make * const self, PyObject * const args)
{
    struct mcdb_make * const restrict m = &self->m;
    int do_fsync = true;
    int rc;
    if (!PyArg_ParseTuple(args, "|i:finish", &do_fsync)) /*exception on error*/
        return mcdbpy_Py_None();
    rc = mcdb_make_finish(m); /*(calls PyMem_Malloc(), PyMem_Free())*/
    Py_BEGIN_ALLOW_THREADS
    if (rc == 0)
        rc = mcdb_makefn_finish(m, do_fsync != 0);
    Py_END_ALLOW_THREADS
    return rc == 0 ? mcdbpy_Py_None() : PyErr_SetFromErrno(PyExc_IOError);
}

static PyObject *
mcdbpy_make_repr (struct mcdbpy_make * const restrict self)
{
    return Py_INCREF(self->fname), self->fname;
}

static PyObject *
mcdbpy_make_cancel(struct mcdbpy_make * const restrict self)
{
    mcdb_make_destroy(&self->m);
    mcdb_makefn_cleanup(&self->m);
    Py_XDECREF(self->fname);
    self->fname = NULL;
    return mcdbpy_Py_None();
}

static void
mcdbpy_make_dealloc(struct mcdbpy_make * const restrict self)
{
    mcdb_make_destroy(&self->m);
    mcdb_makefn_cleanup(&self->m);
    Py_XDECREF(self->fname);
    self->ob_type->tp_free((PyObject*)self);
}

static int
mcdbpy_make_init(struct mcdbpy_make * const restrict self,
                 PyObject * const args, PyObject * const kwds)
{
    PyObject *fname;
    int st_mode = ~0;
    if (PyArg_ParseTuple(args, "O|i:__init__", &fname, &st_mode)) {
        if (!PyString_Check(fname)) {
            PyErr_SetString(PyExc_TypeError,
                            "__init__(): expected filename string");
            return -1;
        }
        self->fname = fname;
        Py_INCREF(self->fname);
        if (mcdb_makefn_start(&self->m, PyString_AS_STRING(self->fname),
                              PyMem_Malloc, PyMem_Free) == 0
            && mcdb_make_start(&self->m, self->m.fd,
                               PyMem_Malloc, PyMem_Free) == 0) {
            if (st_mode != ~0)
                self->m.st_mode = (mode_t)st_mode; /* optional mcdb perm mode */
            return 0;
        }
        PyErr_SetFromErrno(PyExc_IOError);
    }
    return -1;
}

static PyObject *
mcdbpy_make_new(PyTypeObject * const type,
                PyObject * const args, PyObject * const kwds)
{
    struct mcdbpy_make * const restrict self =
      (struct mcdbpy_make *)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->fname    = NULL;
        self->m.head[0]= NULL;
        self->m.fd     = -1;
    }
    return (PyObject *)self;
}

static PyObject *
mcdbpy_make_ctor(PyTypeObject * const type, PyObject * const args)
{
    struct mcdbpy_make * const restrict self =
      (struct mcdbpy_make *)mcdbpy_make_new(&mcdbpy_make_Type, NULL, NULL);
    if (self && mcdbpy_make_init(self, args, NULL) == 0)
        return (PyObject *)self;
    Py_XDECREF(self);
    return NULL;
}





static PyObject *
mcdbpy_hash_djb(PyTypeObject * const type, PyObject * const args)
{
    const char *s; uint32_t sz; /*(does not detect error if len(s) > UINT_MAX)*/
    return mcdbpy_PyObject_to_buf(args, &s, &sz)
      ? Py_BuildValue("I", uint32_hash_djb(UINT32_HASH_DJB_INIT, s, (size_t)sz))
      : NULL;
}


/*
 * python-mcdb module documentation, structures, mappings, bootstrap
 */


static const char mcdbpy_module_doc[] =
  "\n"
  "Python interface to create and read mcdb constant databases.\n"
  "\n"
  "The python-mcdb extension module wraps interfaces in libmcdb.so\n"
  "See https://github.com/gstrauss/mcdb/ for latest info on mcdb.\n"
  "mcdb is based on D. J. Bernstein's constant database package\n"
  "(see http://cr.yp.to/cdb.html).\n";

static const char mcdbpy_object_doc[] =
  "\n"
  "mcdb.read Python object interfaces to mcdb query methods in mcdb library:\n"
  "\n"
  "    m = mcdb.read(fname)  (m = mcdb.read_init(fname))  (identical)\n"
  "\n"
  "  Dict methods:\n"
  "    len(m)  (m.__len__())\n"
  "    k in m  (m.__contains__(key))\n"
  "    m[key]  (m.__getitem__(key)) and m.get(k[,default])\n"
  "    iter(m) (m.__iter__()) and it.next()\n"
  "    m.iterkeys(), m.itervalues(), m.iteritems()\n"
  "    m.keys() m.values() m.items()\n"
  "\n"
  "      An mcdb is a constant database; read-only dict methods implemented.\n"
  "      Multi-valued keys are returned more than once.\n"
  "\n"
  "  Additional methods:\n"
  "    m.find(key), m.findnext(), m.findall(key) m.getseq(key[,sequence])\n"
  "\n"
  "  __members__:\n"
  "    m.name - mcdb filename\n"
  "    m.size - mcdb size in bytes\n";

static const char mcdbpy_make_object_doc[] =
  "\n"
  "mcdb.make Python object interfaces to mcdb_make methods in mcdb library:\n"
  "\n"
  "    mk = mcdb.make(fname)  (mk = mcdb.make_init(fname))  (identical)\n"
  "\n"
  "  mcdb make methods:\n"
  "    mk.add(key, data)\n"
  "    mk.finish()\n"
  "\n"
  "  A temporary file is used during creation.\n"
  "  Call mk.add() to add a record.\n"
  "  Call mk.finish() to atomically install the completed mcdb.\n"
  "\n"
  "  __members__:\n"
  "    mk.name    - mcdb filename\n"
  "    mk.fd      - fd underlying mcdb (-1 after finish())\n";



static PyMethodDef mcdbpy_module_funcs[] = {
  {"read_init",    (PyCFunction)mcdbpy_read_ctor,       METH_O,
   "m = mcdb.read_init(f) opens mcdb with filename f, initializes mcdb object"
  },
  {"make_init",    (PyCFunction)mcdbpy_make_ctor,       METH_VARARGS,
   "mk= mcdb.make_init('data.mcdb') begins making new mcdb 'data.mcdb'."
  },
  {"hash",         (PyCFunction)mcdbpy_hash_djb,        METH_O,
   "h = mcdb.hash(b) uses djb hash func to compute 32-bit hash of bytes in b"
  },
  { NULL, NULL, 0, NULL }
};

static PyMethodDef mcdbpy_methods[] = {

  {"__len__",      (PyCFunction)mcdbpy_numrecs,         METH_NOARGS,
   "len(m) returns the number of records in the mcdb"
  },
  {"__contains__", (PyCFunction)mcdbpy_dict_contains,   METH_O,
   "k in m   or m.__contains__(k) returns true/false if mcdb m contains key k\n"
   "(preferred over deprecated (and not implemented) has_key() dict method)"
  },
  {"__getitem__",  (PyCFunction)mcdbpy_dict_getitem,    METH_O,
   "d = m[k] or d = m.__getitem__(k) returns data of key k\n"
   "d = m[k] is similar to d = m.find(k), d = m.get(k), d = m.getseq(k,0)\n"
   "KeyError exception is raised if k does not exist.\n"
   "Numerical values are treated as string keys, not as indexes into mcdb."
  },
  {"get",          (PyCFunction)mcdbpy_dict_get,        METH_VARARGS,
   "d = m.get(k,[default]) returns data of key k\n"
   "If k does not exist, return default arg, if provided, or else None"
   "d = m.get(k) is similar to d = m[k], d = m.find(k), d = m.getseq(k,0)"
  },
  {"getseq",       (PyCFunction)mcdbpy_getseq,          METH_VARARGS,
   "d = m.getseq(k[,i]) returns data of key k, else None if k does not exist\n"
   "  (optional i (default 0) specifies sequence num for multi-valued keys)\n"
   "d = m.getseq(k) is similar to d = m[k], d = m.get(k), d = m.find(k)"
  },
  {"find",         (PyCFunction)mcdbpy_find,            METH_O,
   "d = m.find(k) returns data of key k, else None if k does not exist\n"
   "d = m.find(k) is similar to d = m[k], d = m.get(k), d = m.getseq(k,0)"
  },
  {"findnext",     (PyCFunction)mcdbpy_findnext,        METH_NOARGS,
   "d = m.findnext() returns next data record for multi-value key or None.\n"
   "d = m.findnext() can be called iteratively until None returned, e.g.\n"
   "  e.g.  d = m.find(k)\n"
   "        while d is not None:\n"
   "          process_data(d)\n"
   "          d = m.findnext()\n"
   "Must call m.find(k) (and not receive None) prior to m.findnext()."
  },
  {"findall",      (PyCFunction)mcdbpy_findall,         METH_O,
   "l = m.findall(k) returns data list for multi-value key, or empty list\n"
   "All records for key k are retrieved and returned in a single list.\n"
   "Alternatively, use m.find() and m.findnext() to iterate."
  },
  {"__iter__",     (PyCFunction)mcdbpy_dict_iterkeys,   METH_NOARGS,
   "it = iter(m)  (it = m.__iter__()) returns iterator for keys in mcdb.\n"
   "Iterate by calling it.next().  it = iter(m) is same as it = m.iterkeys()"
  },
  {"iterkeys",     (PyCFunction)mcdbpy_dict_iterkeys,   METH_NOARGS,
   "it = m.iterkeys() returns iterator for keys in mcdb.\n"
   "Iterate by calling it.next()"
  },
  {"itervalues",   (PyCFunction)mcdbpy_dict_itervalues, METH_NOARGS,
   "it = m.itervalues() returns iterator for values in mcdb.\n"
   "Iterate by calling it.next()"
  },
  {"iteritems",    (PyCFunction)mcdbpy_dict_iteritems,  METH_NOARGS,
   "it = m.iteritems() returns iterator for (key,data) tuple items in mcdb.\n"
   "Iterate by calling it.next()"
  },
  {"next",         (PyCFunction)mcdbpy_dict_iternext,   METH_NOARGS,
   "it.next() returns next key in mcdb and increments iterator it.\n"
   "Obtain iterator with one of\n"
   "  it = iter(m), it = m.iterkeys(), it = m.itervalues, it = m.iteritems() \n"
   "The order returned is order in which records were stored in the mcdb.\n"
   "  def mcdbctl_dump(m):            # implements $ mcdbctl dump x.mcdb\n"
   "      for r in m.iteritems():     # r = ('key', 'val')\n"
   "          print \"+%d,%d:%s->%s\" % ( len(r[0]), len(r[1]), r[0], r[1] )\n"
   "      print"
  },
  {"keys",         (PyCFunction)mcdbpy_dict_keys,       METH_NOARGS,
   "l = m.keys() returns list of all keys in mcdb, or empty list\n"
   "Not recommended for use on large mcdb with many records.\n"
   "Iterate by using it = m.iterkeys()"
  },
  {"values",       (PyCFunction)mcdbpy_dict_values,     METH_NOARGS,
   "l = m.values() returns list of all values in mcdb, or empty list\n"
   "Not recommended for use on large mcdb with many records.\n"
   "Iterate by using it = m.itervalues()"
  },
  {"items",        (PyCFunction)mcdbpy_dict_items,      METH_NOARGS,
   "l = m.items() returns list of tuples of (key,data) in mcdb, or empty list\n"
   "Not recommended for use on large mcdb with many records.\n"
   "Iterate by using it = m.iteritems()"
  },
  { NULL, NULL, 0, NULL }
};

static PyMethodDef mcdbpy_make_methods[] = {
  {"add",          (PyCFunction)mcdbpy_make_add,        METH_VARARGS,
   "mk.add(key, data) writes (key,data) tuple into mcdb being created.\n"
   "Returns None."
  },
  {"finish",       (PyCFunction)mcdbpy_make_finish,     METH_VARARGS,
   "mk.finish() generates and writes the mcdb hash tables,\n"
   "flushes all data to disk, and atomically installs mcdb.\n"
   "Returns None."
  },
  {"cancel",       (PyCFunction)mcdbpy_make_cancel,     METH_NOARGS,
   "mk.cancel() cancels and discards the mcdb being created.\n"
   "Returns None."
  },
  { NULL, NULL, 0, NULL }
};

static PyGetSetDef mcdbpy_getseters[] = {
  {"size", (getter)mcdbpy_get_size, NULL,
   "size in bytes", NULL
  },
  { NULL }
};

static PyMemberDef mcdbpy_members[] = {
  {"name", T_OBJECT_EX, offsetof(struct mcdbpy, fname),      READONLY,
   "filename"
  },
  { NULL }
};

static PyMemberDef mcdbpy_make_members[] = {
  {"name", T_OBJECT_EX, offsetof(struct mcdbpy_make, fname), READONLY,
   "filename"
  },
  {"fd",   T_INT,       offsetof(struct mcdbpy_make, m.fd),  READONLY,
   "file descriptor"
  },
  { NULL }
};



static PyMappingMethods mcdbpy_as_mapping = {
  (lenfunc)mcdbpy_numrecs,          /* PyObject_Size(), PyMapping_Length() */
  (binaryfunc)mcdbpy_dict_getitem,  /* PyObject_GetItem() */
  (objobjargproc)0                  /* PyObject_SetItem() (not supported) */
};

/* mcdbpy_make_as_mapping intentionally not created for clarity.
 * mcdb supports multi-value keys and multiple assignments of same key
 * would end up multiple times in mcdb (i.e. replacement *does not* occur).
 * (For the same reason, the Perl module MCDB_File provides no STORE method.)
 */



statichere PyTypeObject mcdbpy_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                                          /*ob_size*/
    "mcdb.read",                                /*tp_name*/
    sizeof(struct mcdbpy),                      /*tp_basicsize*/
    0,                                          /*tp_itemsize*/
    (destructor)mcdbpy_dealloc,                 /*tp_dealloc*/
    0,                                          /*tp_print*/
    0,                                          /*tp_getattr*/
    0,                                          /*tp_setattr*/
    0,                                          /*tp_compare*/
    (reprfunc)mcdbpy_repr,                      /*tp_repr*/
    0,                                          /*tp_as_number*/
    0,                                          /*tp_as_sequence*/
    &mcdbpy_as_mapping,                         /*tp_as_mapping*/
    0,                                          /*tp_hash*/
    0,                                          /*tp_call*/
    0,                                          /*tp_str*/
    0,                                          /*tp_getattro*/
    0,                                          /*tp_setattro*/
    0,                                          /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT|Py_TPFLAGS_HAVE_ITER|Py_TPFLAGS_HAVE_CLASS, /*tp_flags*/
    mcdbpy_object_doc,                          /*tp_doc*/
    0,                                          /*tp_traverse*/
    0,                                          /*tp_clear*/
    0,                                          /*tp_richcompare*/
    0,                                          /*tp_weaklistoffset*/
    (getiterfunc)mcdbpy_dict_iterkeys,          /*tp_iter*/
    (iternextfunc)mcdbpy_dict_iternext,         /*tp_iternext*/
    mcdbpy_methods,                             /*tp_methods*/
    mcdbpy_members,                             /*tp_members*/
    mcdbpy_getseters,                           /*tp_getset*/
    0,                                          /*tp_base*/
    0,                                          /*tp_dict*/
    0,                                          /*tp_descr_get*/
    0,                                          /*tp_descr_set*/
    0,                                          /*tp_dictoffset*/
    (initproc)mcdbpy_init,                      /*tp_init*/
    0,                                          /*tp_alloc*/
    0,  /* see initmcdb() */                    /*tp_new*/
    0,                                          /*tp_free*/
    0,                                          /*tp_gc*/
    0,                                          /*tp_bases*/
    0,                                          /*tp_mro*/
    0,                                          /*tp_cache*/
    0,                                          /*tp_subclasses*/
    0,                                          /*tp_weaklist*/
};                                              /*tp_... see Include/object.h*/

statichere PyTypeObject mcdbpy_make_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                                          /*ob_size*/
    "mcdb.make",                                /*tp_name*/
    sizeof(struct mcdbpy_make),                 /*tp_basicsize*/
    0,                                          /*tp_itemsize*/
    (destructor)mcdbpy_make_dealloc,            /*tp_dealloc*/
    0,                                          /*tp_print*/
    0,                                          /*tp_getattr*/
    0,                                          /*tp_setattr*/
    0,                                          /*tp_compare*/
    (reprfunc)mcdbpy_make_repr,                 /*tp_repr*/
    0,                                          /*tp_as_number*/
    0,                                          /*tp_as_sequence*/
    0,                                          /*tp_as_mapping*/
    0,                                          /*tp_hash*/
    0,                                          /*tp_call*/
    0,                                          /*tp_str*/
    0,                                          /*tp_getattro*/
    0,                                          /*tp_setattro*/
    0,                                          /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT|Py_TPFLAGS_HAVE_CLASS,   /*tp_flags*/
    mcdbpy_make_object_doc,                     /*tp_doc*/
    0,                                          /*tp_traverse*/
    0,                                          /*tp_clear*/
    0,                                          /*tp_richcompare*/
    0,                                          /*tp_weaklistoffset*/
    0,                                          /*tp_iter*/
    0,                                          /*tp_iternext*/
    mcdbpy_make_methods,                        /*tp_methods*/
    mcdbpy_make_members,                        /*tp_members*/
    0,                                          /*tp_getset*/
    0,                                          /*tp_base*/
    0,                                          /*tp_dict*/
    0,                                          /*tp_descr_get*/
    0,                                          /*tp_descr_set*/
    0,                                          /*tp_dictoffset*/
    (initproc)mcdbpy_make_init,                 /*tp_init*/
    0,                                          /*tp_alloc*/
    0,  /* see initmcdb() */                    /*tp_new*/
    0,                                          /*tp_free*/
    0,                                          /*tp_gc*/
    0,                                          /*tp_bases*/
    0,                                          /*tp_mro*/
    0,                                          /*tp_cache*/
    0,                                          /*tp_subclasses*/
    0,                                          /*tp_weaklist*/
};                                              /*tp_... see Include/object.h*/

#ifndef PyMODINIT_FUNC /* declarations for DLL import/export */
#define PyMODINIT_FUNC void
#endif
PyMODINIT_FUNC
initmcdb(void) {
    PyObject *m;
    mcdbpy_Type.tp_new      = mcdbpy_new;
    mcdbpy_make_Type.tp_new = mcdbpy_make_new;
    if (PyType_Ready(&mcdbpy_Type) < 0 || PyType_Ready(&mcdbpy_make_Type) < 0)
        return;
    m = Py_InitModule3("mcdb", mcdbpy_module_funcs, mcdbpy_module_doc);
    PyModule_AddStringConstant(m, "__version__", VERSION);
    PyModule_AddObject(m, "read", (PyObject *)&mcdbpy_Type);
    PyModule_AddObject(m, "make", (PyObject *)&mcdbpy_make_Type);
}
