/* notify_type.c - python interface to Notify objects
 *
 * Copyright (C) 2010  Daniele Varrazzo <daniele.varrazzo@gmail.com>
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

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <structmember.h>

#define PSYCOPG_MODULE
#include "psycopg/config.h"
#include "psycopg/python.h"
#include "psycopg/psycopg.h"
#include "psycopg/notify.h"

static const char notify_doc[] =
    "A notification received from the backend.\n\n"
    "`!Notify` instances are made available upon reception on the\n"
    "`~connection.notifies` member of the listening connection. The object\n"
    "can be also accessed as a 2 items tuple returning the members\n"
    ":samp:`({pid},{channel})` for backward compatibility.\n\n"
    "See :ref:`async-notify` for details.";

static const char pid_doc[] =
    "The ID of the backend process that sent the notification.\n\n"
    "Note: if the sending session was handled by Psycopg, you can use\n"
    "`~connection.get_backend_pid()` to know its PID.";

static const char channel_doc[] =
    "The name of the channel to which the notification was sent.";

static const char payload_doc[] =
    "The payload message of the notification.\n\n"
    "Attaching a payload to a notification is only available since\n"
    "PostgreSQL 9.0: for notifications received from previous versions\n"
    "of the server this member is always the empty string.";

static PyMemberDef notify_members[] = {
    { "pid", T_OBJECT, offsetof(NotifyObject, pid), RO, (char *)pid_doc },
    { "channel", T_OBJECT, offsetof(NotifyObject, channel), RO, (char *)channel_doc },
    { "payload", T_OBJECT, offsetof(NotifyObject, payload), RO, (char *)payload_doc },
    { NULL }
};

static PyObject *
notify_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    NotifyObject *self = (NotifyObject *)type->tp_alloc(type, 0);

    return (PyObject *)self;
}

static int
notify_init(NotifyObject *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = {"pid", "channel", "payload", NULL};
    PyObject *pid = NULL, *channel = NULL, *payload = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OO|O", kwlist,
                                     &pid, &channel, &payload)) {
        return -1;
    }

    if (!payload) {
        payload = Py_None;
    }

    Py_CLEAR(self->pid);
    Py_INCREF(pid);
    self->pid = pid;

    Py_CLEAR(self->channel);
    Py_INCREF(channel);
    self->channel = channel;

    Py_CLEAR(self->payload);
    Py_INCREF(payload);
    self->payload = payload;

    return 0;
}

static int
notify_traverse(NotifyObject *self, visitproc visit, void *arg)
{
    Py_VISIT(self->pid);
    Py_VISIT(self->channel);
    Py_VISIT(self->payload);
    return 0;
}

static void
notify_dealloc(NotifyObject *self)
{
    Py_CLEAR(self->pid);
    Py_CLEAR(self->channel);
    Py_CLEAR(self->payload);

    self->ob_type->tp_free((PyObject *)self);
}

static void
notify_del(PyObject *self)
{
    PyObject_GC_Del(self);
}

static PyObject*
notify_repr(NotifyObject *self)
{
    PyObject *rv = NULL;
    PyObject *format = NULL;
    PyObject *args = NULL;

    if (!(format = PyString_FromString("Notify(%r, %r, %r)"))) {
        goto exit;
    }

    if (!(args = PyTuple_New(3))) { goto exit; }
    Py_INCREF(self->pid);
    PyTuple_SET_ITEM(args, 0, self->pid);
    Py_INCREF(self->channel);
    PyTuple_SET_ITEM(args, 1, self->channel);
    Py_INCREF(self->payload);
    PyTuple_SET_ITEM(args, 2, self->payload);

    rv = PyString_Format(format, args);

exit:
    Py_XDECREF(args);
    Py_XDECREF(format);

    return rv;
}

/* Notify can be accessed as a 2 items tuple for backward compatibility */

static Py_ssize_t
notify_len(NotifyObject *self)
{
    return 2;
}

static PyObject *
notify_getitem(NotifyObject *self, Py_ssize_t item)
{
    if (item < 0)
        item += 2;

    switch (item) {
    case 0:
        Py_INCREF(self->pid);
        return self->pid;
    case 1:
        Py_INCREF(self->channel);
        return self->channel;
    default:
        PyErr_SetString(PyExc_IndexError, "index out of range");
        return NULL;
    }
}

static PySequenceMethods notify_sequence = {
    (lenfunc)notify_len,          /* sq_length */
    0,                         /* sq_concat */
    0,                         /* sq_repeat */
    (ssizeargfunc)notify_getitem, /* sq_item */
    0,                         /* sq_slice */
    0,                         /* sq_ass_item */
    0,                         /* sq_ass_slice */
    0,                         /* sq_contains */
    0,                         /* sq_inplace_concat */
    0,                         /* sq_inplace_repeat */
};


PyTypeObject NotifyType = {
    PyObject_HEAD_INIT(NULL)
    0,
    "psycopg2.extensions.Notify",
    sizeof(NotifyObject),
    0,
    (destructor)notify_dealloc, /* tp_dealloc */
    0,          /*tp_print*/

    0,          /*tp_getattr*/
    0,          /*tp_setattr*/

    0,          /*tp_compare*/

    (reprfunc)notify_repr, /*tp_repr*/
    0,          /*tp_as_number*/
    &notify_sequence, /*tp_as_sequence*/
    0,          /*tp_as_mapping*/
    0,          /*tp_hash */

    0,          /*tp_call*/
    0,          /*tp_str*/

    0,          /*tp_getattro*/
    0,          /*tp_setattro*/
    0,          /*tp_as_buffer*/

    Py_TPFLAGS_DEFAULT|Py_TPFLAGS_HAVE_GC, /*tp_flags*/
    notify_doc, /*tp_doc*/

    (traverseproc)notify_traverse, /*tp_traverse*/
    0,          /*tp_clear*/

    0,          /*tp_richcompare*/
    0,          /*tp_weaklistoffset*/

    0,          /*tp_iter*/
    0,          /*tp_iternext*/

    /* Attribute descriptor and subclassing stuff */

    0,          /*tp_methods*/
    notify_members, /*tp_members*/
    0,          /*tp_getset*/
    0,          /*tp_base*/
    0,          /*tp_dict*/

    0,          /*tp_descr_get*/
    0,          /*tp_descr_set*/
    0,          /*tp_dictoffset*/

    (initproc)notify_init, /*tp_init*/
    0, /*tp_alloc  will be set to PyType_GenericAlloc in module init*/
    notify_new, /*tp_new*/
    (freefunc)notify_del, /*tp_free  Low-level free-memory routine */
    0,          /*tp_is_gc For PyObject_IS_GC */
    0,          /*tp_bases*/
    0,          /*tp_mro method resolution order */
    0,          /*tp_cache*/
    0,          /*tp_subclasses*/
    0           /*tp_weaklist*/
};


