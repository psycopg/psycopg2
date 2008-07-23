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

    /* file-like objects can be closed multiple times and remember that
       closing the current transaction is equivalent to close all the
       opened large objects */
    if (!lobject_is_closed(self)
        && self->conn->isolation_level > 0
	&& self->conn->mark == self->mark)
    {
        Dprintf("psyco_lobj_close: closing lobject at %p", self);
        if (lobject_close(self) < 0)
            return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

/* write method - write data to the lobject */

#define psyco_lobj_write_doc \
"write(str) -- Write a string to the large object."

static PyObject *
psyco_lobj_write(lobjectObject *self, PyObject *args)
{
    int len, res=0;
    const char *buffer;

    if (!PyArg_ParseTuple(args, "s#", &buffer, &len)) return NULL;

    EXC_IF_LOBJ_CLOSED(self);
    EXC_IF_LOBJ_LEVEL0(self);
    EXC_IF_LOBJ_UNMARKED(self);

    if ((res = lobject_write(self, buffer, len)) < 0) return NULL;

    return PyInt_FromLong((long)res);
}

/* read method - read data from the lobject */

#define psyco_lobj_read_doc \
"read(size=-1) -- Read at most size bytes or to the end of the large object."

static PyObject *
psyco_lobj_read(lobjectObject *self, PyObject *args)
{
    int where, end, size = -1;
    char *buffer;

    if (!PyArg_ParseTuple(args, "|i", &size)) return NULL;

    EXC_IF_LOBJ_CLOSED(self);
    EXC_IF_LOBJ_LEVEL0(self);
    EXC_IF_LOBJ_UNMARKED(self);

    if (size < 0) {
        if ((where = lobject_tell(self)) < 0) return NULL;
        if ((end = lobject_seek(self, 0, SEEK_END)) < 0) return NULL;
        if (lobject_seek(self, where, SEEK_SET) < 0) return NULL;
        size = end - where;
    }

    if ((buffer = PyMem_Malloc(size)) == NULL) return NULL;
    if ((size = lobject_read(self, buffer, size)) < 0) {
        PyMem_Free(buffer);
        return NULL;
    }

    return PyString_FromStringAndSize(buffer, size);
}

/* seek method - seek in the lobject */

#define psyco_lobj_seek_doc \
"seek(offset, whence=0) -- Set the lobject's current position."

static PyObject *
psyco_lobj_seek(lobjectObject *self, PyObject *args)
{
    int offset, whence=0;
    int pos=0;

    if (!PyArg_ParseTuple(args, "i|i", &offset, &whence))
    	return NULL;

    EXC_IF_LOBJ_CLOSED(self);
    EXC_IF_LOBJ_LEVEL0(self);
    EXC_IF_LOBJ_UNMARKED(self);

    if ((pos = lobject_seek(self, offset, whence)) < 0)
    	return NULL;

    return PyInt_FromLong((long)pos);
}

/* tell method - tell current position in the lobject */

#define psyco_lobj_tell_doc \
"tell() -- Return the lobject's current position."

static PyObject *
psyco_lobj_tell(lobjectObject *self, PyObject *args)
{
    int pos;

    if (!PyArg_ParseTuple(args, "")) return NULL;

    EXC_IF_LOBJ_CLOSED(self);
    EXC_IF_LOBJ_LEVEL0(self);
    EXC_IF_LOBJ_UNMARKED(self);

    if ((pos = lobject_tell(self)) < 0)
    	return NULL;

    return PyInt_FromLong((long)pos);
}

/* unlink method - unlink (destroy) the lobject */

#define psyco_lobj_unlink_doc \
"unlink() -- Close and then remove the lobject."

static PyObject *
psyco_lobj_unlink(lobjectObject *self, PyObject *args)
{
    if (!PyArg_ParseTuple(args, "")) return NULL;

    if (lobject_unlink(self) < 0)
    	return NULL;

    Py_INCREF(Py_None);
    return Py_None;
}

/* export method - export lobject's content to given file */

#define psyco_lobj_export_doc \
"export(filename) -- Export large object to given file."

static PyObject *
psyco_lobj_export(lobjectObject *self, PyObject *args)
{
    const char *filename;

    if (!PyArg_ParseTuple(args, "s", &filename))
    	return NULL;

    EXC_IF_LOBJ_LEVEL0(self);

    if (lobject_export(self, filename) < 0)
    	return NULL;

    Py_INCREF(Py_None);
    return Py_None;
}


static PyObject *
psyco_lobj_get_closed(lobjectObject *self, void *closure)
{
    PyObject *closed;

    closed = lobject_is_closed(self) ? Py_True : Py_False;
    Py_INCREF(closed);
    return closed;
}


/** the lobject object **/

/* object method list */

static struct PyMethodDef lobjectObject_methods[] = {
    {"read", (PyCFunction)psyco_lobj_read,
     METH_VARARGS, psyco_lobj_read_doc},
    {"write", (PyCFunction)psyco_lobj_write,
     METH_VARARGS, psyco_lobj_write_doc},
    {"seek", (PyCFunction)psyco_lobj_seek,
     METH_VARARGS, psyco_lobj_seek_doc},
    {"tell", (PyCFunction)psyco_lobj_tell,
     METH_VARARGS, psyco_lobj_tell_doc},
    {"close", (PyCFunction)psyco_lobj_close,
     METH_VARARGS, psyco_lobj_close_doc},
    {"unlink",(PyCFunction)psyco_lobj_unlink,
     METH_VARARGS, psyco_lobj_unlink_doc},
    {"export",(PyCFunction)psyco_lobj_export,
     METH_VARARGS, psyco_lobj_export_doc},
    {NULL}
};

/* object member list */

static struct PyMemberDef lobjectObject_members[] = {
    {"oid", T_UINT, offsetof(lobjectObject, oid), RO,
        "The backend OID associated to this lobject."},
    {"mode", T_STRING, offsetof(lobjectObject, smode), RO,
        "Open mode ('r', 'w', 'rw' or 'n')."},
    {NULL}
};

/* object getset list */

static struct PyGetSetDef lobjectObject_getsets[] = {
    {"closed", (getter)psyco_lobj_get_closed, NULL,
     "The if the large object is closed (no file-like methods)."},
    {NULL}
};

/* initialization and finalization methods */

static int
lobject_setup(lobjectObject *self, connectionObject *conn,
              Oid oid, int mode, Oid new_oid, const char *new_file)
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

    self->fd = -1;
    self->oid = InvalidOid;

    if (lobject_open(self, conn, oid, mode, new_oid, new_file) == -1)
        return -1;

   Dprintf("lobject_setup: good lobject object at %p, refcnt = "
           FORMAT_CODE_PY_SSIZE_T, self, ((PyObject *)self)->ob_refcnt);
   Dprintf("lobject_setup:    oid = %d, fd = %d", self->oid, self->fd);
   return 0;
}

static void
lobject_dealloc(PyObject* obj)
{
    lobjectObject *self = (lobjectObject *)obj;

    if (lobject_close(self) < 0)
        PyErr_Print();
    Py_XDECREF((PyObject*)self->conn);

    Dprintf("lobject_dealloc: deleted lobject object at %p, refcnt = "
            FORMAT_CODE_PY_SSIZE_T, obj, obj->ob_refcnt);

    obj->ob_type->tp_free(obj);
}

static int
lobject_init(PyObject *obj, PyObject *args, PyObject *kwds)
{
    Oid oid=InvalidOid, new_oid=InvalidOid;
    int mode=0;
    const char *new_file = NULL;
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
        "<lobject object at %p; closed: %d>", self, lobject_is_closed(self));
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
    lobjectObject_getsets, /*tp_getset*/
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

