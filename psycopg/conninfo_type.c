/* conninfo_type.c - present information about the libpq connection
 *
 * Copyright (C) 2018  Daniele Varrazzo <daniele.varrazzo@gmail.com>
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

#include "psycopg/conninfo.h"


static const char connInfoType_doc[] =
"Details about the native PostgreSQL database connection.\n"
"\n"
"This class exposes several `informative functions`__ about the status\n"
"of the libpq connection.\n"
"\n"
"Objects of this class are exposed as the `connection.info` attribute.\n"
"\n"
".. __: https://www.postgresql.org/docs/current/static/libpq-status.html";


static const char dbname_doc[] =
"The database name of the connection.\n"
"\n"
"Wrapper for the `PQdb()`__ function.\n"
"\n"
".. __: https://www.postgresql.org/docs/current/static/libpq-status.html"
    "#LIBPQ-PQDB";

static PyObject *
dbname_get(connInfoObject *self)
{
    const char *val;

    val = PQdb(self->conn->pgconn);
    if (!val) {
        Py_RETURN_NONE;
    }
    return conn_text_from_chars(self->conn, val);
}


static const char user_doc[] =
"The user name of the connection.\n"
"\n"
"Wrapper for the `PQuser()`__ function.\n"
"\n"
".. __: https://www.postgresql.org/docs/current/static/libpq-status.html"
    "#LIBPQ-PQUSER";

static PyObject *
user_get(connInfoObject *self)
{
    const char *val;

    val = PQuser(self->conn->pgconn);
    if (!val) {
        Py_RETURN_NONE;
    }
    return conn_text_from_chars(self->conn, val);
}


static const char password_doc[] =
"The password of the connection.\n"
"\n"
".. seealso:: libpq docs for `PQpass()`__ for details.\n"
".. __: https://www.postgresql.org/docs/current/static/libpq-status.html"
    "#LIBPQ-PQPASS";

static PyObject *
password_get(connInfoObject *self)
{
    const char *val;

    val = PQpass(self->conn->pgconn);
    if (!val) {
        Py_RETURN_NONE;
    }
    return conn_text_from_chars(self->conn, val);
}


static const char host_doc[] =
"The server host name of the connection.\n"
"\n"
"This can be a host name, an IP address, or a directory path if the\n"
"connection is via Unix socket. (The path case can be distinguished\n"
"because it will always be an absolute path, beginning with ``/``.)\n"
"\n"
".. seealso:: libpq docs for `PQhost()`__ for details.\n"
".. __: https://www.postgresql.org/docs/current/static/libpq-status.html"
    "#LIBPQ-PQHOST";

static PyObject *
host_get(connInfoObject *self)
{
    const char *val;

    val = PQhost(self->conn->pgconn);
    if (!val) {
        Py_RETURN_NONE;
    }
    return conn_text_from_chars(self->conn, val);
}


static const char port_doc[] =
"The port of the connection.\n"
"\n"
".. seealso:: libpq docs for `PQport()`__ for details.\n"
".. __: https://www.postgresql.org/docs/current/static/libpq-status.html"
    "#LIBPQ-PQPORT";

static PyObject *
port_get(connInfoObject *self)
{
    const char *val;

    val = PQport(self->conn->pgconn);
    if (!val || !val[0]) {
        Py_RETURN_NONE;
    }
    return PyInt_FromString((char *)val, NULL, 10);
}


static const char options_doc[] =
"The command-line options passed in the the connection request.\n"
"\n"
".. seealso:: libpq docs for `PQoptions()`__ for details.\n"
".. __: https://www.postgresql.org/docs/current/static/libpq-status.html"
    "#LIBPQ-PQOPTIONS";

static PyObject *
options_get(connInfoObject *self)
{
    const char *val;

    val = PQoptions(self->conn->pgconn);
    if (!val) {
        Py_RETURN_NONE;
    }
    return conn_text_from_chars(self->conn, val);
}


static const char status_doc[] =
"Return the status of the connection.\n"
"\n"
".. seealso:: libpq docs for `PQstatus()`__ for details.\n"
".. __: https://www.postgresql.org/docs/current/static/libpq-status.html"
    "#LIBPQ-PQSTATUS";

static PyObject *
status_get(connInfoObject *self)
{
    ConnStatusType val;

    val = PQstatus(self->conn->pgconn);
    return PyInt_FromLong((long)val);
}


static struct PyGetSetDef connInfoObject_getsets[] = {
    { "dbname", (getter)dbname_get, NULL, (char *)dbname_doc },
    { "user", (getter)user_get, NULL, (char *)user_doc },
    { "password", (getter)password_get, NULL, (char *)password_doc },
    { "host", (getter)host_get, NULL, (char *)host_doc },
    { "port", (getter)port_get, NULL, (char *)port_doc },
    { "options", (getter)options_get, NULL, (char *)options_doc },
    { "status", (getter)status_get, NULL, (char *)status_doc },
    {NULL}
};

/* initialization and finalization methods */

static PyObject *
conninfo_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    return type->tp_alloc(type, 0);
}

static int
conninfo_init(connInfoObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *conn = NULL;

    if (!PyArg_ParseTuple(args, "O", &conn))
        return -1;

    if (!PyObject_TypeCheck(conn, &connectionType)) {
        PyErr_SetString(PyExc_TypeError,
            "The argument must be a psycopg2 connection");
        return -1;
    }

    Py_INCREF(conn);
    self->conn = (connectionObject *)conn;
    return 0;
}

static void
conninfo_dealloc(connInfoObject* self)
{
    Py_CLEAR(self->conn);
    Py_TYPE(self)->tp_free((PyObject *)self);
}


/* object type */

PyTypeObject connInfoType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "psycopg2.extensions.ConnectionInfo",
    sizeof(connInfoObject), 0,
    (destructor)conninfo_dealloc, /*tp_dealloc*/
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
    connInfoType_doc, /*tp_doc*/
    0,          /*tp_traverse*/
    0,          /*tp_clear*/
    0,          /*tp_richcompare*/
    0,          /*tp_weaklistoffset*/
    0,          /*tp_iter*/
    0,          /*tp_iternext*/
    0,          /*tp_methods*/
    0,          /*tp_members*/
    connInfoObject_getsets, /*tp_getset*/
    0,          /*tp_base*/
    0,          /*tp_dict*/
    0,          /*tp_descr_get*/
    0,          /*tp_descr_set*/
    0,          /*tp_dictoffset*/
    (initproc)conninfo_init, /*tp_init*/
    0,          /*tp_alloc*/
    conninfo_new, /*tp_new*/
};
