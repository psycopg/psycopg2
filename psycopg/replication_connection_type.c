/* replication_connection_type.c - python interface to replication connection objects
 *
 * Copyright (C) 2015 Daniele Varrazzo <daniele.varrazzo@gmail.com>
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

#include "psycopg/replication_connection.h"
#include "psycopg/replication_message.h"
#include "psycopg/green.h"
#include "psycopg/pqpath.h"

#include <string.h>
#include <stdlib.h>


#define psyco_repl_conn_type_doc \
"replication_type -- the replication connection type"

static PyObject *
psyco_repl_conn_get_type(replicationConnectionObject *self)
{
    connectionObject *conn = &self->conn;
    PyObject *res = NULL;

    EXC_IF_CONN_CLOSED(conn);

    if (self->type == REPLICATION_PHYSICAL) {
        res = replicationPhysicalConst;
    } else if (self->type == REPLICATION_LOGICAL) {
        res = replicationLogicalConst;
    } else {
        PyErr_Format(PyExc_TypeError, "unknown replication type constant: %ld", self->type);
    }

    Py_XINCREF(res);
    return res;
}


static int
replicationConnection_init(PyObject *obj, PyObject *args, PyObject *kwargs)
{
    replicationConnectionObject *self = (replicationConnectionObject *)obj;
    PyObject *dsn = NULL, *replication_type = NULL,
        *item = NULL, *ext = NULL, *make_dsn = NULL,
        *extras = NULL, *cursor = NULL;
    int async = 0;
    int ret = -1;

    /* 'replication_type' is not actually optional, but there's no
       good way to put it before 'async' in the list */
    static char *kwlist[] = {"dsn", "async", "replication_type", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|iO", kwlist,
                                     &dsn, &async, &replication_type)) { return ret; }

    /*
      We have to call make_dsn() to add replication-specific
      connection parameters, because the DSN might be an URI (if there
      were no keyword arguments to connect() it is passed unchanged).
    */
    /* we reuse args and kwargs to call make_dsn() and parent type's tp_init() */
    if (!(kwargs = PyDict_New())) { return ret; }
    Py_INCREF(args);

    /* we also reuse the dsn to hold the result of the make_dsn() call */
    Py_INCREF(dsn);

    if (!(ext = PyImport_ImportModule("psycopg2.extensions"))) { goto exit; }
    if (!(make_dsn = PyObject_GetAttrString(ext, "make_dsn"))) { goto exit; }

    /* all the nice stuff is located in python-level ReplicationCursor class */
    if (!(extras = PyImport_ImportModule("psycopg2.extras"))) { goto exit; }
    if (!(cursor = PyObject_GetAttrString(extras, "ReplicationCursor"))) { goto exit; }

    /* checking the object reference helps to avoid recognizing
       unrelated integer constants as valid input values */
    if (replication_type == replicationPhysicalConst) {
        self->type = REPLICATION_PHYSICAL;

#define SET_ITEM(k, v) \
        if (!(item = Text_FromUTF8(#v))) { goto exit; } \
        if (PyDict_SetItemString(kwargs, #k, item) != 0) { goto exit; } \
        Py_DECREF(item); \
        item = NULL;

        SET_ITEM(replication, true);
        SET_ITEM(dbname, replication);  /* required for .pgpass lookup */
    } else if (replication_type == replicationLogicalConst) {
        self->type = REPLICATION_LOGICAL;

        SET_ITEM(replication, database);
#undef SET_ITEM
    } else {
        PyErr_SetString(PyExc_TypeError,
                        "replication_type must be either REPLICATION_PHYSICAL or REPLICATION_LOGICAL");
        goto exit;
    }

    Py_DECREF(args);
    if (!(args = PyTuple_Pack(1, dsn))) { goto exit; }

    Py_DECREF(dsn);
    if (!(dsn = PyObject_Call(make_dsn, args, kwargs))) { goto exit; }

    Py_DECREF(args);
    if (!(args = Py_BuildValue("(Oi)", dsn, async))) { goto exit; }

    /* only attempt the connection once we've handled all possible errors */
    if ((ret = connectionType.tp_init(obj, args, NULL)) < 0) { goto exit; }

    self->conn.autocommit = 1;
    Py_INCREF(self->conn.cursor_factory = cursor);

exit:
    Py_XDECREF(item);
    Py_XDECREF(ext);
    Py_XDECREF(make_dsn);
    Py_XDECREF(extras);
    Py_XDECREF(cursor);
    Py_XDECREF(dsn);
    Py_XDECREF(args);
    Py_XDECREF(kwargs);

    return ret;
}

static PyObject *
replicationConnection_repr(replicationConnectionObject *self)
{
    return PyString_FromFormat(
        "<ReplicationConnection object at %p; dsn: '%s', closed: %ld>",
        self, self->conn.dsn, self->conn.closed);
}


/* object calculated member list */

static struct PyGetSetDef replicationConnectionObject_getsets[] = {
    /* override to prevent user tweaking these: */
    { "autocommit", NULL, NULL, NULL },
    { "isolation_level", NULL, NULL, NULL },
    { "set_session", NULL, NULL, NULL },
    { "set_isolation_level", NULL, NULL, NULL },
    { "reset", NULL, NULL, NULL },
    /* an actual getter */
    { "replication_type",
      (getter)psyco_repl_conn_get_type, NULL,
      psyco_repl_conn_type_doc, NULL },
    {NULL}
};

/* object type */

#define replicationConnectionType_doc \
"A replication connection."

PyTypeObject replicationConnectionType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "psycopg2.extensions.ReplicationConnection",
    sizeof(replicationConnectionObject), 0,
    0,          /*tp_dealloc*/
    0,          /*tp_print*/
    0,          /*tp_getattr*/
    0,          /*tp_setattr*/
    0,          /*tp_compare*/
    (reprfunc)replicationConnection_repr, /*tp_repr*/
    0,          /*tp_as_number*/
    0,          /*tp_as_sequence*/
    0,          /*tp_as_mapping*/
    0,          /*tp_hash*/
    0,          /*tp_call*/
    (reprfunc)replicationConnection_repr, /*tp_str*/
    0,          /*tp_getattro*/
    0,          /*tp_setattro*/
    0,          /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_ITER |
      Py_TPFLAGS_HAVE_GC, /*tp_flags*/
    replicationConnectionType_doc, /*tp_doc*/
    0,          /*tp_traverse*/
    0,          /*tp_clear*/
    0,          /*tp_richcompare*/
    0,          /*tp_weaklistoffset*/
    0,          /*tp_iter*/
    0,          /*tp_iternext*/
    0,          /*tp_methods*/
    0,          /*tp_members*/
    replicationConnectionObject_getsets, /*tp_getset*/
    &connectionType, /*tp_base*/
    0,          /*tp_dict*/
    0,          /*tp_descr_get*/
    0,          /*tp_descr_set*/
    0,          /*tp_dictoffset*/
    replicationConnection_init, /*tp_init*/
    0,          /*tp_alloc*/
    0,          /*tp_new*/
};

PyObject *replicationPhysicalConst;
PyObject *replicationLogicalConst;
