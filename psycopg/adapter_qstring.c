/* adapter_qstring.c - QuotedString objects
 *
 * Copyright (C) 2003-2010 Federico Di Gregorio <fog@debian.org>
 *
 * This file is part of psycopg.
 *
 * psycopg2 is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link this program with the OpenSSL library (or with
 * modified versions of OpenSSL that use the same license as OpenSSL),
 * and distribute linked combinations including the two.
 *
 * You must obey the GNU Lesser General Public License in all respects for
 * all of the code used other than OpenSSL.
 *
 * psycopg2 is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */

#define PSYCOPG_MODULE
#include "psycopg/psycopg.h"

#include "psycopg/connection.h"
#include "psycopg/adapter_qstring.h"
#include "psycopg/microprotocols_proto.h"

#include <string.h>


/* qstring_quote - do the quote process on plain and unicode strings */

BORROWED static PyObject *
qstring_quote(qstringObject *self)
{
    PyObject *str;
    char *s, *buffer;
    Py_ssize_t len, qlen;

    /* if the wrapped object is an unicode object we can encode it to match
       self->encoding but if the encoding is not specified we don't know what
       to do and we raise an exception */

    Dprintf("qstring_quote: encoding to %s", self->encoding);

    if (PyUnicode_Check(self->wrapped) && self->encoding) {
        str = PyUnicode_AsEncodedString(self->wrapped, self->encoding, NULL);
        Dprintf("qstring_quote: got encoded object at %p", str);
        if (str == NULL) return NULL;
    }

#if PY_MAJOR_VERSION < 3
    /* if the wrapped object is a simple string, we don't know how to
       (re)encode it, so we pass it as-is */
    else if (PyString_Check(self->wrapped)) {
        str = self->wrapped;
        /* INCREF to make it ref-wise identical to unicode one */
        Py_INCREF(str);
    }
#endif

    /* if the wrapped object is not a string, this is an error */
    else {
        PyErr_SetString(PyExc_TypeError,
                        "can't quote non-string object (or missing encoding)");
        return NULL;
    }

    /* encode the string into buffer */
    Bytes_AsStringAndSize(str, &s, &len);

    /* Call qstring_escape with the GIL released, then reacquire the GIL
       before verifying that the results can fit into a Python string; raise
       an exception if not. */        

    Py_BEGIN_ALLOW_THREADS
    buffer = psycopg_escape_string(self->conn, s, len, NULL, &qlen);
    Py_END_ALLOW_THREADS
    
    if (buffer == NULL) {
        Py_DECREF(str);
        PyErr_NoMemory();
        return NULL;
    }

    if (qlen > (size_t) PY_SSIZE_T_MAX) {
        PyErr_SetString(PyExc_IndexError,
            "PG buffer too large to fit in Python buffer.");
        PyMem_Free(buffer);
        Py_DECREF(str);
        return NULL;
    }

    self->buffer = Bytes_FromStringAndSize(buffer, qlen);
    PyMem_Free(buffer);
    Py_DECREF(str);

    return self->buffer;
}

/* qstring_str, qstring_getquoted - return result of quoting */

static PyObject *
qstring_getquoted(qstringObject *self, PyObject *args)
{
    if (self->buffer == NULL) {
        qstring_quote(self);
    }
    Py_XINCREF(self->buffer);
    return self->buffer;
}

static PyObject *
qstring_str(qstringObject *self)
{
    return psycopg_ensure_text(qstring_getquoted(self, NULL));
}

static PyObject *
qstring_prepare(qstringObject *self, PyObject *args)
{
    PyObject *conn;

    if (!PyArg_ParseTuple(args, "O!", &connectionType, &conn))
        return NULL;

    /* we bother copying the encoding only if the wrapped string is unicode,
       we don't need the encoding if that's not the case */
    if (PyUnicode_Check(self->wrapped)) {
        if (self->encoding) free(self->encoding);
        self->encoding = strdup(((connectionObject *)conn)->codec);
        Dprintf("qstring_prepare: set encoding to %s", self->encoding);
    }

    Py_CLEAR(self->conn);
    Py_INCREF(conn);
    self->conn = conn;

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
qstring_conform(qstringObject *self, PyObject *args)
{
    PyObject *res, *proto;

    if (!PyArg_ParseTuple(args, "O", &proto)) return NULL;

    if (proto == (PyObject*)&isqlquoteType)
        res = (PyObject*)self;
    else
        res = Py_None;

    Py_INCREF(res);
    return res;
}

/** the QuotedString object **/

/* object member list */

static struct PyMemberDef qstringObject_members[] = {
    {"adapted", T_OBJECT, offsetof(qstringObject, wrapped), READONLY},
    {"buffer", T_OBJECT, offsetof(qstringObject, buffer), READONLY},
    {"encoding", T_STRING, offsetof(qstringObject, encoding), READONLY},
    {NULL}
};

/* object method table */

static PyMethodDef qstringObject_methods[] = {
    {"getquoted", (PyCFunction)qstring_getquoted, METH_NOARGS,
     "getquoted() -> wrapped object value as SQL-quoted string"},
    {"prepare", (PyCFunction)qstring_prepare, METH_VARARGS,
     "prepare(conn) -> set encoding to conn->encoding and store conn"},
    {"__conform__", (PyCFunction)qstring_conform, METH_VARARGS, NULL},
    {NULL}  /* Sentinel */
};

/* initialization and finalization methods */

static int
qstring_setup(qstringObject *self, PyObject *str, const char *enc)
{
    Dprintf("qstring_setup: init qstring object at %p, refcnt = "
        FORMAT_CODE_PY_SSIZE_T,
        self, Py_REFCNT(self)
      );

    self->buffer = NULL;
    self->conn = NULL;

    /* FIXME: remove this orrible strdup */
    if (enc) self->encoding = strdup(enc);

    Py_INCREF(str);
    self->wrapped = str;

    Dprintf("qstring_setup: good qstring object at %p, refcnt = "
        FORMAT_CODE_PY_SSIZE_T,
        self, Py_REFCNT(self)
      );
    return 0;
}

static int
qstring_traverse(PyObject *obj, visitproc visit, void *arg)
{
    qstringObject *self = (qstringObject *)obj;

    Py_VISIT(self->wrapped);
    Py_VISIT(self->buffer);
    Py_VISIT(self->conn);
    return 0;
}

static void
qstring_dealloc(PyObject* obj)
{
    qstringObject *self = (qstringObject *)obj;

    Py_CLEAR(self->wrapped);
    Py_CLEAR(self->buffer);
    Py_CLEAR(self->conn);

    if (self->encoding) free(self->encoding);

    Dprintf("qstring_dealloc: deleted qstring object at %p, refcnt = "
        FORMAT_CODE_PY_SSIZE_T,
        obj, Py_REFCNT(obj)
      );

    Py_TYPE(obj)->tp_free(obj);
}

static int
qstring_init(PyObject *obj, PyObject *args, PyObject *kwds)
{
    PyObject *str;
    const char *enc = "latin-1"; /* default encoding as in Python */

    if (!PyArg_ParseTuple(args, "O|s", &str, &enc))
        return -1;

    return qstring_setup((qstringObject *)obj, str, enc);
}

static PyObject *
qstring_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    return type->tp_alloc(type, 0);
}

static void
qstring_del(PyObject* self)
{
    PyObject_GC_Del(self);
}

static PyObject *
qstring_repr(qstringObject *self)
{
    return PyString_FromFormat("<psycopg2._psycopg.QuotedString object at %p>",
                                self);
}

/* object type */

#define qstringType_doc \
"QuotedString(str, enc) -> new quoted object with 'enc' encoding"

PyTypeObject qstringType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "psycopg2._psycopg.QuotedString",
    sizeof(qstringObject),
    0,
    qstring_dealloc, /*tp_dealloc*/
    0,          /*tp_print*/
    0,          /*tp_getattr*/
    0,          /*tp_setattr*/

    0,          /*tp_compare*/
    (reprfunc)qstring_repr, /*tp_repr*/
    0,          /*tp_as_number*/
    0,          /*tp_as_sequence*/
    0,          /*tp_as_mapping*/
    0,          /*tp_hash */

    0,          /*tp_call*/
    (reprfunc)qstring_str, /*tp_str*/
    0,          /*tp_getattro*/
    0,          /*tp_setattro*/
    0,          /*tp_as_buffer*/

    Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE|Py_TPFLAGS_HAVE_GC, /*tp_flags*/

    qstringType_doc, /*tp_doc*/

    qstring_traverse, /*tp_traverse*/
    0,          /*tp_clear*/

    0,          /*tp_richcompare*/
    0,          /*tp_weaklistoffset*/

    0,          /*tp_iter*/
    0,          /*tp_iternext*/

    /* Attribute descriptor and subclassing stuff */

    qstringObject_methods, /*tp_methods*/
    qstringObject_members, /*tp_members*/
    0,          /*tp_getset*/
    0,          /*tp_base*/
    0,          /*tp_dict*/

    0,          /*tp_descr_get*/
    0,          /*tp_descr_set*/
    0,          /*tp_dictoffset*/

    qstring_init, /*tp_init*/
    0, /*tp_alloc  will be set to PyType_GenericAlloc in module init*/
    qstring_new, /*tp_new*/
    (freefunc)qstring_del, /*tp_free  Low-level free-memory routine */
    0,          /*tp_is_gc For PyObject_IS_GC */
    0,          /*tp_bases*/
    0,          /*tp_mro method resolution order */
    0,          /*tp_cache*/
    0,          /*tp_subclasses*/
    0           /*tp_weaklist*/
};


/** module-level functions **/

PyObject *
psyco_QuotedString(PyObject *module, PyObject *args)
{
    PyObject *str;
    const char *enc = "latin-1"; /* default encoding as in Python */

    if (!PyArg_ParseTuple(args, "O|s", &str, &enc))
        return NULL;

    return PyObject_CallFunction((PyObject *)&qstringType, "Os", str, enc);
}
