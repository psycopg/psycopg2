/* psycopg.h - definitions for the psycopg python module
 *
 * Copyright (C) 2003-2010 Federico Di Gregorio <fog@debian.org>
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

#ifndef PSYCOPG_H
#define PSYCOPG_H 1

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <libpq-fe.h>

#include "psycopg/config.h"
#include "psycopg/python.h"

#ifdef __cplusplus
extern "C" {
#endif

/* DBAPI compliance parameters */
#define APILEVEL "2.0"
#define THREADSAFETY 2
#define PARAMSTYLE "pyformat"

/* C API functions */
#define psyco_errors_fill_NUM 0
#define psyco_errors_fill_RETURN void
#define psyco_errors_fill_PROTO (PyObject *dict)
#define psyco_errors_set_NUM 1
#define psyco_errors_set_RETURN void
#define psyco_errors_set_PROTO (PyObject *type)

/* Total number of C API pointers */
#define PSYCOPG_API_pointers 2

#ifdef PSYCOPG_MODULE

    /** This section is used when compiling psycopgmodule.c & co. **/
HIDDEN psyco_errors_fill_RETURN psyco_errors_fill psyco_errors_fill_PROTO;
HIDDEN psyco_errors_set_RETURN psyco_errors_set psyco_errors_set_PROTO;

/* global exceptions */
extern HIDDEN PyObject *Error, *Warning, *InterfaceError, *DatabaseError,
    *InternalError, *OperationalError, *ProgrammingError,
    *IntegrityError, *DataError, *NotSupportedError;
#ifdef PSYCOPG_EXTENSIONS
extern HIDDEN PyObject *QueryCanceledError, *TransactionRollbackError;
#endif

/* python versions and compatibility stuff */
#ifndef PyMODINIT_FUNC
#define PyMODINIT_FUNC void
#endif

#else
    /** This section is used in modules that use psycopg's C API **/

static void **PSYCOPG_API;

#define psyco_errors_fill \
 (*(psyco_errors_fill_RETURN (*)psyco_errors_fill_PROTO) \
  PSYCOPG_API[psyco_errors_fill_NUM])
#define psyco_errors_set \
 (*(psyco_errors_set_RETURN (*)psyco_errors_set_PROTO) \
  PSYCOPG_API[psyco_errors_set_NUM])

/* Return -1 and set exception on error, 0 on success. */
static int
import_psycopg(void)
{
    PyObject *module = PyImport_ImportModule("psycopg");

    if (module != NULL) {
        PyObject *c_api_object = PyObject_GetAttrString(module, "_C_API");
        if (c_api_object == NULL) return -1;
        if (PyCObject_Check(c_api_object))
            PSYCOPG_API = (void **)PyCObject_AsVoidPtr(c_api_object);
        Py_DECREF(c_api_object);
    }
    return 0;
}

#endif

/* postgresql<->python encoding map */
extern HIDDEN PyObject *psycoEncodings;

/* SQL NULL */
extern HIDDEN PyObject *psyco_null;

typedef struct {
    char *pgenc;
    char *pyenc;
} encodingPair;

/* the Decimal type, used by the DECIMAL typecaster */
HIDDEN PyObject *psyco_GetDecimalType(void);

/* forward declarations */
typedef struct cursorObject cursorObject;
typedef struct connectionObject connectionObject;

/* some utility functions */
RAISES HIDDEN PyObject *psyco_set_error(PyObject *exc, cursorObject *curs, const char *msg);

HIDDEN char *psycopg_escape_string(connectionObject *conn,
              const char *from, Py_ssize_t len, char *to, Py_ssize_t *tolen);
HIDDEN char *psycopg_escape_identifier_easy(const char *from, Py_ssize_t len);
HIDDEN int psycopg_strdup(char **to, const char *from, Py_ssize_t len);
HIDDEN int psycopg_is_text_file(PyObject *f);

STEALS(1) HIDDEN PyObject * psycopg_ensure_bytes(PyObject *obj);

STEALS(1) HIDDEN PyObject * psycopg_ensure_text(PyObject *obj);

/* Exceptions docstrings */
#define Error_doc \
"Base class for error exceptions."

#define Warning_doc \
"A database warning."

#define InterfaceError_doc \
"Error related to the database interface."

#define DatabaseError_doc \
"Error related to the database engine."

#define InternalError_doc \
"The database encountered an internal error."

#define OperationalError_doc \
"Error related to database operation (disconnect, memory allocation etc)."

#define ProgrammingError_doc \
"Error related to database programming (SQL error, table not found etc)."

#define IntegrityError_doc \
"Error related to database integrity."

#define DataError_doc \
"Error related to problems with the processed data."

#define NotSupportedError_doc \
"A method or database API was used which is not supported by the database."

#ifdef PSYCOPG_EXTENSIONS
#define QueryCanceledError_doc \
"Error related to SQL query cancellation."

#define TransactionRollbackError_doc \
"Error causing transaction rollback (deadlocks, serialization failures, etc)."
#endif

#ifdef __cplusplus
}
#endif

#endif /* !defined(PSYCOPG_H) */
