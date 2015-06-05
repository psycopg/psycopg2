/* replication_message_type.c - python interface to ReplcationMessage objects
 *
 * Copyright (C) 2003-2015 Federico Di Gregorio <fog@debian.org>
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

#include "psycopg/replication_message.h"


static PyObject *
replmsg_repr(replicationMessageObject *self)
{
    return PyString_FromFormat(
        "<replicationMessage object at %p; data_start: "XLOGFMTSTR"; wal_end: "XLOGFMTSTR">",
        self, XLOGFMTARGS(self->data_start), XLOGFMTARGS(self->wal_end));
}

static int
replmsg_init(PyObject *obj, PyObject *args, PyObject *kwargs)
{
    replicationMessageObject *self = (replicationMessageObject*) obj;

    if (!PyArg_ParseTuple(args, "O", &self->payload))
        return -1;
    Py_XINCREF(self->payload);

    self->data_start = 0;
    self->wal_end = 0;

    return 0;
}

static int
replmsg_clear(PyObject *self)
{
    Py_CLEAR(((replicationMessageObject*) self)->payload);
    return 0;
}

static void
replmsg_dealloc(PyObject* obj)
{
    replmsg_clear(obj);
}


#define OFFSETOF(x) offsetof(replicationMessageObject, x)

/* object member list */

static struct PyMemberDef replicationMessageObject_members[] = {
    {"payload", T_OBJECT, OFFSETOF(payload), READONLY,
        "TODO"},
    {"data_start", T_ULONGLONG, OFFSETOF(data_start), READONLY,
        "TODO"},
    {"wal_end", T_ULONGLONG, OFFSETOF(wal_end), READONLY,
        "TODO"},
    {NULL}
};

/* object type */

#define replicationMessageType_doc \
"A database replication message."

PyTypeObject replicationMessageType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "psycopg2.extensions.ReplicationMessage",
    sizeof(replicationMessageObject), 0,
    replmsg_dealloc, /*tp_dealloc*/
    0,          /*tp_print*/
    0,          /*tp_getattr*/
    0,          /*tp_setattr*/
    0,          /*tp_compare*/
    (reprfunc)replmsg_repr, /*tp_repr*/
    0,          /*tp_as_number*/
    0,          /*tp_as_sequence*/
    0,          /*tp_as_mapping*/
    0,          /*tp_hash */
    0,          /*tp_call*/
    0,          /*tp_str*/
    0,          /*tp_getattro*/
    0,          /*tp_setattro*/
    0,          /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
                /*tp_flags*/
    replicationMessageType_doc, /*tp_doc*/
    0,          /*tp_traverse*/
    replmsg_clear, /*tp_clear*/
    0,          /*tp_richcompare*/
    0, /*tp_weaklistoffset*/
    0, /*tp_iter*/
    0, /*tp_iternext*/
    0, /*tp_methods*/
    replicationMessageObject_members, /*tp_members*/
    0, /*tp_getset*/
    0, /*tp_base*/
    0,          /*tp_dict*/
    0,          /*tp_descr_get*/
    0,          /*tp_descr_set*/
    0,          /*tp_dictoffset*/
    replmsg_init, /*tp_init*/
    0,          /*tp_alloc*/
    PyType_GenericNew, /*tp_new*/
};
