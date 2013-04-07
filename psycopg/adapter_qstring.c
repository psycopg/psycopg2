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

static const char *default_encoding = "latin1";

/* qstring_quote - do the quote process on plain and unicode strings */

static PyObject *
qstring_quote(qstringObject *self)
{
    PyObject *str = NULL;
    char *s, *buffer = NULL;
    Py_ssize_t len, qlen;
    const char *encoding = default_encoding;
    PyObject *rv = NULL;

    /* if the wrapped object is an unicode object we can encode it to match
       conn->encoding but if the encoding is not specified we don't know what
       to do and we raise an exception */
    if (self->conn) {
        encoding = self->conn->codec;
    }

    Dprintf("qstring_quote: encoding to %s", encoding);

    if (PyUnicode_Check(self->wrapped) && encoding) {
        str = PyUnicode_AsEncodedString(self->wrapped, encoding, NULL);
        Dprintf("qstring_quote: got encoded object at %p", str);
        if (str == NULL) goto exit;
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
        goto exit;
    }

    /* encode the string into buffer */
    Bytes_AsStringAndSize(str, &s, &len);
    if (!(buffer = psycopg_escape_string(self->conn, s, len, NULL, &qlen))) {
        goto exit;
    }

    if (qlen > (size_t) PY_SSIZE_T_MAX) {
        PyErr_SetString(PyExc_IndexError,
            "PG buffer too large to fit in Python buffer.");
        goto exit;
    }

    rv = Bytes_FromStringAndSize(buffer, qlen);

exit:
    PyMem_Free(buffer);
    Py_XDECREF(str);

    return rv;
}

/* qstring_str, qstring_getquoted - return result of quoting */

static PyObject *
qstring_getquoted(qstringObject *self, PyObject *args)
{
    if (self->buffer == NULL) {
        self->buffer = qstring_quote(self);
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
    connectionObject *conn;

    if (!PyArg_ParseTuple(args, "O!", &connectionType, &conn))
        return NULL;

    Py_CLEAR(self->conn);
    Py_INCREF(conn);
    self->conn = conn;

    Py_RETURN_NONE;
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

static PyObject *
qstring_get_encoding(qstringObject *self)
{
    const char *encoding = default_encoding;

    if (self->conn) {
        encoding = self->conn->codec;
    }

    return Text_FromUTF8(encoding);
}

/** the QuotedString object **/

/* object member list */

static struct PyMemberDef qstringObject_members[] = {
    {"adapted", T_OBJECT, offsetof(qstringObject, wrapped), READONLY},
    {"buffer", T_OBJECT, offsetof(qstringObject, buffer), READONLY},
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

static PyGetSetDef qstringObject_getsets[] = {
    { "encoding",
        (getter)qstring_get_encoding,
        (setter)NULL,
        "current encoding of the adapter" },
    {NULL}
};

/* initialization and finalization methods */

static int
qstring_setup(qstringObject *self, PyObject *str)
{
    Dprintf("qstring_setup: init qstring object at %p, refcnt = "
        FORMAT_CODE_PY_SSIZE_T,
        self, Py_REFCNT(self)
      );

    Py_INCREF(str);
    self->wrapped = str;

    Dprintf("qstring_setup: good qstring object at %p, refcnt = "
        FORMAT_CODE_PY_SSIZE_T,
        self, Py_REFCNT(self)
      );
    return 0;
}

static void
qstring_dealloc(PyObject* obj)
{
    qstringObject *self = (qstringObject *)obj;

    Py_CLEAR(self->wrapped);
    Py_CLEAR(self->buffer);
    Py_CLEAR(self->conn);

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

    if (!PyArg_ParseTuple(args, "O", &str))
        return -1;

    return qstring_setup((qstringObject *)obj, str);
}

static PyObject *
qstring_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    return type->tp_alloc(type, 0);
}

static PyObject *
qstring_repr(qstringObject *self)
{
    return PyString_FromFormat("<psycopg2._psycopg.QuotedString object at %p>",
                                self);
}

/* object type */

#define qstringType_doc \
"QuotedString(str) -> new quoted object"

PyTypeObject qstringType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "psycopg2._psycopg.QuotedString",
    sizeof(qstringObject), 0,
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
    Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE, /*tp_flags*/
    qstringType_doc, /*tp_doc*/
    0,          /*tp_traverse*/
    0,          /*tp_clear*/
    0,          /*tp_richcompare*/
    0,          /*tp_weaklistoffset*/
    0,          /*tp_iter*/
    0,          /*tp_iternext*/
    qstringObject_methods, /*tp_methods*/
    qstringObject_members, /*tp_members*/
    qstringObject_getsets, /*tp_getset*/
    0,          /*tp_base*/
    0,          /*tp_dict*/
    0,          /*tp_descr_get*/
    0,          /*tp_descr_set*/
    0,          /*tp_dictoffset*/
    qstring_init, /*tp_init*/
    0,          /*tp_alloc*/
    qstring_new, /*tp_new*/
};


/** module-level functions **/

PyObject *
psyco_QuotedString(PyObject *module, PyObject *args)
{
    PyObject *str;

    if (!PyArg_ParseTuple(args, "O", &str))
        return NULL;

    return PyObject_CallFunctionObjArgs((PyObject *)&qstringType, str, NULL);
}
