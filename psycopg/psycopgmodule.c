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

#include "psycopg/adapter_qstring.h"
#include "psycopg/adapter_binary.h"
#include "psycopg/adapter_pboolean.h"
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
"connect(dsn, ...) -- Create a new database connection.\n\n"               \
"This function supports two different but equivalent sets of arguments.\n" \
"A single data source name or ``dsn`` string can be used to specify the\n" \
"connection parameters, as follows::\n\n"                                  \
"    psycopg2.connect(\"dbname=xxx user=xxx ...\")\n\n"   \
"If ``dsn`` is not provided it is possible to pass the parameters as\n"    \
"keyword arguments; e.g.::\n\n"                                            \
"    psycopg2.connect(database='xxx', user='xxx', ...)\n\n"                \
"The full list of available parameters is:\n\n"                            \
"- ``dbname`` -- database name (only in 'dsn')\n"                          \
"- ``database`` -- database name (only as keyword argument)\n"             \
"- ``host`` -- host address (defaults to UNIX socket if not provided)\n"   \
"- ``port`` -- port number (defaults to 5432 if not provided)\n"           \
"- ``user`` -- user name used to authenticate\n"                           \
"- ``password`` -- password used to authenticate\n"                        \
"- ``sslmode`` -- SSL mode (see PostgreSQL documentation)\n\n"             \
"- ``async`` -- if the connection should provide asynchronous API\n\n"     \
"If the ``connection_factory`` keyword argument is not provided this\n"    \
"function always return an instance of the `connection` class.\n"          \
"Else the given sub-class of `extensions.connection` will be used to\n"    \
"instantiate the connection object.\n\n"                                   \
":return: New database connection\n"                                         \
":rtype: `extensions.connection`"

static size_t
_psyco_connect_fill_dsn(char *dsn, const char *kw, const char *v, size_t i)
{
    strcpy(&dsn[i], kw); i += strlen(kw);
    strcpy(&dsn[i], v); i += strlen(v);
    return i;
}

static PyObject *
psyco_connect(PyObject *self, PyObject *args, PyObject *keywds)
{
    PyObject *conn = NULL, *factory = NULL;
    PyObject *pyport = NULL;

    size_t idsn=-1;
    int iport=-1;
    const char *dsn_static = NULL;
    char *dsn_dynamic=NULL;
    const char *database=NULL, *user=NULL, *password=NULL;
    const char *host=NULL, *sslmode=NULL;
    char port[16];
    int async = 0;

    static char *kwlist[] = {"dsn", "database", "host", "port",
                             "user", "password", "sslmode",
                             "connection_factory", "async", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywds, "|sssOsssOi", kwlist,
                                     &dsn_static, &database, &host, &pyport,
                                     &user, &password, &sslmode,
                                     &factory, &async)) {
        return NULL;
    }

#if PY_MAJOR_VERSION < 3
    if (pyport && PyString_Check(pyport)) {
        PyObject *pyint = PyInt_FromString(PyString_AsString(pyport), NULL, 10);
        if (!pyint) goto fail;
        /* Must use PyInt_AsLong rather than PyInt_AS_LONG, because
         * PyInt_FromString can return a PyLongObject: */
        iport = PyInt_AsLong(pyint);
        Py_DECREF(pyint);
        if (iport == -1 && PyErr_Occurred())
            goto fail;
    }
    else if (pyport && PyInt_Check(pyport)) {
        iport = PyInt_AsLong(pyport);
        if (iport == -1 && PyErr_Occurred())
            goto fail;
    }
#else
    if (pyport && PyUnicode_Check(pyport)) {
        PyObject *pyint = PyObject_CallFunction((PyObject*)&PyLong_Type,
                                                "Oi", pyport, 10);
        if (!pyint) goto fail;
        iport = PyLong_AsLong(pyint);
        Py_DECREF(pyint);
        if (iport == -1 && PyErr_Occurred())
            goto fail;
    }
    else if (pyport && PyLong_Check(pyport)) {
        iport = PyLong_AsLong(pyport);
        if (iport == -1 && PyErr_Occurred())
            goto fail;
    }
#endif
    else if (pyport != NULL) {
      PyErr_SetString(PyExc_TypeError, "port must be a string or int");
      goto fail;
    }

    if (iport > 0)
      PyOS_snprintf(port, 16, "%d", iport);

    if (dsn_static == NULL) {
        size_t l = 46; /* len(" dbname= user= password= host= port= sslmode=\0") */

        if (database) l += strlen(database);
        if (host) l += strlen(host);
        if (iport > 0) l += strlen(port);
        if (user) l += strlen(user);
        if (password) l += strlen(password);
        if (sslmode) l += strlen(sslmode);

        dsn_dynamic = malloc(l*sizeof(char));
        if (dsn_dynamic == NULL) {
            PyErr_SetString(InterfaceError, "dynamic dsn allocation failed");
            goto fail;
        }

        idsn = 0;
        if (database)
            idsn = _psyco_connect_fill_dsn(dsn_dynamic, " dbname=", database, idsn);
        if (host)
            idsn = _psyco_connect_fill_dsn(dsn_dynamic, " host=", host, idsn);
        if (iport > 0)
            idsn = _psyco_connect_fill_dsn(dsn_dynamic, " port=", port, idsn);
        if (user)
            idsn = _psyco_connect_fill_dsn(dsn_dynamic, " user=", user, idsn);
        if (password)
            idsn = _psyco_connect_fill_dsn(dsn_dynamic, " password=", password, idsn);
        if (sslmode)
            idsn = _psyco_connect_fill_dsn(dsn_dynamic, " sslmode=", sslmode, idsn);

        if (idsn > 0) {
            dsn_dynamic[idsn] = '\0';
            memmove(dsn_dynamic, &dsn_dynamic[1], idsn);
        }
        else {
            PyErr_SetString(InterfaceError, "missing dsn and no parameters");
            goto fail;
        }
    }

    {
      const char *dsn = (dsn_static != NULL ? dsn_static : dsn_dynamic);
      Dprintf("psyco_connect: dsn = '%s', async = %d", dsn, async);

      /* allocate connection, fill with errors and return it */
      if (factory == NULL) factory = (PyObject *)&connectionType;
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
    }

    goto cleanup;
    fail:
      assert (PyErr_Occurred());
      if (conn != NULL) {
        Py_DECREF(conn);
        conn = NULL;
      }
      /* Fall through to cleanup: */
    cleanup:
      if (dsn_dynamic != NULL) {
        free(dsn_dynamic);
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
"new_type(oids, name, adapter) -> new type object\n\n" \
"Create a new binding object. The object can be used with the\n" \
"`register_type()` function to bind PostgreSQL objects to python objects.\n\n" \
":Parameters:\n" \
"  * `oids`: Tuple of ``oid`` of the PostgreSQL types to convert.\n" \
"  * `name`: Name for the new type\n" \
"  * `adapter`: Callable to perform type conversion.\n" \
"    It must have the signature ``fun(value, cur)`` where ``value`` is\n" \
"    the string representation returned by PostgreSQL (`!None` if ``NULL``)\n" \
"    and ``cur`` is the cursor from which data are read."

static void
_psyco_register_type_set(PyObject **dict, PyObject *type)
{
    if (*dict == NULL)
        *dict = PyDict_New();
    typecast_add(type, *dict, 0);
}

static PyObject *
psyco_register_type(PyObject *self, PyObject *args)
{
    PyObject *type, *obj = NULL;

    if (!PyArg_ParseTuple(args, "O!|O", &typecastType, &type, &obj)) {
        return NULL;
    }

    if (obj != NULL && obj != Py_None) {
        if (PyObject_TypeCheck(obj, &cursorType)) {
            _psyco_register_type_set(&(((cursorObject*)obj)->string_types), type);
        }
        else if (PyObject_TypeCheck(obj, &connectionType)) {
            typecast_add(type, ((connectionObject*)obj)->string_types, 0);
        }
        else {
            PyErr_SetString(PyExc_TypeError,
                "argument 2 must be a connection, cursor or None");
            return NULL;
        }
    }
    else {
        typecast_add(type, NULL, 0);
    }

    Py_INCREF(Py_None);
    return Py_None;
}


/* default adapters */

static void
psyco_adapters_init(PyObject *mod)
{
    PyObject *call;
    PyTypeObject *type;

    microprotocols_add(&PyFloat_Type, NULL, (PyObject*)&pfloatType);
#if PY_MAJOR_VERSION < 3
    microprotocols_add(&PyInt_Type, NULL, (PyObject*)&asisType);
#endif
    microprotocols_add(&PyLong_Type, NULL, (PyObject*)&asisType);
    microprotocols_add(&PyBool_Type, NULL, (PyObject*)&pbooleanType);

    /* strings */
#if PY_MAJOR_VERSION < 3
    microprotocols_add(&PyString_Type, NULL, (PyObject*)&qstringType);
#endif
    microprotocols_add(&PyUnicode_Type, NULL, (PyObject*)&qstringType);

    /* binary */
#if PY_MAJOR_VERSION < 3
    microprotocols_add(&PyBuffer_Type, NULL, (PyObject*)&binaryType);
#else
    microprotocols_add(&PyBytes_Type, NULL, (PyObject*)&binaryType);
#endif

#if PY_MAJOR_VERSION >= 3 || PY_MINOR_VERSION >= 6
    microprotocols_add(&PyByteArray_Type, NULL, (PyObject*)&binaryType);
#endif
#if PY_MAJOR_VERSION >= 3 || PY_MINOR_VERSION >= 7
    microprotocols_add(&PyMemoryView_Type, NULL, (PyObject*)&binaryType);
#endif

    microprotocols_add(&PyList_Type, NULL, (PyObject*)&listType);

    if ((type = (PyTypeObject*)psyco_GetDecimalType()) != NULL)
        microprotocols_add(type, NULL, (PyObject*)&pdecimalType);

    /* the module has already been initialized, so we can obtain the callable
       objects directly from its dictionary :) */
    call = PyMapping_GetItemString(mod, "DateFromPy");
    microprotocols_add(PyDateTimeAPI->DateType, NULL, call);
    call = PyMapping_GetItemString(mod, "TimeFromPy");
    microprotocols_add(PyDateTimeAPI->TimeType, NULL, call);
    call = PyMapping_GetItemString(mod, "TimestampFromPy");
    microprotocols_add(PyDateTimeAPI->DateTimeType, NULL, call);
    call = PyMapping_GetItemString(mod, "IntervalFromPy");
    microprotocols_add(PyDateTimeAPI->DeltaType, NULL, call);

#ifdef HAVE_MXDATETIME
    /* as above, we use the callable objects from the psycopg module */
    call = PyMapping_GetItemString(mod, "TimestampFromMx");
    microprotocols_add(mxDateTime.DateTime_Type, NULL, call);
    call = PyMapping_GetItemString(mod, "TimeFromMx");
    microprotocols_add(mxDateTime.DateTimeDelta_Type, NULL, call);
#endif
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

static void psyco_encodings_fill(PyObject *dict)
{
    encodingPair *enc;

    for (enc = encodings; enc->pgenc != NULL; enc++) {
        PyObject *value = Text_FromUTF8(enc->pyenc);
        PyDict_SetItemString(dict, enc->pgenc, value);
        Py_DECREF(value);
    }
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
    { "psycopg2.Error", &Error, 0, Error_doc },
    { "psycopg2.Warning", &Warning, 0, Warning_doc },
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

static void
psyco_errors_init(void)
{
    /* the names of the exceptions here reflect the oranization of the
       psycopg2 module and not the fact the the original error objects
       live in _psycopg */

    int i;
    PyObject *dict;
    PyObject *base;
    PyObject *str;

    for (i=0; exctable[i].name; i++) {
        dict = PyDict_New();

        if (exctable[i].docstr) {
            str = Text_FromUTF8(exctable[i].docstr);
            PyDict_SetItemString(dict, "__doc__", str);
        }

        if (exctable[i].base == 0) {
            #if PY_MAJOR_VERSION < 3
            base = PyExc_StandardError;
            #else
            /* StandardError is gone in 3.0 */
            base = NULL;
            #endif
        }
        else
            base = *exctable[i].base;

        *exctable[i].exc = PyErr_NewException(exctable[i].name, base, dict);
    }

    /* Make pgerror, pgcode and cursor default to None on psycopg
       error objects.  This simplifies error handling code that checks
       these attributes. */
    PyObject_SetAttrString(Error, "pgerror", Py_None);
    PyObject_SetAttrString(Error, "pgcode", Py_None);
    PyObject_SetAttrString(Error, "cursor", Py_None);
}

void
psyco_errors_fill(PyObject *dict)
{
    PyDict_SetItemString(dict, "Error", Error);
    PyDict_SetItemString(dict, "Warning", Warning);
    PyDict_SetItemString(dict, "InterfaceError", InterfaceError);
    PyDict_SetItemString(dict, "DatabaseError", DatabaseError);
    PyDict_SetItemString(dict, "InternalError", InternalError);
    PyDict_SetItemString(dict, "OperationalError", OperationalError);
    PyDict_SetItemString(dict, "ProgrammingError", ProgrammingError);
    PyDict_SetItemString(dict, "IntegrityError", IntegrityError);
    PyDict_SetItemString(dict, "DataError", DataError);
    PyDict_SetItemString(dict, "NotSupportedError", NotSupportedError);
#ifdef PSYCOPG_EXTENSIONS
    PyDict_SetItemString(dict, "QueryCanceledError", QueryCanceledError);
    PyDict_SetItemString(dict, "TransactionRollbackError",
                         TransactionRollbackError);
#endif
}

void
psyco_errors_set(PyObject *type)
{
    PyObject_SetAttrString(type, "Error", Error);
    PyObject_SetAttrString(type, "Warning", Warning);
    PyObject_SetAttrString(type, "InterfaceError", InterfaceError);
    PyObject_SetAttrString(type, "DatabaseError", DatabaseError);
    PyObject_SetAttrString(type, "InternalError", InternalError);
    PyObject_SetAttrString(type, "OperationalError", OperationalError);
    PyObject_SetAttrString(type, "ProgrammingError", ProgrammingError);
    PyObject_SetAttrString(type, "IntegrityError", IntegrityError);
    PyObject_SetAttrString(type, "DataError", DataError);
    PyObject_SetAttrString(type, "NotSupportedError", NotSupportedError);
#ifdef PSYCOPG_EXTENSIONS
    PyObject_SetAttrString(type, "QueryCanceledError", QueryCanceledError);
    PyObject_SetAttrString(type, "TransactionRollbackError",
                           TransactionRollbackError);
#endif
}

/* psyco_error_new

   Create a new error of the given type with extra attributes. */

void
psyco_set_error(PyObject *exc, cursorObject *curs, const char *msg,
                const char *pgerror, const char *pgcode)
{
    PyObject *t;
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
        return;
    }

    if (err) {
        if (curs) {
            PyObject_SetAttrString(err, "cursor", (PyObject *)curs);
        }

        if (pgerror) {
            t = conn_text_from_chars(conn, pgerror);
            PyObject_SetAttrString(err, "pgerror", t);
            Py_DECREF(t);
        }

        if (pgcode) {
            t = conn_text_from_chars(conn, pgcode);
            PyObject_SetAttrString(err, "pgcode", t);
            Py_DECREF(t);
        }

        PyErr_SetObject(exc, err);
        Py_DECREF(err);
    }
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
    if (can_cache && !cachedType) {
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
        PyErr_Clear();
        rv = Py_None;
        goto exit;
    }
    if (!(nt = PyObject_GetAttrString(coll, "namedtuple"))) {
        Dprintf("psyco_make_description_type: no collections.namedtuple");
        PyErr_Clear();
        rv = Py_None;
        goto exit;
    }

    /* Build the namedtuple */
    rv = PyObject_CallFunction(nt, "ss", "Column",
        "name type_code display_size internal_size precision scale null_ok");

exit:
    Py_XDECREF(coll);
    Py_XDECREF(nt);

    return rv;
}


/** method table and module initialization **/

static PyMethodDef psycopgMethods[] = {
    {"connect",  (PyCFunction)psyco_connect,
     METH_VARARGS|METH_KEYWORDS, psyco_connect_doc},
    {"adapt",  (PyCFunction)psyco_microprotocols_adapt,
     METH_VARARGS, psyco_microprotocols_adapt_doc},

    {"register_type", (PyCFunction)psyco_register_type,
     METH_VARARGS, psyco_register_type_doc},
    {"new_type", (PyCFunction)typecast_from_python,
     METH_VARARGS|METH_KEYWORDS, typecast_from_python_doc},

    {"AsIs",  (PyCFunction)psyco_AsIs,
     METH_VARARGS, psyco_AsIs_doc},
    {"QuotedString",  (PyCFunction)psyco_QuotedString,
     METH_VARARGS, psyco_QuotedString_doc},
    {"Boolean",  (PyCFunction)psyco_Boolean,
     METH_VARARGS, psyco_Float_doc},
    {"Float",  (PyCFunction)psyco_Float,
     METH_VARARGS, psyco_Decimal_doc},
    {"Decimal",  (PyCFunction)psyco_Decimal,
     METH_VARARGS, psyco_Boolean_doc},
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
    Py_TYPE(&cursorType)     = &PyType_Type;
    Py_TYPE(&typecastType)   = &PyType_Type;
    Py_TYPE(&qstringType)    = &PyType_Type;
    Py_TYPE(&binaryType)     = &PyType_Type;
    Py_TYPE(&isqlquoteType)  = &PyType_Type;
    Py_TYPE(&pbooleanType)   = &PyType_Type;
    Py_TYPE(&pfloatType)     = &PyType_Type;
    Py_TYPE(&pdecimalType)   = &PyType_Type;
    Py_TYPE(&asisType)       = &PyType_Type;
    Py_TYPE(&listType)       = &PyType_Type;
    Py_TYPE(&chunkType)      = &PyType_Type;
    Py_TYPE(&NotifyType)     = &PyType_Type;
    Py_TYPE(&XidType)        = &PyType_Type;

    if (PyType_Ready(&connectionType) == -1) goto exit;
    if (PyType_Ready(&cursorType) == -1) goto exit;
    if (PyType_Ready(&typecastType) == -1) goto exit;
    if (PyType_Ready(&qstringType) == -1) goto exit;
    if (PyType_Ready(&binaryType) == -1) goto exit;
    if (PyType_Ready(&isqlquoteType) == -1) goto exit;
    if (PyType_Ready(&pbooleanType) == -1) goto exit;
    if (PyType_Ready(&pfloatType) == -1) goto exit;
    if (PyType_Ready(&pdecimalType) == -1) goto exit;
    if (PyType_Ready(&asisType) == -1) goto exit;
    if (PyType_Ready(&listType) == -1) goto exit;
    if (PyType_Ready(&chunkType) == -1) goto exit;
    if (PyType_Ready(&NotifyType) == -1) goto exit;
    if (PyType_Ready(&XidType) == -1) goto exit;

#ifdef PSYCOPG_EXTENSIONS
    Py_TYPE(&lobjectType)    = &PyType_Type;
    if (PyType_Ready(&lobjectType) == -1) goto exit;
#endif

    /* import mx.DateTime module, if necessary */
#ifdef HAVE_MXDATETIME
    Py_TYPE(&mxdatetimeType) = &PyType_Type;
    if (PyType_Ready(&mxdatetimeType) == -1) goto exit;
    if (mxDateTime_ImportModuleAndAPI() != 0) {
        Dprintf("initpsycopg: why marc hide mx.DateTime again?!");
        PyErr_SetString(PyExc_ImportError, "can't import mx.DateTime module");
        goto exit;
    }
    if (psyco_adapter_mxdatetime_init()) { goto exit; }
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
    psycoEncodings = PyDict_New();
    psyco_encodings_fill(psycoEncodings);
    psyco_null = Bytes_FromString("NULL");
    psyco_DescriptionType = psyco_make_description_type();

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
    PyModule_AddObject(module, "Notify", (PyObject*)&NotifyType);
    PyModule_AddObject(module, "Xid", (PyObject*)&XidType);
#ifdef PSYCOPG_EXTENSIONS
    PyModule_AddObject(module, "lobject", (PyObject*)&lobjectType);
#endif

    /* encodings dictionary in module dictionary */
    PyModule_AddObject(module, "encodings", psycoEncodings);

    /* initialize default set of typecasters */
    typecast_init(dict);

    /* initialize microprotocols layer */
    microprotocols_init(dict);
    psyco_adapters_init(dict);

    /* create a standard set of exceptions and add them to the module's dict */
    psyco_errors_init();
    psyco_errors_fill(dict);

    /* Solve win32 build issue about non-constant initializer element */
    cursorType.tp_alloc = PyType_GenericAlloc;
    binaryType.tp_alloc = PyType_GenericAlloc;
    isqlquoteType.tp_alloc = PyType_GenericAlloc;
    pbooleanType.tp_alloc = PyType_GenericAlloc;
    pfloatType.tp_alloc = PyType_GenericAlloc;
    pdecimalType.tp_alloc = PyType_GenericAlloc;
    connectionType.tp_alloc = PyType_GenericAlloc;
    asisType.tp_alloc = PyType_GenericAlloc;
    qstringType.tp_alloc = PyType_GenericAlloc;
    listType.tp_alloc = PyType_GenericAlloc;
    chunkType.tp_alloc = PyType_GenericAlloc;
    pydatetimeType.tp_alloc = PyType_GenericAlloc;
    NotifyType.tp_alloc = PyType_GenericAlloc;
    XidType.tp_alloc = PyType_GenericAlloc;

#ifdef PSYCOPG_EXTENSIONS
    lobjectType.tp_alloc = PyType_GenericAlloc;
#endif


#ifdef HAVE_MXDATETIME
    mxdatetimeType.tp_alloc = PyType_GenericAlloc;
#endif

    Dprintf("initpsycopg: module initialization complete");

exit:
#if PY_MAJOR_VERSION > 2
    return module;
#else
    return;
#endif
}
