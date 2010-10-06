/* cursor_type.c - python interface to cursor objects
 *
 * Copyright (C) 2008  Canonical Ltd.
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
 * You should have received a copy of the GNU General Public Likcense
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <structmember.h>

#define PSYCOPG_MODULE
#include "psycopg/config.h"
#include "psycopg/python.h"
#include "psycopg/psycopg.h"
#include "psycopg/xid.h"

static const char xid_prefix[] = "psycopg-v1";

static PyMemberDef xid_members[] = {
    { "format_id", T_OBJECT, offsetof(XidObject, format_id), RO },
    { "gtrid", T_OBJECT, offsetof(XidObject, gtrid), RO },
    { "bqual", T_OBJECT, offsetof(XidObject, bqual), RO },
    { "prepared", T_OBJECT, offsetof(XidObject, prepared), RO },
    { "owner", T_OBJECT, offsetof(XidObject, owner), RO },
    { "database", T_OBJECT, offsetof(XidObject, database), RO },
    { NULL }
};

static PyObject *
xid_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    XidObject *self = (XidObject *)type->tp_alloc(type, 0);

    self->pg_xact_id = NULL;
    Py_INCREF(Py_None);
    self->format_id = Py_None;
    Py_INCREF(Py_None);
    self->gtrid = Py_None;
    Py_INCREF(Py_None);
    self->bqual = Py_None;
    Py_INCREF(Py_None);
    self->prepared = Py_None;
    Py_INCREF(Py_None);
    self->owner = Py_None;
    Py_INCREF(Py_None);
    self->database = Py_None;

    return (PyObject *)self;
}

static int
xid_init(XidObject *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = {"format_id", "gtrid", "bqual", NULL};
    int format_id, i, gtrid_len, bqual_len, xid_len;
    const char *gtrid, *bqual;
    PyObject *tmp;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "iss", kwlist,
                                     &format_id, &gtrid, &bqual))
        return -1;

    if (format_id < 0 || format_id > 0x7fffffff) {
        PyErr_SetString(PyExc_ValueError,
                        "format_id must be a non-negative 32-bit integer");
        return -1;
    }

    /* make sure that gtrid is no more than 64 characters long and
       made of printable characters (which we're defining as those
       between 0x20 and 0x7f). */
    gtrid_len = strlen(gtrid);
    if (gtrid_len > 64) {
        PyErr_SetString(PyExc_ValueError,
                        "gtrid must be a string no longer than 64 characters");
        return -1;
    }
    for (i = 0; i < gtrid_len; i++) {
        if (gtrid[i] < 0x20 || gtrid[i] >= 0x7f) {
            PyErr_SetString(PyExc_ValueError,
                            "gtrid must contain only printable characters.");
            return -1;
        }
    }
    /* Same for bqual */
    bqual_len = strlen(bqual);
    if (bqual_len > 64) {
        PyErr_SetString(PyExc_ValueError,
                        "bqual must be a string no longer than 64 characters");
        return -1;
    }
    for (i = 0; i < bqual_len; i++) {
        if (bqual[i] < 0x20 || bqual[i] >= 0x7f) {
            PyErr_SetString(PyExc_ValueError,
                            "bqual must contain only printable characters.");
            return -1;
        }
    }

    /* Now we can construct the PostgreSQL transaction ID, which is of
       the following form:

         psycopg-v1:$FORMAT_ID:$GTRID_LEN:$GTRID$BQUAL

       Where $FORMAT_ID is eight hex digits and $GTRID_LEN is 2 hex digits.
    */
    xid_len = strlen(xid_prefix) + 1 + 8 + 1 + 2 + 1 + gtrid_len + bqual_len + 1;
    if (self->pg_xact_id)
        free(self->pg_xact_id);
    self->pg_xact_id = malloc(xid_len);
    memset(self->pg_xact_id, '\0', xid_len);
    snprintf(self->pg_xact_id, xid_len, "%s:%08X:%02X:%s%s",
             xid_prefix, format_id, gtrid_len, gtrid, bqual);

    tmp = self->format_id;
    self->format_id = PyInt_FromLong(format_id);
    Py_XDECREF(tmp);

    tmp = self->gtrid;
    self->gtrid = PyString_FromString(gtrid);
    Py_XDECREF(tmp);

    tmp = self->bqual;
    self->bqual = PyString_FromString(bqual);
    Py_XDECREF(tmp);

    return 0;
}

static int
xid_traverse(XidObject *self, visitproc visit, void *arg)
{
    Py_VISIT(self->format_id);
    Py_VISIT(self->gtrid);
    Py_VISIT(self->bqual);
    Py_VISIT(self->prepared);
    Py_VISIT(self->owner);
    Py_VISIT(self->database);
    return 0;
}

static void
xid_dealloc(XidObject *self)
{
    if (self->pg_xact_id) {
        free(self->pg_xact_id);
        self->pg_xact_id = NULL;
    }
    Py_CLEAR(self->format_id);
    Py_CLEAR(self->gtrid);
    Py_CLEAR(self->bqual);
    Py_CLEAR(self->prepared);
    Py_CLEAR(self->owner);
    Py_CLEAR(self->database);

    self->ob_type->tp_free((PyObject *)self);
}

static void
xid_del(PyObject *self)
{
    PyObject_GC_Del(self);
}

static PyObject *
xid_repr(XidObject *self)
{
    return PyString_FromFormat("<Xid \"%s\">",
                               self->pg_xact_id ? self->pg_xact_id : "(null)");
}

static Py_ssize_t
xid_len(XidObject *self)
{
    return 3;
}

static PyObject *
xid_getitem(XidObject *self, Py_ssize_t item)
{
    if (item < 0)
        item += 3;

    switch (item) {
    case 0:
        Py_INCREF(self->format_id);
        return self->format_id;
    case 1:
        Py_INCREF(self->gtrid);
        return self->gtrid;
    case 2:
        Py_INCREF(self->bqual);
        return self->bqual;
    default:
        PyErr_SetString(PyExc_IndexError, "index out of range");
        return NULL;
    }
}

static PySequenceMethods xid_sequence = {
    (lenfunc)xid_len,          /* sq_length */
    0,                         /* sq_concat */
    0,                         /* sq_repeat */
    (ssizeargfunc)xid_getitem, /* sq_item */
    0,                         /* sq_slice */
    0,                         /* sq_ass_item */
    0,                         /* sq_ass_slice */
    0,                         /* sq_contains */
    0,                         /* sq_inplace_concat */
    0,                         /* sq_inplace_repeat */
};

static const char xid_doc[] =
    "A transaction identifier used for two phase commit.";

PyTypeObject XidType = {
    PyObject_HEAD_INIT(NULL)
    0,
    "psycopg2.extensions.Xid",
    sizeof(XidObject),
    0,
    (destructor)xid_dealloc, /* tp_dealloc */
    0,          /*tp_print*/

    0,          /*tp_getattr*/
    0,          /*tp_setattr*/

    0,          /*tp_compare*/

    (reprfunc)xid_repr, /*tp_repr*/
    0,          /*tp_as_number*/
    &xid_sequence, /*tp_as_sequence*/
    0,          /*tp_as_mapping*/
    0,          /*tp_hash */

    0,          /*tp_call*/
    0,          /*tp_str*/

    0,          /*tp_getattro*/
    0,          /*tp_setattro*/
    0,          /*tp_as_buffer*/

    Py_TPFLAGS_DEFAULT|Py_TPFLAGS_HAVE_GC, /*tp_flags*/
    xid_doc, /*tp_doc*/

    (traverseproc)xid_traverse, /*tp_traverse*/
    0,          /*tp_clear*/

    0,          /*tp_richcompare*/
    0,          /*tp_weaklistoffset*/

    0,          /*tp_iter*/
    0,          /*tp_iternext*/

    /* Attribute descriptor and subclassing stuff */

    0,          /*tp_methods*/
    xid_members, /*tp_members*/
    0,          /*tp_getset*/
    0,          /*tp_base*/
    0,          /*tp_dict*/

    0,          /*tp_descr_get*/
    0,          /*tp_descr_set*/
    0,          /*tp_dictoffset*/

    (initproc)xid_init, /*tp_init*/
    0, /*tp_alloc  will be set to PyType_GenericAlloc in module init*/
    xid_new, /*tp_new*/
    (freefunc)xid_del, /*tp_free  Low-level free-memory routine */
    0,          /*tp_is_gc For PyObject_IS_GC */
    0,          /*tp_bases*/
    0,          /*tp_mro method resolution order */
    0,          /*tp_cache*/
    0,          /*tp_subclasses*/
    0           /*tp_weaklist*/
};
