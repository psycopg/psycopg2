/* microprotocol_proto.c - psycopg protocols
 *
 * Copyright (C) 2003 Federico Di Gregorio <fog@debian.org>
 *
 * This file is part of psycopg.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <structmember.h>
#include <stringobject.h>

#include <string.h>

#define PSYCOPG_MODULE
#include "psycopg/config.h"
#include "psycopg/python.h"
#include "psycopg/psycopg.h"
#include "psycopg/microprotocols_proto.h"


/** void protocol implementation **/


/* getquoted - return quoted representation for object */

#define psyco_isqlquote_getquoted_doc \
"getquoted() -- return SQL-quoted representation of this object"

static PyObject *
psyco_isqlquote_getquoted(isqlquoteObject *self, PyObject *args)
{
    if (!PyArg_ParseTuple(args, "")) return NULL;

    Py_INCREF(Py_None);
    return Py_None;
}

/* getbinary - return quoted representation for object */

#define psyco_isqlquote_getbinary_doc \
"getbinary() -- return SQL-quoted binary representation of this object"

static PyObject *
psyco_isqlquote_getbinary(isqlquoteObject *self, PyObject *args)
{
    if (!PyArg_ParseTuple(args, "")) return NULL;

    Py_INCREF(Py_None);
    return Py_None;
}

/* getbuffer - return quoted representation for object */

#define psyco_isqlquote_getbuffer_doc \
"getbuffer() -- return this object"

static PyObject *
psyco_isqlquote_getbuffer(isqlquoteObject *self, PyObject *args)
{
    if (!PyArg_ParseTuple(args, "")) return NULL;

    Py_INCREF(Py_None);
    return Py_None;
}



/** the ISQLQuote object **/


/* object method list */

static struct PyMethodDef isqlquoteObject_methods[] = {
    {"getquoted", (PyCFunction)psyco_isqlquote_getquoted,
     METH_VARARGS, psyco_isqlquote_getquoted_doc},
    {"getbinary", (PyCFunction)psyco_isqlquote_getbinary,
     METH_VARARGS, psyco_isqlquote_getbinary_doc},
    {"getbuffer", (PyCFunction)psyco_isqlquote_getbuffer,
     METH_VARARGS, psyco_isqlquote_getbuffer_doc},
    /*    {"prepare", (PyCFunction)psyco_isqlquote_prepare,
          METH_VARARGS, psyco_isqlquote_prepare_doc}, */
    {NULL}
};

/* object member list */

static struct PyMemberDef isqlquoteObject_members[] = {
    /* DBAPI-2.0 extensions (exception objects) */
    {"_wrapped", T_OBJECT, offsetof(isqlquoteObject, wrapped), RO},
    {NULL}
};

/* initialization and finalization methods */

static int
isqlquote_setup(isqlquoteObject *self, PyObject *wrapped)
{
    self->wrapped = wrapped;
    Py_INCREF(wrapped);

    return 0;
}

static void
isqlquote_dealloc(PyObject* obj)
{
    isqlquoteObject *self = (isqlquoteObject *)obj;

    Py_XDECREF(self->wrapped);

    obj->ob_type->tp_free(obj);
}

static int
isqlquote_init(PyObject *obj, PyObject *args, PyObject *kwds)
{
    PyObject *wrapped = NULL;

    if (!PyArg_ParseTuple(args, "O", &wrapped))
        return -1;

    return isqlquote_setup((isqlquoteObject *)obj, wrapped);
}

static PyObject *
isqlquote_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    return type->tp_alloc(type, 0);
}

static void
isqlquote_del(PyObject* self)
{
    PyObject_Del(self);
}


/* object type */

#define isqlquoteType_doc \
"Abstract ISQLQuote protocol\n\n" \
"An object conform to this protocol should expose a ``getquoted()`` method\n" \
"returning the SQL representation of the object.\n\n"

PyTypeObject isqlquoteType = {
    PyObject_HEAD_INIT(NULL)
    0,
    "psycopg2._psycopg.ISQLQuote",
    sizeof(isqlquoteObject),
    0,
    isqlquote_dealloc, /*tp_dealloc*/
    0,          /*tp_print*/
    0,          /*tp_getattr*/
    0,          /*tp_setattr*/
    0,          /*tp_compare*/
    0,          /*tp_repr*/
    0,          /*tp_as_number*/
    0,          /*tp_as_sequence*/
    0,          /*tp_as_mapping*/
    0,          /*tp_hash */

    0,          /*tp_call*/
    0,          /*tp_str*/
    0,          /*tp_getattro*/
    0,          /*tp_setattro*/
    0,          /*tp_as_buffer*/

    Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE, /*tp_flags*/
    isqlquoteType_doc, /*tp_doc*/

    0,          /*tp_traverse*/
    0,          /*tp_clear*/

    0,          /*tp_richcompare*/
    0,          /*tp_weaklistoffset*/

    0,          /*tp_iter*/
    0,          /*tp_iternext*/

    /* Attribute descriptor and subclassing stuff */

    isqlquoteObject_methods, /*tp_methods*/
    isqlquoteObject_members, /*tp_members*/
    0,          /*tp_getset*/
    0,          /*tp_base*/
    0,          /*tp_dict*/

    0,          /*tp_descr_get*/
    0,          /*tp_descr_set*/
    0,          /*tp_dictoffset*/

    isqlquote_init, /*tp_init*/
    0, /*tp_alloc  will be set to PyType_GenericAlloc in module init*/
    isqlquote_new, /*tp_new*/
    (freefunc)isqlquote_del, /*tp_free  Low-level free-memory routine */
    0,          /*tp_is_gc For PyObject_IS_GC */
    0,          /*tp_bases*/
    0,          /*tp_mro method resolution order */
    0,          /*tp_cache*/
    0,          /*tp_subclasses*/
    0           /*tp_weaklist*/
};
