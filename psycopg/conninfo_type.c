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
".. seealso:: libpq docs for `PQdb()`__ for details.\n"
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
".. seealso:: libpq docs for `PQuser()`__ for details.\n"
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
":type: `!int`\n"
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
"The status of the connection.\n"
"\n"
":type: `!int`\n"
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


static const char transaction_status_doc[] =
"The current in-transaction status of the connection.\n"
"\n"
"Symbolic constants for the values are defined in the module\n"
"`psycopg2.extensions`: see :ref:`transaction-status-constants` for the\n"
"available values.\n"
"\n"
":type: `!int`\n"
"\n"
".. seealso:: libpq docs for `PQtransactionStatus()`__ for details.\n"
".. __: https://www.postgresql.org/docs/current/static/libpq-status.html"
    "#LIBPQ-PQTRANSACTIONSTATUS";

static PyObject *
transaction_status_get(connInfoObject *self)
{
    PGTransactionStatusType val;

    val = PQtransactionStatus(self->conn->pgconn);
    return PyInt_FromLong((long)val);
}


static const char protocol_version_doc[] =
"The frontend/backend protocol being used.\n"
"\n"
":type: `!int`\n"
"\n"
".. seealso:: libpq docs for `PQprotocolVersion()`__ for details.\n"
".. __: https://www.postgresql.org/docs/current/static/libpq-status.html"
    "#LIBPQ-PQPROTOCOLVERSION";

static PyObject *
protocol_version_get(connInfoObject *self)
{
    int val;

    val = PQprotocolVersion(self->conn->pgconn);
    return PyInt_FromLong((long)val);
}


static const char server_version_doc[] =
"Returns an integer representing the server version.\n"
"\n"
":type: `!int`\n"
"\n"
".. seealso:: libpq docs for `PQserverVersion()`__ for details.\n"
".. __: https://www.postgresql.org/docs/current/static/libpq-status.html"
    "#LIBPQ-PQSERVERVERSION";

static PyObject *
server_version_get(connInfoObject *self)
{
    int val;

    val = PQserverVersion(self->conn->pgconn);
    return PyInt_FromLong((long)val);
}


static const char error_message_doc[] =
"The error message most recently generated by an operation on the connection.\n"
"\n"
"`!None` if there is no current message.\n"
"\n"
".. seealso:: libpq docs for `PQerrorMessage()`__ for details.\n"
".. __: https://www.postgresql.org/docs/current/static/libpq-status.html"
    "#LIBPQ-PQERRORMESSAGE";

static PyObject *
error_message_get(connInfoObject *self)
{
    const char *val;

    val = PQerrorMessage(self->conn->pgconn);
    if (!val || !val[0]) {
        Py_RETURN_NONE;
    }
    return conn_text_from_chars(self->conn, val);
}


static const char socket_doc[] =
"The file descriptor number of the connection socket to the server.\n"
"\n"
":type: `!int`\n"
"\n"
".. seealso:: libpq docs for `PQsocket()`__ for details.\n"
".. __: https://www.postgresql.org/docs/current/static/libpq-status.html"
    "#LIBPQ-PQSOCKET";

static PyObject *
socket_get(connInfoObject *self)
{
    int val;

    val = PQsocket(self->conn->pgconn);
    return PyInt_FromLong((long)val);
}


static const char backend_pid_doc[] =
"The process ID (PID) of the backend process handling this connection.\n"
"\n"
":type: `!int`\n"
"\n"
".. seealso:: libpq docs for `PQbackendPID()`__ for details.\n"
".. __: https://www.postgresql.org/docs/current/static/libpq-status.html"
    "#LIBPQ-PQBACKENDPID";

static PyObject *
backend_pid_get(connInfoObject *self)
{
    int val;

    val = PQbackendPID(self->conn->pgconn);
    return PyInt_FromLong((long)val);
}


static const char needs_password_doc[] =
"The connection authentication method required a password, but none was available.\n"
"\n"
":type: `!bool`\n"
"\n"
".. seealso:: libpq docs for `PQconnectionNeedsPassword()`__ for details.\n"
".. __: https://www.postgresql.org/docs/current/static/libpq-status.html"
    "#LIBPQ-PQCONNECTIONNEEDSPASSWORD";

static PyObject *
needs_password_get(connInfoObject *self)
{
    PyObject *rv;

    rv = PQconnectionNeedsPassword(self->conn->pgconn) ? Py_True : Py_False;
    Py_INCREF(rv);
    return rv;
}


static const char used_password_doc[] =
"The connection authentication method used a password.\n"
"\n"
":type: `!bool`\n"
"\n"
".. seealso:: libpq docs for `PQconnectionUsedPassword()`__ for details.\n"
".. __: https://www.postgresql.org/docs/current/static/libpq-status.html"
    "#LIBPQ-PQCONNECTIONUSEDPASSWORD";

static PyObject *
used_password_get(connInfoObject *self)
{
    PyObject *rv;

    rv = PQconnectionUsedPassword(self->conn->pgconn) ? Py_True : Py_False;
    Py_INCREF(rv);
    return rv;
}


static const char ssl_in_use_doc[] =
"`!True` if the connection uses SSL, `!False` if not.\n"
"\n"
":type: `!bool`\n"
"\n"
".. seealso:: libpq docs for `PQsslInUse()`__ for details.\n"
".. __: https://www.postgresql.org/docs/current/static/libpq-status.html"
    "#LIBPQ-PQSSLINUSE";

static PyObject *
ssl_in_use_get(connInfoObject *self)
{
    PyObject *rv;

    rv = PQsslInUse(self->conn->pgconn) ? Py_True : Py_False;
    Py_INCREF(rv);
    return rv;
}


static struct PyGetSetDef connInfoObject_getsets[] = {
    { "dbname", (getter)dbname_get, NULL, (char *)dbname_doc },
    { "user", (getter)user_get, NULL, (char *)user_doc },
    { "password", (getter)password_get, NULL, (char *)password_doc },
    { "host", (getter)host_get, NULL, (char *)host_doc },
    { "port", (getter)port_get, NULL, (char *)port_doc },
    { "options", (getter)options_get, NULL, (char *)options_doc },
    { "status", (getter)status_get, NULL, (char *)status_doc },
    { "transaction_status", (getter)transaction_status_get, NULL,
        (char *)transaction_status_doc },
    { "protocol_version", (getter)protocol_version_get, NULL,
        (char *)protocol_version_doc },
    { "server_version", (getter)server_version_get, NULL,
        (char *)server_version_doc },
    { "error_message", (getter)error_message_get, NULL,
        (char *)error_message_doc },
    { "socket", (getter)socket_get, NULL, (char *)socket_doc },
    { "backend_pid", (getter)backend_pid_get, NULL, (char *)backend_pid_doc },
    { "used_password", (getter)used_password_get, NULL,
        (char *)used_password_doc },
    { "needs_password", (getter)needs_password_get, NULL,
        (char *)needs_password_doc },
    { "ssl_in_use", (getter)ssl_in_use_get, NULL,
        (char *)ssl_in_use_doc },
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
