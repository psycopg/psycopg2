/* utils.c - miscellaneous utility functions
 *
 * Copyright (C) 2008-2010 Federico Di Gregorio <fog@debian.org>
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

#include "psycopg/connection.h"
#include "psycopg/pgtypes.h"

#include <string.h>
#include <stdlib.h>

/* Escape a string for sql inclusion.
 *
 * The function must be called holding the GIL.
 *
 * Return a pointer to a new string on the Python heap on success, else NULL
 * and set an exception. The returned string includes quotes and leading E if
 * needed.
 *
 * `len` is optional: if < 0 it will be calculated.
 *
 * If tolen is set, it will contain the length of the escaped string,
 * including quotes.
 */
char *
psycopg_escape_string(connectionObject *conn, const char *from, Py_ssize_t len,
                       char *to, Py_ssize_t *tolen)
{
    Py_ssize_t ql;
    int eq = (conn && (conn->equote)) ? 1 : 0;

    if (len < 0) {
        len = strlen(from);
    } else if (strchr(from, '\0') != from + len) {
        PyErr_Format(PyExc_ValueError, "A string literal cannot contain NUL (0x00) characters.");

	return NULL;
    }

    if (to == NULL) {
        to = (char *)PyMem_Malloc((len * 2 + 4) * sizeof(char));
        if (to == NULL) {
            PyErr_NoMemory();
            return NULL;
        }
    }

    {
            int err;
            if (conn && conn->pgconn)
                ql = PQescapeStringConn(conn->pgconn, to+eq+1, from, len, &err);
            else
                ql = PQescapeString(to+eq+1, from, len);
    }

    if (eq) {
        to[0] = 'E';
        to[1] = to[ql+2] = '\'';
        to[ql+3] = '\0';
    }
    else {
        to[0] = to[ql+1] = '\'';
        to[ql+2] = '\0';
    }

    if (tolen)
        *tolen = ql+eq+2;

    return to;
}

/* Escape a string for inclusion in a query as identifier.
 *
 * 'len' is optional: if < 0 it will be calculated.
 *
 * Return a string allocated by Postgres: free it using PQfreemem
 * In case of error set a Python exception.
 */
char *
psycopg_escape_identifier(connectionObject *conn, const char *str, Py_ssize_t len)
{
    char *rv = NULL;

    if (!conn || !conn->pgconn) {
        PyErr_SetString(InterfaceError, "connection not valid");
        goto exit;
    }

    if (len < 0) { len = strlen(str); }

    rv = PQescapeIdentifier(conn->pgconn, str, len);
    if (!rv) {
        char *msg;
        msg = PQerrorMessage(conn->pgconn);
        if (!msg || !msg[0]) {
            msg = "no message provided";
        }
        PyErr_Format(InterfaceError, "failed to escape identifier: %s", msg);
    }

exit:
    return rv;
}


/* Duplicate a string.
 *
 * Allocate a new buffer on the Python heap containing the new string.
 * 'len' is optional: if < 0 the length is calculated.
 *
 * Store the return in 'to' and return 0 in case of success, else return -1
 * and raise an exception.
 *
 * If from is null, store null into to.
 */
RAISES_NEG int
psycopg_strdup(char **to, const char *from, Py_ssize_t len)
{
    if (!from) {
        *to = NULL;
        return 0;
    }
    if (len < 0) { len = strlen(from); }
    if (!(*to = PyMem_Malloc(len + 1))) {
        PyErr_NoMemory();
        return -1;
    }
    strcpy(*to, from);
    return 0;
}

/* Ensure a Python object is a bytes string.
 *
 * Useful when a char * is required out of it.
 *
 * The function is ref neutral: steals a ref from obj and adds one to the
 * return value. This also means that you shouldn't call the function on a
 * borrowed ref, if having the object unallocated is not what you want.
 *
 * It is safe to call the function on NULL.
 */
STEALS(1) PyObject *
psycopg_ensure_bytes(PyObject *obj)
{
    PyObject *rv = NULL;
    if (!obj) { return NULL; }

    if (PyUnicode_CheckExact(obj)) {
        rv = PyUnicode_AsUTF8String(obj);
        Py_DECREF(obj);
    }
    else if (Bytes_CheckExact(obj)) {
        rv = obj;
    }
    else {
        PyErr_Format(PyExc_TypeError,
            "Expected bytes or unicode string, got %s instead",
            Py_TYPE(obj)->tp_name);
        Py_DECREF(obj);  /* steal the ref anyway */
    }

    return rv;
}

/* Take a Python object and return text from it.
 *
 * On Py3 this means converting bytes to unicode. On Py2 bytes are fine.
 *
 * The function is ref neutral: steals a ref from obj and adds one to the
 * return value.  It is safe to call it on NULL.
 */
STEALS(1) PyObject *
psycopg_ensure_text(PyObject *obj)
{
#if PY_MAJOR_VERSION < 3
    return obj;
#else
    if (obj) {
        /* bytes to unicode in Py3 */
        PyObject *rv = PyUnicode_FromEncodedObject(obj, "utf8", "replace");
        Py_DECREF(obj);
        return rv;
    }
    else {
        return NULL;
    }
#endif
}

/* Check if a file derives from TextIOBase.
 *
 * Return 1 if it does, else 0, -1 on errors.
 */
int
psycopg_is_text_file(PyObject *f)
{
    /* NULL before any call.
     * then io.TextIOBase if exists, else None. */
    static PyObject *base;

    /* Try to import os.TextIOBase */
    if (NULL == base) {
        PyObject *m;
        Dprintf("psycopg_is_text_file: importing io.TextIOBase");
        if (!(m = PyImport_ImportModule("io"))) {
            Dprintf("psycopg_is_text_file: io module not found");
            PyErr_Clear();
            Py_INCREF(Py_None);
            base = Py_None;
        }
        else {
            if (!(base = PyObject_GetAttrString(m, "TextIOBase"))) {
                Dprintf("psycopg_is_text_file: io.TextIOBase not found");
                PyErr_Clear();
                Py_INCREF(Py_None);
                base = Py_None;
            }
        }
        Py_XDECREF(m);
    }

    if (base != Py_None) {
        return PyObject_IsInstance(f, base);
    } else {
        return 0;
    }
}

/* Make a dict out of PQconninfoOption array */
PyObject *
psycopg_dict_from_conninfo_options(PQconninfoOption *options, int include_password)
{
    PyObject *dict, *res = NULL;
    PQconninfoOption *o;

    if (!(dict = PyDict_New())) { goto exit; }
    for (o = options; o->keyword != NULL; o++) {
        if (o->val != NULL &&
            (include_password || strcmp(o->keyword, "password") != 0)) {
            PyObject *value;
            if (!(value = Text_FromUTF8(o->val))) { goto exit; }
            if (PyDict_SetItemString(dict, o->keyword, value) != 0) {
                Py_DECREF(value);
                goto exit;
            }
            Py_DECREF(value);
        }
    }

    res = dict;
    dict = NULL;

exit:
    Py_XDECREF(dict);

    return res;
}


/* Make a connection string out of a string and a dictionary of arguments.
 *
 * Helper to call psycopg2.extensions.make_dns()
 */
PyObject *
psycopg_make_dsn(PyObject *dsn, PyObject *kwargs)
{
    PyObject *ext = NULL, *make_dsn = NULL;
    PyObject *args = NULL, *rv = NULL;

    if (!(ext = PyImport_ImportModule("psycopg2.extensions"))) { goto exit; }
    if (!(make_dsn = PyObject_GetAttrString(ext, "make_dsn"))) { goto exit; }

    if (!(args = PyTuple_Pack(1, dsn))) { goto exit; }
    rv = PyObject_Call(make_dsn, args, kwargs);

exit:
    Py_XDECREF(args);
    Py_XDECREF(make_dsn);
    Py_XDECREF(ext);

    return rv;
}

/* Convert a C string into Python Text using a specified codec.
 *
 * The codec is the python function codec.getdecoder(enc). It is only used on
 * Python 3 to return unicode: in Py2 the function returns a string.
 *
 * len is optional: use -1 to have it calculated by the function.
 */
PyObject *
psycopg_text_from_chars_safe(const char *str, Py_ssize_t len, PyObject *decoder)
{
#if PY_MAJOR_VERSION < 3

    if (!str) { Py_RETURN_NONE; }

    if (len < 0) { len = strlen(str); }

    return PyString_FromStringAndSize(str, len);

#else

    static PyObject *replace = NULL;
    PyObject *rv = NULL;
    PyObject *b = NULL;
    PyObject *t = NULL;

    if (!str) { Py_RETURN_NONE; }

    if (len < 0) { len = strlen(str); }

    if (decoder) {
        if (!replace) {
            if (!(replace = PyUnicode_FromString("replace"))) { goto exit; }
        }
        if (!(b = PyBytes_FromStringAndSize(str, len))) { goto exit; }
        if (!(t = PyObject_CallFunctionObjArgs(decoder, b, replace, NULL))) {
            goto exit;
        }

        if (!(rv = PyTuple_GetItem(t, 0))) { goto exit; }
        Py_INCREF(rv);
    }
    else {
        rv = PyUnicode_DecodeASCII(str, len, "replace");
    }

exit:
    Py_XDECREF(t);
    Py_XDECREF(b);
    return rv;

#endif
}
