/* connection_type.c - python interface to connection objects
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

#include <Python.h>
#include <structmember.h>
#include <stringobject.h>

#include <string.h>

#define PSYCOPG_MODULE
#include "psycopg/config.h"
#include "psycopg/python.h"
#include "psycopg/psycopg.h"
#include "psycopg/connection.h"
#include "psycopg/cursor.h"

/** DBAPI methods **/

/* cursor method - allocate a new cursor */

#define psyco_conn_cursor_doc \
"cursor(factory=psycopg.cursor) -> new cursor\n\n" \
"Return a new cursor. The 'factory' argument can be used to create\n" \
"non-standard cursors by passing a class different from the default.\n" \
"Note that the new class *should* be a sub-class of psycopg.cursor."

static PyObject *
psyco_conn_cursor(connectionObject *self, PyObject *args, PyObject *keywds)
{
    char *name = NULL;
    PyObject *obj, *factory = NULL;

    static char *kwlist[] = {"name", "factory", NULL};
    
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "|sO", kwlist,
                                     &name, &factory)) {
        return NULL;
    }

    EXC_IF_CONN_CLOSED(self);

    Dprintf("psyco_conn_cursor: new cursor for connection at %p", self);
    Dprintf("psyco_conn_cursor:     parameters: name = %s", name);
    
    if (factory == NULL) factory = (PyObject *)&cursorType;
    obj = PyObject_CallFunction(factory, "O", self);    

    /* TODO: added error checking on obj (cursor) here */
    
    /* add the cursor to this connection's list (and decref it, so that it has
       the right number of references to go away even if still in the list) */
    PyList_Append(self->cursors, obj);
    Py_DECREF(obj);
        
    Dprintf("psyco_conn_cursor: new cursor at %p: refcnt = %d",
            obj, obj->ob_refcnt);
    return obj;
}



/* close method - close the connection and all related cursors */

#define psyco_conn_close_doc "close() -> close the connection"

static PyObject *
psyco_conn_close(connectionObject *self, PyObject *args)
{
    EXC_IF_CONN_CLOSED(self);

    if (!PyArg_ParseTuple(args, "")) return NULL;
                                     
    Dprintf("psyco_conn_close: closing connection at %p", self);
    conn_close(self);
    Dprintf("psyco_conn_close: connection at %p closed", self);

    Py_INCREF(Py_None);
    return Py_None;
}



/* commit method - commit all changes to the database */

#define psyco_conn_commit_doc "commit() -> commit all changes to database"

static PyObject *
psyco_conn_commit(connectionObject *self, PyObject *args)
{
    EXC_IF_CONN_CLOSED(self);

    if (!PyArg_ParseTuple(args, "")) return NULL;

    /* FIXME: check return status? */
    conn_commit(self);

    Py_INCREF(Py_None);
    return Py_None;
}



/* rollback method - roll back all changes done to the database */

#define psyco_conn_rollback_doc \
"rollback() -> roll back all changes done to database"

static PyObject *
psyco_conn_rollback(connectionObject *self, PyObject *args)
{
    EXC_IF_CONN_CLOSED(self);

    if (!PyArg_ParseTuple(args, "")) return NULL;

    /* FIXME: check return status? */
    conn_rollback(self);

    Py_INCREF(Py_None);
    return Py_None;
}




/* set_isolation_level method - switch connection isolation level */

#define psyco_conn_set_isolation_level_doc \
"set_isolation_level(level) -> swicth isolation level to 'level'"

static PyObject *
psyco_conn_set_isolation_level(connectionObject *self, PyObject *args)
{
    int level = 1;
    
    EXC_IF_CONN_CLOSED(self);

    if (!PyArg_ParseTuple(args, "i", &level)) return NULL;

    if (level < 0 || level > 3) {
        PyErr_SetString(PyExc_ValueError,
                        "isolation level out of bounds (0,3)");
        return NULL;
    }
    
    /* FIXME: check return status? */
    conn_switch_isolation_level(self, level);

    Py_INCREF(Py_None);
    return Py_None;
}



/* set_isolation_level method - switch connection isolation level */

#define psyco_conn_set_client_encoding_doc \
"set_client_encoding(level) -> swicth isolation level to 'level'"

static PyObject *
psyco_conn_set_client_encoding(connectionObject *self, PyObject *args)
{
    char *enc = NULL;
    
    EXC_IF_CONN_CLOSED(self);

    if (!PyArg_ParseTuple(args, "s", &enc)) return NULL;
    
    if (conn_set_client_encoding(self, enc) == 0) {
        Py_INCREF(Py_None);
        return Py_None;
    }
    else {
        return NULL;
    }
}



/** the connection object **/


/* object method list */

static struct PyMethodDef connectionObject_methods[] = {
    {"cursor", (PyCFunction)psyco_conn_cursor,
     METH_VARARGS|METH_KEYWORDS, psyco_conn_cursor_doc},
    {"close", (PyCFunction)psyco_conn_close,
     METH_VARARGS, psyco_conn_close_doc},
    {"commit", (PyCFunction)psyco_conn_commit,
     METH_VARARGS, psyco_conn_commit_doc},
    {"rollback", (PyCFunction)psyco_conn_rollback,
     METH_VARARGS, psyco_conn_rollback_doc},
#ifdef PSYCOPG_EXTENSIONS
    {"set_isolation_level", (PyCFunction)psyco_conn_set_isolation_level,
     METH_VARARGS, psyco_conn_set_isolation_level_doc},
    {"set_client_encoding", (PyCFunction)psyco_conn_set_client_encoding,
     METH_VARARGS, psyco_conn_set_client_encoding_doc},    
#endif    
    {NULL}
};

/* object member list */

static struct PyMemberDef connectionObject_members[] = {
    /* DBAPI-2.0 extensions (exception objects) */
    {"Error", T_OBJECT, offsetof(connectionObject,exc_Error), RO},
    {"Warning", T_OBJECT, offsetof(connectionObject, exc_Warning), RO},
    {"InterfaceError", T_OBJECT,
     offsetof(connectionObject, exc_InterfaceError), RO},
    {"DatabaseError", T_OBJECT,
     offsetof(connectionObject, exc_DatabaseError), RO},
    {"InternalError", T_OBJECT,
     offsetof(connectionObject, exc_InternalError), RO},
    {"OperationalError", T_OBJECT,
     offsetof(connectionObject, exc_OperationalError), RO},
    {"ProgrammingError", T_OBJECT,
     offsetof(connectionObject, exc_ProgrammingError), RO},
    {"IntegrityError", T_OBJECT,
     offsetof(connectionObject, exc_IntegrityError), RO},
    {"DataError", T_OBJECT,
     offsetof(connectionObject, exc_DataError), RO},
    {"NotSupportedError", T_OBJECT,
     offsetof(connectionObject, exc_NotSupportedError), RO},
#ifdef PSYCOPG_EXTENSIONS    
    {"closed", T_LONG, offsetof(connectionObject, closed), RO},
    {"isolation_level", T_LONG,
     offsetof(connectionObject, isolation_level), RO},
    {"encoding", T_STRING, offsetof(connectionObject, encoding), RO},
    {"cursors", T_OBJECT, offsetof(connectionObject, cursors), RO},   
    {"notices", T_OBJECT, offsetof(connectionObject, notice_list), RO},
    {"notifies", T_OBJECT, offsetof(connectionObject, notifies), RO},
    {"dsn", T_STRING, offsetof(connectionObject, dsn), RO},
#endif    
    {NULL}
};

/* initialization and finalization methods */

static int
connection_setup(connectionObject *self, char *dsn)
{
    Dprintf("connection_setup: init connection object at %p, refcnt = %d",
            self, ((PyObject *)self)->ob_refcnt);
    
    self->dsn = strdup(dsn);
    self->cursors = PyList_New(0);
    self->notice_list = PyList_New(0);
    self->closed = 0;
    self->isolation_level = 1;
    self->status = CONN_STATUS_READY;
    self->critical = NULL;
    self->async_cursor = NULL;
    self->pgconn = NULL;
    
    pthread_mutex_init(&(self->lock), NULL);
    
    if (conn_connect(self) != 0) {
        Py_XDECREF(self->cursors);
        pthread_mutex_destroy(&(self->lock));
        Dprintf("connection_init: FAILED");
        return -1;
    }

    Dprintf("connection_setup: good connection object at %p, refcnt = %d",
            self, ((PyObject *)self)->ob_refcnt);
    return 0;
}

static void
connection_dealloc(PyObject* obj)
{
    connectionObject *self = (connectionObject *)obj;

    if (self->closed == 0) conn_close(self);
    
    Py_XDECREF(self->cursors);
    if (self->dsn) free(self->dsn);
    if (self->encoding) free(self->encoding);
    if (self->critical) free(self->critical);
    
    pthread_mutex_destroy(&(self->lock));

    Dprintf("connection_dealloc: deleted connection object at %p, refcnt = %d",
            obj, obj->ob_refcnt);

    obj->ob_type->tp_free(obj);
}

static int
connection_init(PyObject *obj, PyObject *args, PyObject *kwds)
{
    char *dsn;

    if (!PyArg_ParseTuple(args, "s", &dsn))
        return -1;

    return connection_setup((connectionObject *)obj, dsn);
}

static PyObject *
connection_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    return type->tp_alloc(type, 0);
}

static void
connection_del(PyObject* self)
{
    PyObject_Del(self);
}

static PyObject *
connection_repr(connectionObject *self)
{
    return PyString_FromFormat(
        "<connection object at %p; dsn: '%s', closed: %ld>",
        self, self->dsn, self->closed);
}


/* object type */

#define connectionType_doc \
"connection(dsn, ...) -> new connection object"

PyTypeObject connectionType = {
    PyObject_HEAD_INIT(NULL)
    0,
    "psycopg.connection",
    sizeof(connectionObject),
    0,
    connection_dealloc, /*tp_dealloc*/
    0,          /*tp_print*/ 
    0,          /*tp_getattr*/
    0,          /*tp_setattr*/
    0,          /*tp_compare*/
    (reprfunc)connection_repr, /*tp_repr*/
    0,          /*tp_as_number*/
    0,          /*tp_as_sequence*/
    0,          /*tp_as_mapping*/
    0,          /*tp_hash */

    0,          /*tp_call*/
    (reprfunc)connection_repr, /*tp_str*/
    0,          /*tp_getattro*/
    0,          /*tp_setattro*/
    0,          /*tp_as_buffer*/

    Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE, /*tp_flags*/
    connectionType_doc, /*tp_doc*/
    
    0,          /*tp_traverse*/
    0,          /*tp_clear*/

    0,          /*tp_richcompare*/
    0,          /*tp_weaklistoffset*/

    0,          /*tp_iter*/
    0,          /*tp_iternext*/

    /* Attribute descriptor and subclassing stuff */

    connectionObject_methods, /*tp_methods*/
    connectionObject_members, /*tp_members*/
    0,          /*tp_getset*/
    0,          /*tp_base*/
    0,          /*tp_dict*/
    
    0,          /*tp_descr_get*/
    0,          /*tp_descr_set*/
    0,          /*tp_dictoffset*/
    
    connection_init, /*tp_init*/
    PyType_GenericAlloc, /*tp_alloc*/
    connection_new, /*tp_new*/
    (freefunc)connection_del, /*tp_free  Low-level free-memory routine */
    0,          /*tp_is_gc For PyObject_IS_GC */
    0,          /*tp_bases*/
    0,          /*tp_mro method resolution order */
    0,          /*tp_cache*/
    0,          /*tp_subclasses*/
    0           /*tp_weaklist*/
};
