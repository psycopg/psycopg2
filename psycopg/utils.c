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

char *
psycopg_escape_string(PyObject *obj, const char *from, Py_ssize_t len,
                       char *to, Py_ssize_t *tolen)
{
    Py_ssize_t ql;
    connectionObject *conn = (connectionObject*)obj;
    int eq = (conn && (conn->equote)) ? 1 : 0;   

    if (len == 0)
        len = strlen(from);
    
    if (to == NULL) {
        to = (char *)PyMem_Malloc((len * 2 + 4) * sizeof(char));
        if (to == NULL)
            return NULL;
    }

    {
        #if PG_VERSION_HEX >= 0x080104
            int err;
            if (conn && conn->pgconn)
                ql = PQescapeStringConn(conn->pgconn, to+eq+1, from, len, &err);
            else
        #endif
                ql = PQescapeString(to+eq+1, from, len);
    }

    if (eq)
        to[0] = 'E';
    to[eq] = '\''; 
    to[ql+eq+1] = '\'';
    to[ql+eq+2] = '\0';

    if (tolen)
        *tolen = ql+eq+2;
        
    return to;
}

/* Escape a string to build a valid PostgreSQL identifier
 *
 * Allocate a new buffer on the Python heap containing the new string.
 * 'len' is optional: if 0 the length is calculated.
 *
 * The returned string doesn't include quotes.
 *
 * WARNING: this function is not so safe to allow untrusted input: it does no
 * check for multibyte chars. Such a function should be built on
 * PQescapeIndentifier, which is only available from PostgreSQL 9.0.
 */
char *
psycopg_escape_identifier_easy(const char *from, Py_ssize_t len)
{
    char *rv;
    const char *src;
    char *dst;

    if (!len) { len = strlen(from); }
    if (!(rv = PyMem_New(char, 1 + 2 * len))) {
        PyErr_NoMemory();
        return NULL;
    }

    /* The only thing to do is double quotes */
    for (src = from, dst = rv; *src; ++src, ++dst) {
        *dst = *src;
        if ('"' == *src) {
            *++dst = '"';
        }
    }

    *dst = '\0';

    return rv;
}

/* Duplicate a string.
 *
 * Allocate a new buffer on the Python heap containing the new string.
 * 'len' is optional: if 0 the length is calculated.
 *
 * Store the return in 'to' and return 0 in case of success, else return -1
 * and raise an exception.
 */
RAISES_NEG int
psycopg_strdup(char **to, const char *from, Py_ssize_t len)
{
    if (!len) { len = strlen(from); }
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

