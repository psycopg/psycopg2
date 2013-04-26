/* psycopgmodule.c - psycopg module (will import other C classes)
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

#define PSYCOPG_MODULE
#include "psycopg/psycopg.h"

#include "psycopg/connection.h"
#include "psycopg/cursor.h"
#include "psycopg/green.h"
#include "psycopg/lobject.h"
#include "psycopg/notify.h"
#include "psycopg/xid.h"
#include "psycopg/typecast.h"
#include "psycopg/microprotocols.h"
#include "psycopg/microprotocols_proto.h"
#include "psycopg/error.h"
#include "psycopg/diagnostics.h"

#include "psycopg/adapter_qstring.h"
#include "psycopg/adapter_binary.h"
#include "psycopg/adapter_pboolean.h"
#include "psycopg/adapter_pint.h"
#include "psycopg/adapter_pfloat.h"
#include "psycopg/adapter_pdecimal.h"
#include "psycopg/adapter_asis.h"
#include "psycopg/adapter_list.h"
#include "psycopg/typecast_binary.h"

#ifdef HAVE_MXDATETIME
#include <mxDateTime.h>
#include "psycopg/adapter_mxdatetime.h"
#endif

/* some module-level variables, like the datetime module */
#include <datetime.h>
#include "psycopg/adapter_datetime.h"
HIDDEN PyObject *pyDateTimeModuleP = NULL;

/* pointers to the psycopg.tz classes */
HIDDEN PyObject *pyPsycopgTzModule = NULL;
HIDDEN PyObject *pyPsycopgTzLOCAL = NULL;
HIDDEN PyObject *pyPsycopgTzFixedOffsetTimezone = NULL;

HIDDEN PyObject *psycoEncodings = NULL;

#ifdef PSYCOPG_DEBUG
HIDDEN int psycopg_debug_enabled = 0;
#endif

/* Python representation of SQL NULL */
HIDDEN PyObject *psyco_null = NULL;

/* The type of the cursor.description items */
HIDDEN PyObject *psyco_DescriptionType = NULL;

/** connect module-level function **/
#define psyco_connect_doc \
"_connect(dsn, [connection_factory], [async]) -- New database connection.\n\n"

static PyObject *
psyco_connect(PyObject *self, PyObject *args, PyObject *keywds)
{
    PyObject *conn = NULL;
    PyObject *factory = NULL;
    const char *dsn = NULL;
    int async = 0;

    static char *kwlist[] = {"dsn", "connection_factory", "async", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywds, "s|Oi", kwlist,
                                     &dsn, &factory, &async)) {
        return NULL;
    }

    Dprintf("psyco_connect: dsn = '%s', async = %d", dsn, async);

    /* allocate connection, fill with errors and return it */
    if (factory == NULL || factory == Py_None) {
        factory = (PyObject *)&connectionType;
    }

    /* Here we are breaking the connection.__init__ interface defined
     * by psycopg2. So, if not requiring an async conn, avoid passing
     * the async parameter. */
    /* TODO: would it be possible to avoid an additional parameter
     * to the conn constructor? A subclass? (but it would require mixins
     * to further subclass) Another dsn parameter (but is not really
     * a connection parameter that can be configured) */
    if (!async) {
        conn = PyObject_CallFunction(factory, "s", dsn);
    } else {
        conn = PyObject_CallFunction(factory, "si", dsn, async);
    }

    return conn;
}

/** type registration **/
#define psyco_register_type_doc \
"register_type(obj, conn_or_curs) -> None -- register obj with psycopg type system\n\n" \
":Parameters:\n" \
"  * `obj`: A type adapter created by `new_type()`\n" \
"  * `conn_or_curs`: A connection, cursor or None"

#define typecast_from_python_doc \
"new_type(oids, name, castobj) -> new type object\n\n" \
"Create a new binding object. The object can be used with the\n" \
"`register_type()` function to bind PostgreSQL objects to python objects.\n\n" \
":Parameters:\n" \
"  * `oids`: Tuple of ``oid`` of the PostgreSQL types to convert.\n" \
"  * `name`: Name for the new type\n" \
"  * `adapter`: Callable to perform type conversion.\n" \
"    It must have the signature ``fun(value, cur)`` where ``value`` is\n" \
"    the string representation returned by PostgreSQL (`!None` if ``NULL``)\n" \
"    and ``cur`` is the cursor from which data are read."

#define typecast_array_from_python_doc \
"new_array_type(oids, name, baseobj) -> new type object\n\n" \
"Create a new binding object to parse an array.\n\n" \
"The object can be used with `register_type()`.\n\n" \
":Parameters:\n" \
"  * `oids`: Tuple of ``oid`` of the PostgreSQL types to convert.\n" \
"  * `name`: Name for the new type\n" \
"  * `baseobj`: Adapter to perform type conversion of a single array item."

static PyObject *
psyco_register_type(PyObject *self, PyObject *args)
{
    PyObject *type, *obj = NULL;

    if (!PyArg_ParseTuple(args, "O!|O", &typecastType, &type, &obj)) {
        return NULL;
    }

    if (obj != NULL && obj != Py_None) {
        if (PyObject_TypeCheck(obj, &cursorType)) {
            PyObject **dict = &(((cursorObject*)obj)->string_types);
            if (*dict == NULL) {
                if (!(*dict = PyDict_New())) { return NULL; }
            }
            if (0 > typecast_add(type, *dict, 0)) { return NULL; }
        }
        else if (PyObject_TypeCheck(obj, &connectionType)) {
            if (0 > typecast_add(type, ((connectionObject*)obj)->string_types, 0)) {
                return NULL;
            }
        }
        else {
            PyErr_SetString(PyExc_TypeError,
                "argument 2 must be a connection, cursor or None");
            return NULL;
        }
    }
    else {
        if (0 > typecast_add(type, NULL, 0)) { return NULL; }
    }

    Py_RETURN_NONE;
}


/* Initialize the default adapters map
 *
 * Return 0 on success, else -1 and set an exception.
 */
static int
psyco_adapters_init(PyObject *mod)
{
    PyObject *call = NULL;
    int rv = -1;

    if (0 != microprotocols_add(&PyFloat_Type, NULL, (PyObject*)&pfloatType)) {
        goto exit;
    }
#if PY_MAJOR_VERSION < 3
    if (0 != microprotocols_add(&PyInt_Type, NULL, (PyObject*)&pintType)) {
        goto exit;
    }
#endif
    if (0 != microprotocols_add(&PyLong_Type, NULL, (PyObject*)&pintType)) {
        goto exit;
    }
    if (0 != microprotocols_add(&PyBool_Type, NULL, (PyObject*)&pbooleanType)) {
        goto exit;
    }

    /* strings */
#if PY_MAJOR_VERSION < 3
    if (0 != microprotocols_add(&PyString_Type, NULL, (PyObject*)&qstringType)) {
        goto exit;
    }
#endif
    if (0 != microprotocols_add(&PyUnicode_Type, NULL, (PyObject*)&qstringType)) {
        goto exit;
    }

    /* binary */
#if PY_MAJOR_VERSION < 3
    if (0 != microprotocols_add(&PyBuffer_Type, NULL, (PyObject*)&binaryType)) {
        goto exit;
    }
#else
    if (0 != microprotocols_add(&PyBytes_Type, NULL, (PyObject*)&binaryType)) {
        goto exit;
    }
#endif

#if PY_MAJOR_VERSION >= 3 || PY_MINOR_VERSION >= 6
    if (0 != microprotocols_add(&PyByteArray_Type, NULL, (PyObject*)&binaryType)) {
        goto exit;
    }
#endif
#if PY_MAJOR_VERSION >= 3 || PY_MINOR_VERSION >= 7
    if (0 != microprotocols_add(&PyMemoryView_Type, NULL, (PyObject*)&binaryType)) {
        goto exit;
    }
#endif

    if (0 != microprotocols_add(&PyList_Type, NULL, (PyObject*)&listType)) {
        goto exit;
    }

    /* the module has already been initialized, so we can obtain the callable
       objects directly from its dictionary :) */
    if (!(call = PyMapping_GetItemString(mod, "DateFromPy"))) { goto exit; }
    if (0 != microprotocols_add(PyDateTimeAPI->DateType, NULL, call)) { goto exit; }
    Py_CLEAR(call);

    if (!(call = PyMapping_GetItemString(mod, "TimeFromPy"))) { goto exit; }
    if (0 != microprotocols_add(PyDateTimeAPI->TimeType, NULL, call)) { goto exit; }
    Py_CLEAR(call);

    if (!(call = PyMapping_GetItemString(mod, "TimestampFromPy"))) { goto exit; }
    if (0 != microprotocols_add(PyDateTimeAPI->DateTimeType, NULL, call)) { goto exit; }
    Py_CLEAR(call);

    if (!(call = PyMapping_GetItemString(mod, "IntervalFromPy"))) { goto exit; }
    if (0 != microprotocols_add(PyDateTimeAPI->DeltaType, NULL, call)) { goto exit; }
    Py_CLEAR(call);

#ifdef HAVE_MXDATETIME
    /* as above, we use the callable objects from the psycopg module */
    if (NULL != (call = PyMapping_GetItemString(mod, "TimestampFromMx"))) {
        if (0 != microprotocols_add(mxDateTime.DateTime_Type, NULL, call)) { goto exit; }
        Py_CLEAR(call);

        /* if we found the above, we have this too. */
        if (!(call = PyMapping_GetItemString(mod, "TimeFromMx"))) { goto exit; }
        if (0 != microprotocols_add(mxDateTime.DateTimeDelta_Type, NULL, call)) { goto exit; }
        Py_CLEAR(call);
    }
    else {
        PyErr_Clear();
    }
#endif

    /* Success! */
    rv = 0;

exit:
    Py_XDECREF(call);

    return rv;
}

/* psyco_encodings_fill

   Fill the module's postgresql<->python encoding table */

static encodingPair encodings[] = {
    {"ABC",          "cp1258"},
    {"ALT",          "cp866"},
    {"BIG5",         "big5"},
    {"EUC_CN",       "euccn"},
    {"EUC_JIS_2004", "euc_jis_2004"},
    {"EUC_JP",       "euc_jp"},
    {"EUC_KR",       "euc_kr"},
    {"GB18030",      "gb18030"},
    {"GBK",          "gbk"},
    {"ISO_8859_1",   "iso8859_1"},
    {"ISO_8859_2",   "iso8859_2"},
    {"ISO_8859_3",   "iso8859_3"},
    {"ISO_8859_5",   "iso8859_5"},
    {"ISO_8859_6",   "iso8859_6"},
    {"ISO_8859_7",   "iso8859_7"},
    {"ISO_8859_8",   "iso8859_8"},
    {"ISO_8859_9",   "iso8859_9"},
    {"ISO_8859_10",  "iso8859_10"},
    {"ISO_8859_13",  "iso8859_13"},
    {"ISO_8859_14",  "iso8859_14"},
    {"ISO_8859_15",  "iso8859_15"},
    {"ISO_8859_16",  "iso8859_16"},
    {"JOHAB",        "johab"},
    {"KOI8",         "koi8_r"},
    {"KOI8R",        "koi8_r"},
    {"KOI8U",        "koi8_u"},
    {"LATIN1",       "iso8859_1"},
    {"LATIN2",       "iso8859_2"},
    {"LATIN3",       "iso8859_3"},
    {"LATIN4",       "iso8859_4"},
    {"LATIN5",       "iso8859_9"},
    {"LATIN6",       "iso8859_10"},
    {"LATIN7",       "iso8859_13"},
    {"LATIN8",       "iso8859_14"},
    {"LATIN9",       "iso8859_15"},
    {"LATIN10",      "iso8859_16"},
    {"Mskanji",      "cp932"},
    {"ShiftJIS",     "cp932"},
    {"SHIFT_JIS_2004", "shift_jis_2004"},
    {"SJIS",         "cp932"},
    {"SQL_ASCII",    "ascii"},  /* XXX this is wrong: SQL_ASCII means "no
                                 *  encoding" we should fix the unicode
                                 *  typecaster to return a str or bytes in Py3
                                 */
    {"TCVN",         "cp1258"},
    {"TCVN5712",     "cp1258"},
    {"UHC",          "cp949"},
    {"UNICODE",      "utf_8"}, /* Not valid in 8.2, backward compatibility */
    {"UTF8",         "utf_8"},
    {"VSCII",        "cp1258"},
    {"WIN",          "cp1251"},
    {"WIN866",       "cp866"},
    {"WIN874",       "cp874"},
    {"WIN932",       "cp932"},
    {"WIN936",       "gbk"},
    {"WIN949",       "cp949"},
    {"WIN950",       "cp950"},
    {"WIN1250",      "cp1250"},
    {"WIN1251",      "cp1251"},
    {"WIN1252",      "cp1252"},
    {"WIN1253",      "cp1253"},
    {"WIN1254",      "cp1254"},
    {"WIN1255",      "cp1255"},
    {"WIN1256",      "cp1256"},
    {"WIN1257",      "cp1257"},
    {"WIN1258",      "cp1258"},
    {"Windows932",   "cp932"},
    {"Windows936",   "gbk"},
    {"Windows949",   "cp949"},
    {"Windows950",   "cp950"},

/* those are missing from Python:                */
/*    {"EUC_TW", "?"},                           */
/*    {"MULE_INTERNAL", "?"},                    */
    {NULL, NULL}
};

/* Initialize the encodings table.
 *
 * Return 0 on success, else -1 and set an exception.
 */
static int psyco_encodings_fill(PyObject *dict)
{
    PyObject *value = NULL;
    encodingPair *enc;
    int rv = -1;

    for (enc = encodings; enc->pgenc != NULL; enc++) {
        if (!(value = Text_FromUTF8(enc->pyenc))) { goto exit; }
        if (0 != PyDict_SetItemString(dict, enc->pgenc, value)) { goto exit; }
        Py_CLEAR(value);
    }
    rv = 0;

exit:
    Py_XDECREF(value);

    return rv;
}

/* psyco_errors_init, psyco_errors_fill (callable from C)

   Initialize the module's exceptions and after that a dictionary with a full
   set of exceptions. */

PyObject *Error, *Warning, *InterfaceError, *DatabaseError,
    *InternalError, *OperationalError, *ProgrammingError,
    *IntegrityError, *DataError, *NotSupportedError;
#ifdef PSYCOPG_EXTENSIONS
PyObject *QueryCanceledError, *TransactionRollbackError;
#endif

/* mapping between exception names and their PyObject */
static struct {
    char *name;
    PyObject **exc;
    PyObject **base;
    const char *docstr;
} exctable[] = {
    { "psycopg2.Error", &Error, NULL, Error_doc },
    { "psycopg2.Warning", &Warning, NULL, Warning_doc },
    { "psycopg2.InterfaceError", &InterfaceError, &Error, InterfaceError_doc },
    { "psycopg2.DatabaseError", &DatabaseError, &Error, DatabaseError_doc },
    { "psycopg2.InternalError", &InternalError, &DatabaseError, InternalError_doc },
    { "psycopg2.OperationalError", &OperationalError, &DatabaseError,
        OperationalError_doc },
    { "psycopg2.ProgrammingError", &ProgrammingError, &DatabaseError,
        ProgrammingError_doc },
    { "psycopg2.IntegrityError", &IntegrityError, &DatabaseError,
        IntegrityError_doc },
    { "psycopg2.DataError", &DataError, &DatabaseError, DataError_doc },
    { "psycopg2.NotSupportedError", &NotSupportedError, &DatabaseError,
        NotSupportedError_doc },
#ifdef PSYCOPG_EXTENSIONS
    { "psycopg2.extensions.QueryCanceledError", &QueryCanceledError,
      &OperationalError, QueryCanceledError_doc },
    { "psycopg2.extensions.TransactionRollbackError",
      &TransactionRollbackError, &OperationalError,
      TransactionRollbackError_doc },
#endif
    {NULL}  /* Sentinel */
};


static int
psyco_errors_init(void)
{
    /* the names of the exceptions here reflect the organization of the
       psycopg2 module and not the fact the the original error objects
       live in _psycopg */

    int i;
    PyObject *dict = NULL;
    PyObject *str = NULL;
    int rv = -1;

    /* 'Error' has been defined elsewhere: only init the other classes */
    Error = (PyObject *)&errorType;

    for (i = 1; exctable[i].name; i++) {
        if (!(dict = PyDict_New())) { goto exit; }

        if (exctable[i].docstr) {
            if (!(str = Text_FromUTF8(exctable[i].docstr))) { goto exit; }
            if (0 != PyDict_SetItemString(dict, "__doc__", str)) { goto exit; }
            Py_CLEAR(str);
        }

        /* can't put PyExc_StandardError in the static exctable:
         * windows build will fail */
        if (!(*exctable[i].exc = PyErr_NewException(
                exctable[i].name,
                exctable[i].base ? *exctable[i].base : PyExc_StandardError,
                dict))) {
            goto exit;
        }
        Py_CLEAR(dict);
    }

    rv = 0;

exit:
    Py_XDECREF(str);
    Py_XDECREF(dict);
    return rv;
}

void
psyco_errors_fill(PyObject *dict)
{
    int i;
    char *name;

    for (i = 0; exctable[i].name; i++) {
        if (NULL == exctable[i].exc) { continue; }

        /* the name is the part after the last dot */
        name = strrchr(exctable[i].name, '.');
        name = name ? name + 1 : exctable[i].name;

        PyDict_SetItemString(dict, name, *exctable[i].exc);
    }
}

void
psyco_errors_set(PyObject *type)
{
    int i;
    char *name;

    for (i = 0; exctable[i].name; i++) {
        if (NULL == exctable[i].exc) { continue; }

        /* the name is the part after the last dot */
        name = strrchr(exctable[i].name, '.');
        name = name ? name + 1 : exctable[i].name;

        PyObject_SetAttrString(type, name, *exctable[i].exc);
    }
}

/* psyco_set_error

   Create a new error of the given type with extra attributes. */

/* TODO: may have been changed to BORROWED */
RAISES PyObject *
psyco_set_error(PyObject *exc, cursorObject *curs, const char *msg)
{
    PyObject *pymsg;
    PyObject *err = NULL;
    connectionObject *conn = NULL;

    if (curs) {
        conn = ((cursorObject *)curs)->conn;
    }

    if ((pymsg = conn_text_from_chars(conn, msg))) {
        err = PyObject_CallFunctionObjArgs(exc, pymsg, NULL);
        Py_DECREF(pymsg);
    }
    else {
        /* what's better than an error in an error handler in the morning?
         * Anyway, some error was set, refcount is ok... get outta here. */
        return NULL;
    }

    if (err && PyObject_TypeCheck(err, &errorType)) {
        errorObject *perr = (errorObject *)err;
        if (curs) {
            Py_CLEAR(perr->cursor);
            Py_INCREF(curs);
            perr->cursor = curs;
        }
    }

    if (err) {
        PyErr_SetObject(exc, err);
        Py_DECREF(err);
    }

    return err;
}


/* Return nonzero if the current one is the main interpreter */
static int
psyco_is_main_interp(void)
{
    static PyInterpreterState *main_interp = NULL;  /* Cached reference */
    PyInterpreterState *interp;

    if (main_interp) {
        return (main_interp == PyThreadState_Get()->interp);
    }

    /* No cached value: cache the proper value and try again. */
    interp = PyInterpreterState_Head();
    while (interp->next)
        interp = interp->next;

    main_interp = interp;
    assert (main_interp);
    return psyco_is_main_interp();
}


/* psyco_GetDecimalType

   Return a new reference to the adapter for decimal type.

   If decimals should be used but the module import fails, fall back on
   the float type.

    If decimals are not to be used, return NULL.
*/

PyObject *
psyco_GetDecimalType(void)
{
    static PyObject *cachedType = NULL;
    PyObject *decimalType = NULL;
    PyObject *decimal;

    /* Use the cached object if running from the main interpreter. */
    int can_cache = psyco_is_main_interp();
    if (can_cache && cachedType) {
        Py_INCREF(cachedType);
        return cachedType;
    }

    /* Get a new reference to the Decimal type. */
    decimal = PyImport_ImportModule("decimal");
    if (decimal) {
        decimalType = PyObject_GetAttrString(decimal, "Decimal");
        Py_DECREF(decimal);
    }
    else {
        PyErr_Clear();
        decimalType = NULL;
    }

    /* Store the object from future uses. */
    if (can_cache && !cachedType && decimalType) {
        Py_INCREF(decimalType);
        cachedType = decimalType;
    }

    return decimalType;
}


/* Create a namedtuple for cursor.description items
 *
 * Return None in case of expected errors (e.g. namedtuples not available)
 * NULL in case of errors to propagate.
 */
static PyObject *
psyco_make_description_type(void)
{
    PyObject *nt = NULL;
    PyObject *coll = NULL;
    PyObject *rv = NULL;

    /* Try to import collections.namedtuple */
    if (!(coll = PyImport_ImportModule("collections"))) {
        Dprintf("psyco_make_description_type: collections import failed");
        goto error;
    }
    if (!(nt = PyObject_GetAttrString(coll, "namedtuple"))) {
        Dprintf("psyco_make_description_type: no collections.namedtuple");
        goto error;
    }

    /* Build the namedtuple */
    rv = PyObject_CallFunction(nt, "ss", "Column",
        "name type_code display_size internal_size precision scale null_ok");

exit:
    Py_XDECREF(coll);
    Py_XDECREF(nt);

    return rv;

error:
    /* controlled error: we will fall back to regular tuples. Return None. */
    PyErr_Clear();
    rv = Py_None;
    Py_INCREF(rv);
    goto exit;
}


/** method table and module initialization **/

static PyMethodDef psycopgMethods[] = {
    {"_connect",  (PyCFunction)psyco_connect,
     METH_VARARGS|METH_KEYWORDS, psyco_connect_doc},
    {"adapt",  (PyCFunction)psyco_microprotocols_adapt,
     METH_VARARGS, psyco_microprotocols_adapt_doc},

    {"register_type", (PyCFunction)psyco_register_type,
     METH_VARARGS, psyco_register_type_doc},
    {"new_type", (PyCFunction)typecast_from_python,
     METH_VARARGS|METH_KEYWORDS, typecast_from_python_doc},
    {"new_array_type", (PyCFunction)typecast_array_from_python,
     METH_VARARGS|METH_KEYWORDS, typecast_array_from_python_doc},

    {"AsIs",  (PyCFunction)psyco_AsIs,
     METH_VARARGS, psyco_AsIs_doc},
    {"QuotedString",  (PyCFunction)psyco_QuotedString,
     METH_VARARGS, psyco_QuotedString_doc},
    {"Boolean",  (PyCFunction)psyco_Boolean,
     METH_VARARGS, psyco_Boolean_doc},
    {"Int",  (PyCFunction)psyco_Int,
     METH_VARARGS, psyco_Int_doc},
    {"Float",  (PyCFunction)psyco_Float,
     METH_VARARGS, psyco_Float_doc},
    {"Decimal",  (PyCFunction)psyco_Decimal,
     METH_VARARGS, psyco_Decimal_doc},
    {"Binary",  (PyCFunction)psyco_Binary,
     METH_VARARGS, psyco_Binary_doc},
    {"Date",  (PyCFunction)psyco_Date,
     METH_VARARGS, psyco_Date_doc},
    {"Time",  (PyCFunction)psyco_Time,
     METH_VARARGS, psyco_Time_doc},
    {"Timestamp",  (PyCFunction)psyco_Timestamp,
     METH_VARARGS, psyco_Timestamp_doc},
    {"DateFromTicks",  (PyCFunction)psyco_DateFromTicks,
     METH_VARARGS, psyco_DateFromTicks_doc},
    {"TimeFromTicks",  (PyCFunction)psyco_TimeFromTicks,
     METH_VARARGS, psyco_TimeFromTicks_doc},
    {"TimestampFromTicks",  (PyCFunction)psyco_TimestampFromTicks,
     METH_VARARGS, psyco_TimestampFromTicks_doc},
    {"List",  (PyCFunction)psyco_List,
     METH_VARARGS, psyco_List_doc},

    {"DateFromPy",  (PyCFunction)psyco_DateFromPy,
     METH_VARARGS, psyco_DateFromPy_doc},
    {"TimeFromPy",  (PyCFunction)psyco_TimeFromPy,
     METH_VARARGS, psyco_TimeFromPy_doc},
    {"TimestampFromPy",  (PyCFunction)psyco_TimestampFromPy,
     METH_VARARGS, psyco_TimestampFromPy_doc},
    {"IntervalFromPy",  (PyCFunction)psyco_IntervalFromPy,
     METH_VARARGS, psyco_IntervalFromPy_doc},

#ifdef HAVE_MXDATETIME
    /* to be deleted if not found at import time */
    {"DateFromMx",  (PyCFunction)psyco_DateFromMx,
     METH_VARARGS, psyco_DateFromMx_doc},
    {"TimeFromMx",  (PyCFunction)psyco_TimeFromMx,
     METH_VARARGS, psyco_TimeFromMx_doc},
    {"TimestampFromMx",  (PyCFunction)psyco_TimestampFromMx,
     METH_VARARGS, psyco_TimestampFromMx_doc},
    {"IntervalFromMx",  (PyCFunction)psyco_IntervalFromMx,
     METH_VARARGS, psyco_IntervalFromMx_doc},
#endif

#ifdef PSYCOPG_EXTENSIONS
    {"set_wait_callback",  (PyCFunction)psyco_set_wait_callback,
     METH_O, psyco_set_wait_callback_doc},
    {"get_wait_callback",  (PyCFunction)psyco_get_wait_callback,
     METH_NOARGS, psyco_get_wait_callback_doc},
#endif

    {NULL, NULL, 0, NULL}        /* Sentinel */
};

#if PY_MAJOR_VERSION > 2
static struct PyModuleDef psycopgmodule = {
        PyModuleDef_HEAD_INIT,
        "_psycopg",
        NULL,
        -1,
        psycopgMethods,
        NULL,
        NULL,
        NULL,
        NULL
};
#endif

PyMODINIT_FUNC
INIT_MODULE(_psycopg)(void)
{
#if PY_VERSION_HEX < 0x03020000
    static void *PSYCOPG_API[PSYCOPG_API_pointers];
    PyObject *c_api_object;
#endif

    PyObject *module = NULL, *dict;

#ifdef PSYCOPG_DEBUG
    if (getenv("PSYCOPG_DEBUG"))
        psycopg_debug_enabled = 1;
#endif

    Dprintf("initpsycopg: initializing psycopg %s", PSYCOPG_VERSION);

    /* initialize all the new types and then the module */
    Py_TYPE(&connectionType) = &PyType_Type;
    if (PyType_Ready(&connectionType) == -1) goto exit;

    Py_TYPE(&cursorType) = &PyType_Type;
    if (PyType_Ready(&cursorType) == -1) goto exit;

    Py_TYPE(&typecastType) = &PyType_Type;
    if (PyType_Ready(&typecastType) == -1) goto exit;

    Py_TYPE(&qstringType) = &PyType_Type;
    if (PyType_Ready(&qstringType) == -1) goto exit;

    Py_TYPE(&binaryType) = &PyType_Type;
    if (PyType_Ready(&binaryType) == -1) goto exit;

    Py_TYPE(&isqlquoteType) = &PyType_Type;
    if (PyType_Ready(&isqlquoteType) == -1) goto exit;

    Py_TYPE(&pbooleanType) = &PyType_Type;
    if (PyType_Ready(&pbooleanType) == -1) goto exit;

    Py_TYPE(&pintType) = &PyType_Type;
    if (PyType_Ready(&pintType) == -1) goto exit;

    Py_TYPE(&pfloatType) = &PyType_Type;
    if (PyType_Ready(&pfloatType) == -1) goto exit;

    Py_TYPE(&pdecimalType) = &PyType_Type;
    if (PyType_Ready(&pdecimalType) == -1) goto exit;

    Py_TYPE(&asisType) = &PyType_Type;
    if (PyType_Ready(&asisType) == -1) goto exit;

    Py_TYPE(&listType) = &PyType_Type;
    if (PyType_Ready(&listType) == -1) goto exit;

    Py_TYPE(&chunkType) = &PyType_Type;
    if (PyType_Ready(&chunkType) == -1) goto exit;

    Py_TYPE(&notifyType) = &PyType_Type;
    if (PyType_Ready(&notifyType) == -1) goto exit;

    Py_TYPE(&xidType) = &PyType_Type;
    if (PyType_Ready(&xidType) == -1) goto exit;

    Py_TYPE(&errorType) = &PyType_Type;
    errorType.tp_base = (PyTypeObject *)PyExc_StandardError;
    if (PyType_Ready(&errorType) == -1) goto exit;

    Py_TYPE(&diagnosticsType) = &PyType_Type;
    if (PyType_Ready(&diagnosticsType) == -1) goto exit;

#ifdef PSYCOPG_EXTENSIONS
    Py_TYPE(&lobjectType) = &PyType_Type;
    if (PyType_Ready(&lobjectType) == -1) goto exit;
#endif

    /* import mx.DateTime module, if necessary */
#ifdef HAVE_MXDATETIME
    Py_TYPE(&mxdatetimeType) = &PyType_Type;
    if (PyType_Ready(&mxdatetimeType) == -1) goto exit;

    if (0 != mxDateTime_ImportModuleAndAPI()) {
        PyErr_Clear();

        /* only fail if the mx typacaster should have been the default */
#ifdef PSYCOPG_DEFAULT_MXDATETIME
        PyErr_SetString(PyExc_ImportError,
            "can't import mx.DateTime module (requested as default adapter)");
        goto exit;
#endif
    }
#endif

    /* import python builtin datetime module, if available */
    pyDateTimeModuleP = PyImport_ImportModule("datetime");
    if (pyDateTimeModuleP == NULL) {
        Dprintf("initpsycopg: can't import datetime module");
        PyErr_SetString(PyExc_ImportError, "can't import datetime module");
        goto exit;
    }

    /* Initialize the PyDateTimeAPI everywhere is used */
    PyDateTime_IMPORT;
    if (psyco_adapter_datetime_init()) { goto exit; }

    Py_TYPE(&pydatetimeType) = &PyType_Type;
    if (PyType_Ready(&pydatetimeType) == -1) goto exit;

    /* import psycopg2.tz anyway (TODO: replace with C-level module?) */
    pyPsycopgTzModule = PyImport_ImportModule("psycopg2.tz");
    if (pyPsycopgTzModule == NULL) {
        Dprintf("initpsycopg: can't import psycopg2.tz module");
        PyErr_SetString(PyExc_ImportError, "can't import psycopg2.tz module");
        goto exit;
    }
    pyPsycopgTzLOCAL =
        PyObject_GetAttrString(pyPsycopgTzModule, "LOCAL");
    pyPsycopgTzFixedOffsetTimezone =
        PyObject_GetAttrString(pyPsycopgTzModule, "FixedOffsetTimezone");

    /* initialize the module and grab module's dictionary */
#if PY_MAJOR_VERSION < 3
    module = Py_InitModule("_psycopg", psycopgMethods);
#else
    module = PyModule_Create(&psycopgmodule);
#endif
    if (!module) { goto exit; }

    dict = PyModule_GetDict(module);

    /* initialize all the module's exported functions */
    /* PyBoxer_API[PyBoxer_Fake_NUM] = (void *)PyBoxer_Fake; */

    /* Create a CObject containing the API pointer array's address */
    /* If anybody asks for a PyCapsule we'll deal with it. */
#if PY_VERSION_HEX < 0x03020000
    c_api_object = PyCObject_FromVoidPtr((void *)PSYCOPG_API, NULL);
    if (c_api_object != NULL)
        PyModule_AddObject(module, "_C_API", c_api_object);
#endif

    /* other mixed initializations of module-level variables */
    if (!(psycoEncodings = PyDict_New())) { goto exit; }
    if (0 != psyco_encodings_fill(psycoEncodings)) { goto exit; }
    psyco_null = Bytes_FromString("NULL");
    if (!(psyco_DescriptionType = psyco_make_description_type())) { goto exit; }

    /* set some module's parameters */
    PyModule_AddStringConstant(module, "__version__", PSYCOPG_VERSION);
    PyModule_AddStringConstant(module, "__doc__", "psycopg PostgreSQL driver");
    PyModule_AddObject(module, "apilevel", Text_FromUTF8(APILEVEL));
    PyModule_AddObject(module, "threadsafety", PyInt_FromLong(THREADSAFETY));
    PyModule_AddObject(module, "paramstyle", Text_FromUTF8(PARAMSTYLE));

    /* put new types in module dictionary */
    PyModule_AddObject(module, "connection", (PyObject*)&connectionType);
    PyModule_AddObject(module, "cursor", (PyObject*)&cursorType);
    PyModule_AddObject(module, "ISQLQuote", (PyObject*)&isqlquoteType);
    PyModule_AddObject(module, "Notify", (PyObject*)&notifyType);
    PyModule_AddObject(module, "Xid", (PyObject*)&xidType);
    PyModule_AddObject(module, "Diagnostics", (PyObject*)&diagnosticsType);
#ifdef PSYCOPG_EXTENSIONS
    PyModule_AddObject(module, "lobject", (PyObject*)&lobjectType);
#endif

    /* encodings dictionary in module dictionary */
    PyModule_AddObject(module, "encodings", psycoEncodings);

#ifdef HAVE_MXDATETIME
    /* If we can't find mx.DateTime objects at runtime,
     * remove them from the module (and, as consequence, from the adapters). */
    if (0 != psyco_adapter_mxdatetime_init()) {
        PyDict_DelItemString(dict, "DateFromMx");
        PyDict_DelItemString(dict, "TimeFromMx");
        PyDict_DelItemString(dict, "TimestampFromMx");
        PyDict_DelItemString(dict, "IntervalFromMx");
    }
#endif
    /* initialize default set of typecasters */
    if (0 != typecast_init(dict)) { goto exit; }

    /* initialize microprotocols layer */
    microprotocols_init(dict);
    if (0 != psyco_adapters_init(dict)) { goto exit; }

    /* create a standard set of exceptions and add them to the module's dict */
    if (0 != psyco_errors_init()) { goto exit; }
    psyco_errors_fill(dict);

    Dprintf("initpsycopg: module initialization complete");

exit:
#if PY_MAJOR_VERSION > 2
    return module;
#else
    return;
#endif
}
