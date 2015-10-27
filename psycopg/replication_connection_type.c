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
    PyObject *dsn = NULL;
    PyObject *async = NULL;
    PyObject *tmp = NULL;
    const char *repl = NULL;
    int ret = -1;

    Py_XINCREF(args);
    Py_XINCREF(kwargs);

    /* dsn, async, replication_type */
    if (!(dsn = parse_arg(0, "dsn", Py_None, args, kwargs))) { goto exit; }
    if (!(async = parse_arg(1, "async", Py_False, args, kwargs))) { goto exit; }
    if (!(tmp = parse_arg(2, "replication_type", Py_None, args, kwargs))) { goto exit; }

    if (tmp == replicationPhysicalConst) {
        self->type = REPLICATION_PHYSICAL;
        repl = "true";
    } else if (tmp == replicationLogicalConst) {
        self->type = REPLICATION_LOGICAL;
        repl = "database";
    } else {
        PyErr_SetString(PyExc_TypeError,
                        "replication_type must be either REPLICATION_PHYSICAL or REPLICATION_LOGICAL");
        goto exit;
    }
    Py_DECREF(tmp);
    tmp = NULL;

    if (dsn != Py_None) {
        if (kwargs && PyMapping_Size(kwargs) > 0) {
            PyErr_SetString(PyExc_TypeError, "both dsn and parameters given");
            goto exit;
        } else {
            if (!(tmp = PyTuple_Pack(1, dsn))) { goto exit; }

            Py_XDECREF(kwargs);
            if (!(kwargs = psyco_parse_dsn(NULL, tmp, NULL))) { goto exit; }
        }
    } else {
        if (!(kwargs && PyMapping_Size(kwargs) > 0)) {
            PyErr_SetString(PyExc_TypeError, "missing dsn and no parameters");
            goto exit;
        }
    }

    if (!PyMapping_HasKeyString(kwargs, "replication")) {
        PyMapping_SetItemString(kwargs, "replication", Text_FromUTF8(repl));
    }

    Py_DECREF(dsn);
    if (!(dsn = psyco_make_dsn(NULL, NULL, kwargs))) { goto exit; }

    Py_DECREF(args);
    Py_DECREF(kwargs);
    kwargs = NULL;
    if (!(args = PyTuple_Pack(2, dsn, async))) { goto exit; }

    if ((ret = connectionType.tp_init(obj, args, NULL)) < 0) { goto exit; }

    self->conn.autocommit = 1;
    self->conn.cursor_factory = (PyObject *)&replicationCursorType;
    Py_INCREF(self->conn.cursor_factory);

exit:
    Py_XDECREF(tmp);
    Py_XDECREF(dsn);
    Py_XDECREF(async);
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
