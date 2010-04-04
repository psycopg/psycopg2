/* green.c - cooperation with coroutine libraries.
 *
 * Copyright (C) 2010 Daniele Varrazzo <daniele.varrazzo@gmail.com>
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
#include "psycopg/config.h"
#include "psycopg/python.h"
#include "psycopg/psycopg.h"
#include "psycopg/green.h"
#include "psycopg/connection.h"

HIDDEN PyObject *wait_callback = NULL;

PyObject *have_wait_callback(void);

/* Register a callback function to block waiting for data.
 *
 * The function is exported by the _psycopg module.
 */
PyObject *
psyco_set_wait_callback(PyObject *self, PyObject *obj)
{
    Py_XDECREF(wait_callback);

    if (obj != Py_None) {
        wait_callback = obj;
        Py_INCREF(obj);
    }
    else {
        wait_callback = NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


/* Return the currently registered wait callback function.
 *
 * The function is exported by the _psycopg module.
 */
PyObject *
psyco_get_wait_callback(PyObject *self, PyObject *obj)
{
    PyObject *ret;

    ret = wait_callback;
    if (!ret) {
        ret = Py_None;
    }

    Py_INCREF(ret);
    return ret;
}


/* Return nonzero if a wait callback should be called. */
int
psyco_green()
{
#ifdef PSYCOPG_EXTENSIONS
    return (NULL != wait_callback);
#else
    return 0;
#endif
}

/* Return the wait callback if available.
 *
 * If not available, set a Python exception and return.
 *
 * The function returns a new reference: decref after use.
 */
PyObject *
have_wait_callback()
{
    PyObject *cb;

    cb = wait_callback;
    if (!cb) {
        PyErr_SetString(OperationalError, "wait callback not available");
        return NULL;
    }
    Py_INCREF(cb);
    return cb;
}

/* Block waiting for data available in an async connection.
 *
 * This function assumes `wait_callback` to be available:
 * raise `InterfaceError` if it is not. Use `psyco_green()` to check if
 * the function is to be called.
 *
 * The function returns the return value of the called function.
 */
PyObject *
psyco_wait(connectionObject *conn)
{
    PyObject *rv;
    PyObject *cb;

    Dprintf("psyco_wait");
    if (!(cb = have_wait_callback())) {
        return NULL;
    }

    rv = PyObject_CallFunctionObjArgs(cb, conn, NULL);
    Py_DECREF(cb);

    return rv;
}

/* Replacement for PQexec using the user-provided wait function.
 *
 * The function should be called helding the connection lock, and
 * the GIL because some Python code is expected to be called.
 *
 * If PGresult is NULL, there may have been either a libpq error
 * or an exception raised by Python code: before raising an exception
 * check if there is already one using `PyErr_Occurred()` */
PGresult *
psyco_exec_green(connectionObject *conn, const char *command)
{
    PGconn *pgconn = conn->pgconn;
    PGresult *result = NULL, *res;
    PyObject *cb, *pyrv;

    if (!(cb = have_wait_callback())) {
        goto end;
    }

    /* Send the query asynchronously */
    Dprintf("psyco_exec_green: sending query async");
    if (0 == PQsendQuery(pgconn, command)) {
        Dprintf("psyco_exec_green: PQsendQuery returned 0");
        goto clear;
    }

    /* Ensure the query reached the server. */
    conn->async_status = ASYNC_WRITE;

    pyrv = PyObject_CallFunctionObjArgs(cb, conn, NULL);
    if (!pyrv) {
        Dprintf("psyco_exec_green: error in callback sending query");
        goto clear;
    }
    Py_DECREF(pyrv);

    /* Loop reading data using the user-provided wait function */
    conn->async_status = ASYNC_READ;

    pyrv = PyObject_CallFunctionObjArgs(cb, conn, NULL);
    if (!pyrv) {
        Dprintf("psyco_exec_green: error in callback reading result");
        goto clear;
    }
    Py_DECREF(pyrv);

    /* Now we can read the data without fear of blocking.
     * Read until PQgetResult gives a NULL */
    while (NULL != (res = PQgetResult(pgconn))) {
        if (result) {
            /* TODO too bad: we are discarding results from all the queries
             * except the last. We could have populated `nextset()` with it
             * but it would be an incompatible change (apps currently issue
             * groups of queries expecting to receive the last result: they
             * would start receiving the first instead). */
            PQclear(result);
        }
        result = res;
    }

clear:
    Py_DECREF(cb);
end:
    return result;
}

