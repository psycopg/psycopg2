/* adapter_list.c - python list objects
 *
 * Copyright (C) 2004-2010 Federico Di Gregorio <fog@debian.org>
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

#include "psycopg/adapter_list.h"
#include "psycopg/microprotocols.h"
#include "psycopg/microprotocols_proto.h"


/* list_str, list_getquoted - return result of quoting */

static PyObject *
list_quote(listObject *self)
{
    /*  adapt the list by calling adapt() recursively and then wrapping
        everything into "ARRAY[]" */
    PyObject *tmp = NULL, *str = NULL, *joined = NULL, *res = NULL;
    Py_ssize_t i, len;

    len = PyList_GET_SIZE(self->wrapped);

    /* empty arrays are converted to NULLs (still searching for a way to
       insert an empty array in postgresql */
    if (len == 0) return Bytes_FromString("'{}'");

    tmp = PyTuple_New(len);

    for (i=0; i<len; i++) {
        PyObject *quoted;
        PyObject *wrapped = PyList_GET_ITEM(self->wrapped, i);
        if (wrapped == Py_None) {
            Py_INCREF(psyco_null);
            quoted = psyco_null;
        }
        else {
            quoted = microprotocol_getquoted(wrapped,
                                       (connectionObject*)self->connection);
            if (quoted == NULL) goto error;
        }

        /* here we don't loose a refcnt: SET_ITEM does not change the
           reference count and we are just transferring ownership of the tmp
           object to the tuple */
        PyTuple_SET_ITEM(tmp, i, quoted);
    }

    /* now that we have a tuple of adapted objects we just need to join them
       and put "ARRAY[] around the result */
    str = Bytes_FromString(", ");
    joined = PyObject_CallMethod(str, "join", "(O)", tmp);
    if (joined == NULL) goto error;

    res = Bytes_FromFormat("ARRAY[%s]", Bytes_AsString(joined));

 error:
    Py_XDECREF(tmp);
    Py_XDECREF(str);
    Py_XDECREF(joined);
    return res;
}

static PyObject *
list_str(listObject *self)
{
    return psycopg_ensure_text(list_quote(self));
}

static PyObject *
list_getquoted(listObject *self, PyObject *args)
{
    return list_quote(self);
}

static PyObject *
list_prepare(listObject *self, PyObject *args)
{
    PyObject *conn;

    if (!PyArg_ParseTuple(args, "O!", &connectionType, &conn))
        return NULL;

    /* note that we don't copy the encoding from the connection, but take a
       reference to it; we'll need it during the recursive adapt() call (the
       encoding is here for a future expansion that will make .getquoted()
       work even without a connection to the backend. */
    Py_CLEAR(self->connection);
    Py_INCREF(conn);
    self->connection = conn;

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
list_conform(listObject *self, PyObject *args)
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

/** the DateTime wrapper object **/

/* object member list */

static struct PyMemberDef listObject_members[] = {
    {"adapted", T_OBJECT, offsetof(listObject, wrapped), READONLY},
    {NULL}
};

/* object method table */

static PyMethodDef listObject_methods[] = {
    {"getquoted", (PyCFunction)list_getquoted, METH_NOARGS,
     "getquoted() -> wrapped object value as SQL date/time"},
    {"prepare", (PyCFunction)list_prepare, METH_VARARGS,
     "prepare(conn) -> set encoding to conn->encoding"},
    {"__conform__", (PyCFunction)list_conform, METH_VARARGS, NULL},
    {NULL}  /* Sentinel */
};

/* initialization and finalization methods */

static int
list_setup(listObject *self, PyObject *obj, const char *enc)
{
    Dprintf("list_setup: init list object at %p, refcnt = "
        FORMAT_CODE_PY_SSIZE_T,
        self, Py_REFCNT(self)
      );

    if (!PyList_Check(obj))
        return -1;

    /* FIXME: remove this orrible strdup */
    if (enc) self->encoding = strdup(enc);

    self->connection = NULL;
    Py_INCREF(obj);
    self->wrapped = obj;

    Dprintf("list_setup: good list object at %p, refcnt = "
        FORMAT_CODE_PY_SSIZE_T,
        self, Py_REFCNT(self)
      );
    return 0;
}

static int
list_traverse(PyObject *obj, visitproc visit, void *arg)
{
    listObject *self = (listObject *)obj;

    Py_VISIT(self->wrapped);
    Py_VISIT(self->connection);
    return 0;
}

static void
list_dealloc(PyObject* obj)
{
    listObject *self = (listObject *)obj;

    Py_CLEAR(self->wrapped);
    Py_CLEAR(self->connection);
    if (self->encoding) free(self->encoding);

    Dprintf("list_dealloc: deleted list object at %p, "
            "refcnt = " FORMAT_CODE_PY_SSIZE_T, obj, Py_REFCNT(obj));

    Py_TYPE(obj)->tp_free(obj);
}

static int
list_init(PyObject *obj, PyObject *args, PyObject *kwds)
{
    PyObject *l;
    const char *enc = "latin-1"; /* default encoding as in Python */

    if (!PyArg_ParseTuple(args, "O|s", &l, &enc))
        return -1;

    return list_setup((listObject *)obj, l, enc);
}

static PyObject *
list_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    return type->tp_alloc(type, 0);
}

static void
list_del(PyObject* self)
{
    PyObject_GC_Del(self);
}

static PyObject *
list_repr(listObject *self)
{
    return PyString_FromFormat("<psycopg2._psycopg.List object at %p>", self);
}

/* object type */

#define listType_doc \
"List(list) -> new list wrapper object"

PyTypeObject listType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "psycopg2._psycopg.List",
    sizeof(listObject),
    0,
    list_dealloc, /*tp_dealloc*/
    0,          /*tp_print*/
    0,          /*tp_getattr*/
    0,          /*tp_setattr*/

    0,          /*tp_compare*/
    (reprfunc)list_repr, /*tp_repr*/
    0,          /*tp_as_number*/
    0,          /*tp_as_sequence*/
    0,          /*tp_as_mapping*/
    0,          /*tp_hash */

    0,          /*tp_call*/
    (reprfunc)list_str, /*tp_str*/
    0,          /*tp_getattro*/
    0,          /*tp_setattro*/
    0,          /*tp_as_buffer*/

    Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE|Py_TPFLAGS_HAVE_GC, /*tp_flags*/

    listType_doc, /*tp_doc*/

    list_traverse, /*tp_traverse*/
    0,          /*tp_clear*/

    0,          /*tp_richcompare*/
    0,          /*tp_weaklistoffset*/

    0,          /*tp_iter*/
    0,          /*tp_iternext*/

    /* Attribute descriptor and subclassing stuff */

    listObject_methods, /*tp_methods*/
    listObject_members, /*tp_members*/
    0,          /*tp_getset*/
    0,          /*tp_base*/
    0,          /*tp_dict*/

    0,          /*tp_descr_get*/
    0,          /*tp_descr_set*/
    0,          /*tp_dictoffset*/

    list_init, /*tp_init*/
    0, /*tp_alloc  will be set to PyType_GenericAlloc in module init*/
    list_new, /*tp_new*/
    (freefunc)list_del, /*tp_free  Low-level free-memory routine */
    0,          /*tp_is_gc For PyObject_IS_GC */
    0,          /*tp_bases*/
    0,          /*tp_mro method resolution order */
    0,          /*tp_cache*/
    0,          /*tp_subclasses*/
    0           /*tp_weaklist*/
};


/** module-level functions **/

PyObject *
psyco_List(PyObject *module, PyObject *args)
{
    PyObject *str;
    const char *enc = "latin-1"; /* default encoding as in Python */

    if (!PyArg_ParseTuple(args, "O|s", &str, &enc))
        return NULL;

    return PyObject_CallFunction((PyObject *)&listType, "Os", str, enc);
}
