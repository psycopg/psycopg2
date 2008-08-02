/* psycopg.h - definitions for the psycopg python module
 *
 * Copyright (C) 2003 Federico Di Gregorio <fog@debian.org>
 *
 * This file is part of psycopg.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef PSYCOPG_H
#define PSYCOPG_H 1

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <libpq-fe.h>

#include "psycopg/config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Python 2.5+ Py_ssize_t compatibility: */
#ifndef PY_FORMAT_SIZE_T
  #define PY_FORMAT_SIZE_T ""
#endif

/* FORMAT_CODE_SIZE_T is for plain size_t, not for Py_ssize_t: */
#ifdef _MSC_VER
  /* For MSVC: */
  #define FORMAT_CODE_SIZE_T "%Iu"
#else
  /* C99 standard format code: */
  #define FORMAT_CODE_SIZE_T "%zu"
#endif
/* FORMAT_CODE_PY_SSIZE_T is for Py_ssize_t: */
#define FORMAT_CODE_PY_SSIZE_T "%" PY_FORMAT_SIZE_T "d"

#if PY_MAJOR_VERSION == 2 && PY_MINOR_VERSION >= 5
  #define CONV_CODE_PY_SSIZE_T "n"
#else
  #define CONV_CODE_PY_SSIZE_T "i"

  typedef int Py_ssize_t;
  #define PY_SSIZE_T_MIN INT_MIN
  #define PY_SSIZE_T_MAX INT_MAX

  #define readbufferproc getreadbufferproc
  #define writebufferproc getwritebufferproc
  #define segcountproc getsegcountproc
  #define charbufferproc getcharbufferproc
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

/* global excpetions */
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

typedef struct {
    char *pgenc;
    char *pyenc;
} encodingPair;

/* the Decimal type, used by the DECIMAL typecaster */
HIDDEN PyObject *psyco_GetDecimalType(void);

/* some utility functions */
HIDDEN void psyco_set_error(PyObject *exc, PyObject *curs,  const char *msg,
                            const char *pgerror, const char *pgcode);

HIDDEN size_t qstring_escape(char *to, char *from, size_t len, PGconn *conn);

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
"A not supported datbase API was called."

#ifdef PSYCOPG_EXTENSIONS
#define QueryCanceledError_doc \
"Error related to SQL query cancelation."

#define TransactionRollbackError_doc \
"Error causing transaction rollback (deadlocks, serialisation failures, etc)."
#endif

#ifdef __cplusplus
}
#endif

#endif /* !defined(PSYCOPG_H) */
