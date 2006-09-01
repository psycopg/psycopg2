/* lobject_type.c - python interface to lobject objects
 *
 * Copyright (C) 2003-2006 Federico Di Gregorio <fog@debian.org>
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

#include <Python.h>
#include <structmember.h>
#include <string.h>

#define PSYCOPG_MODULE
#include "psycopg/config.h"
#include "psycopg/python.h"
#include "psycopg/psycopg.h"
#include "psycopg/lobject.h"
#include "psycopg/connection.h"
#include "psycopg/microprotocols.h"
#include "psycopg/microprotocols_proto.h"
#include "psycopg/pqpath.h"
#include "pgversion.h"


#ifdef PSYCOPG_EXTENSIONS

/** public methods **/

/* close method - close the lobject */

#define psyco_lobj_close_doc \
"close() -- Close the lobject."

static PyObject *
psyco_lobj_close(lobjectObject *self, PyObject *args)
{
    if (!PyArg_ParseTuple(args, "")) return NULL;
    
    EXC_IF_LOBJ_CLOSED(self);
    EXC_IF_LOBJ_LEVEL0(self);
    EXC_IF_LOBJ_UNMARKED(self);

    self->closed = 1;
    if (self->fd != -1)
        lo_close(self->conn->pgconn, self->fd);
    self->fd = -1;

    Dprintf("psyco_lobj_close: lobject at %p closed", self);

    Py_INCREF(Py_None);
    return Py_None;
}


/** the lobject object **/

/* object method list */

static struct PyMethodDef lobjectObject_methods[] = {
    {"close", (PyCFunction)psyco_lobj_close,
     METH_VARARGS, psyco_lobj_close_doc},
    {NULL}
};

/* object member list */

static struct PyMemberDef lobjectObject_members[] = {
    {"oid", T_LONG,
        offsetof(lobjectObject, oid), RO,
        "The backend OID associated to this lobject."},
    {NULL}
};

/* initialization and finalization methods */

static int
lobject_setup(lobjectObject *self, connectionObject *conn,
              Oid oid, int mode, Oid new_oid, char *new_file)
{
    Dprintf("lobject_setup: init lobject object at %p", self);
    
    if (conn->isolation_level == 0) {
        psyco_set_error(ProgrammingError, (PyObject*)self, 
            "can't use a lobject outside of transactions", NULL, NULL);
        return -1;
    }

    self->conn = conn;
    self->mark = conn->mark;

    Py_INCREF((PyObject*)self->conn);
    
    self->closed = 0;
    self->oid = InvalidOid;
    self->fd = -1;

    if (lobject_open(self, conn, oid, mode, new_oid, new_file) == -1)
        return -1;
    else {
        Dprintf("lobject_setup: good lobject object at %p, refcnt = %d",
                self, ((PyObject *)self)->ob_refcnt);
        Dprintf("lobject_setup:    oid = %d, fd = %d", self->oid, self->fd);
        return 0;
    }
}

static void
lobject_dealloc(PyObject* obj)
{
    lobjectObject *self = (lobjectObject *)obj;

    if (self->conn->isolation_level > 0
        && self->conn->mark == self->mark) {
        if (self->fd != -1)
            lo_close(self->conn->pgconn, self->fd);
    }
    Py_XDECREF((PyObject*)self->conn);

    Dprintf("lobject_dealloc: deleted lobject object at %p, refcnt = %d",
            obj, obj->ob_refcnt);

    obj->ob_type->tp_free(obj);
}

static int
lobject_init(PyObject *obj, PyObject *args, PyObject *kwds)
{
    Oid oid=InvalidOid, new_oid=InvalidOid;
    int mode=0;
    char *new_file = NULL;
    PyObject *conn;
    
    if (!PyArg_ParseTuple(args, "O|iiis",
         &conn, &oid, &mode, &new_oid, &new_file))
        return -1;

    return lobject_setup((lobjectObject *)obj,
        (connectionObject *)conn, oid, mode, new_oid, new_file);
}

static PyObject *
lobject_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    return type->tp_alloc(type, 0);
}

static void
lobject_del(PyObject* self)
{
    PyObject_Del(self);
}

static PyObject *
lobject_repr(lobjectObject *self)
{
    return PyString_FromFormat(
        "<lobject object at %p; closed: %d>", self, self->closed);
}


/* object type */

#define lobjectType_doc \
"A database large object."

PyTypeObject lobjectType = {
    PyObject_HEAD_INIT(NULL)
    0,
    "psycopg2._psycopg.lobject",
    sizeof(lobjectObject),
    0,
    lobject_dealloc, /*tp_dealloc*/
    0,          /*tp_print*/  
    0,          /*tp_getattr*/
    0,          /*tp_setattr*/
    0,          /*tp_compare*/
    (reprfunc)lobject_repr, /*tp_repr*/
    0,          /*tp_as_number*/
    0,          /*tp_as_sequence*/
    0,          /*tp_as_mapping*/
    0,          /*tp_hash */

    0,          /*tp_call*/
    (reprfunc)lobject_repr, /*tp_str*/
    0,          /*tp_getattro*/
    0,          /*tp_setattro*/
    0,          /*tp_as_buffer*/

    Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE|Py_TPFLAGS_HAVE_ITER, /*tp_flags*/
    lobjectType_doc, /*tp_doc*/
    
    0,          /*tp_traverse*/
    0,          /*tp_clear*/

    0,          /*tp_richcompare*/
    0,          /*tp_weaklistoffset*/

    0,          /*tp_iter*/
    0,          /*tp_iternext*/

    /* Attribute descriptor and subclassing stuff */

    lobjectObject_methods, /*tp_methods*/
    lobjectObject_members, /*tp_members*/
    0,          /*tp_getset*/
    0,          /*tp_base*/
    0,          /*tp_dict*/
    
    0,          /*tp_descr_get*/
    0,          /*tp_descr_set*/
    0,          /*tp_dictoffset*/
    
    lobject_init, /*tp_init*/
    0, /*tp_alloc  Will be set to PyType_GenericAlloc in module init*/
    lobject_new, /*tp_new*/
    (freefunc)lobject_del, /*tp_free  Low-level free-memory routine */
    0,          /*tp_is_gc For PyObject_IS_GC */
    0,          /*tp_bases*/
    0,          /*tp_mro method resolution order */
    0,          /*tp_cache*/
    0,          /*tp_subclasses*/
    0           /*tp_weaklist*/
};

#endif

