/* psycopgmodule.c - psycopg module (will import other C classes)
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

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#define PSYCOPG_MODULE
#include "psycopg/config.h"
#include "psycopg/python.h"
#include "psycopg/psycopg.h"
#include "psycopg/connection.h"
#include "psycopg/cursor.h"
#include "psycopg/lobject.h"
#include "psycopg/typecast.h"
#include "psycopg/microprotocols.h"
#include "psycopg/microprotocols_proto.h"

#include "psycopg/adapter_qstring.h"
#include "psycopg/adapter_binary.h"
#include "psycopg/adapter_pboolean.h"
#include "psycopg/adapter_asis.h"
#include "psycopg/adapter_list.h"
#include "psycopg/typecast_binary.h"

#ifdef HAVE_MXDATETIME
#include <mxDateTime.h>
#include "psycopg/adapter_mxdatetime.h"
HIDDEN mxDateTimeModule_APIObject *mxDateTimeP = NULL;
#endif

/* some module-level variables, like the datetime module */
#ifdef HAVE_PYDATETIME
#include <datetime.h>
#include "psycopg/adapter_datetime.h"
HIDDEN PyObject *pyDateTimeModuleP = NULL;
HIDDEN PyObject *pyDateTypeP = NULL;
HIDDEN PyObject *pyTimeTypeP = NULL;
HIDDEN PyObject *pyDateTimeTypeP = NULL;
HIDDEN PyObject *pyDeltaTypeP = NULL;
#endif

/* pointers to the psycopg.tz classes */
HIDDEN PyObject *pyPsycopgTzModule = NULL;
HIDDEN PyObject *pyPsycopgTzLOCAL = NULL;
HIDDEN PyObject *pyPsycopgTzFixedOffsetTimezone = NULL;

HIDDEN PyObject *psycoEncodings = NULL;

#ifdef PSYCOPG_DEBUG
HIDDEN int psycopg_debug_enabled = 0;
#endif

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

    static char *kwlist[] = {"dsn", "database", "host", "port",
                             "user", "password", "sslmode",
                             "connection_factory", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywds, "|sssOsssO", kwlist,
                                     &dsn_static, &database, &host, &pyport,
                                     &user, &password, &sslmode, &factory)) {
        return NULL;
    }

    if (pyport && PyString_Check(pyport)) {
      PyObject *pyint = PyInt_FromString(PyString_AsString(pyport), NULL, 10);
      if (!pyint) goto fail;
      /* Must use PyInt_AsLong rather than PyInt_AS_LONG, because
       * PyInt_FromString can return a PyLongObject: */
      iport = PyInt_AsLong(pyint);
      Py_DECREF(pyint);
    }
    else if (pyport && PyInt_Check(pyport)) {
      iport = PyInt_AsLong(pyport);
    }
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
      Dprintf("psyco_connect: dsn = '%s'", dsn);

      /* allocate connection, fill with errors and return it */
      if (factory == NULL) factory = (PyObject *)&connectionType;
      conn = PyObject_CallFunction(factory, "s", dsn);
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
"register_type(obj) -> None -- register obj with psycopg type system\n\n" \
":Parameters:\n" \
"  * `obj`: A type adapter created by `new_type()`"

#define typecast_from_python_doc \
"new_type(oids, name, adapter) -> new type object\n\n" \
"Create a new binding object. The object can be used with the\n" \
"`register_type()` function to bind PostgreSQL objects to python objects.\n\n" \
":Parameters:\n" \
"  * `oids`: Tuple of ``oid`` of the PostgreSQL types to convert.\n" \
"  * `name`: Name for the new type\n" \
"  * `adapter`: Callable to perform type conversion.\n" \
"    It must have the signature ``fun(value, cur)`` where ``value`` is\n" \
"    the string representation returned by PostgreSQL (`None` if ``NULL``)\n" \
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

    if (obj != NULL) {
        if (obj->ob_type == &cursorType) {
            _psyco_register_type_set(&(((cursorObject*)obj)->string_types), type);
        }
        else if (obj->ob_type == &connectionType) {
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

    microprotocols_add(&PyFloat_Type, NULL, (PyObject*)&asisType);
    microprotocols_add(&PyInt_Type, NULL, (PyObject*)&asisType);
    microprotocols_add(&PyLong_Type, NULL, (PyObject*)&asisType);

    microprotocols_add(&PyString_Type, NULL, (PyObject*)&qstringType);
    microprotocols_add(&PyUnicode_Type, NULL, (PyObject*)&qstringType);
    microprotocols_add(&PyBuffer_Type, NULL, (PyObject*)&binaryType);
    microprotocols_add(&PyList_Type, NULL, (PyObject*)&listType);

#ifdef HAVE_MXDATETIME
    /* the module has already been initialized, so we can obtain the callable
       objects directly from its dictionary :) */
    call = PyMapping_GetItemString(mod, "TimestampFromMx");
    microprotocols_add(mxDateTimeP->DateTime_Type, NULL, call);
    call = PyMapping_GetItemString(mod, "TimeFromMx");
    microprotocols_add(mxDateTimeP->DateTimeDelta_Type, NULL, call);
#endif

#ifdef HAVE_PYDATETIME
    /* as above, we use the callable objects from the psycopg module */
    call = PyMapping_GetItemString(mod, "DateFromPy");
    microprotocols_add((PyTypeObject*)pyDateTypeP, NULL, call);
    call = PyMapping_GetItemString(mod, "TimeFromPy");
    microprotocols_add((PyTypeObject*)pyTimeTypeP, NULL, call);
    call = PyMapping_GetItemString(mod, "TimestampFromPy");
    microprotocols_add((PyTypeObject*)pyDateTimeTypeP, NULL, call);
    call = PyMapping_GetItemString(mod, "IntervalFromPy");
    microprotocols_add((PyTypeObject*)pyDeltaTypeP, NULL, call);
#endif

#ifdef HAVE_PYBOOL
    microprotocols_add(&PyBool_Type, NULL, (PyObject*)&pbooleanType);
#endif

#ifdef HAVE_DECIMAL
    microprotocols_add((PyTypeObject*)psyco_GetDecimalType(),
                       NULL, (PyObject*)&asisType);
#endif
}

/* psyco_encodings_fill

   Fill the module's postgresql<->python encoding table */

static encodingPair encodings[] = {
    {"SQL_ASCII",    "ascii"},
    {"LATIN1",       "iso8859_1"},
    {"LATIN2",       "iso8859_2"},
    {"LATIN3",       "iso8859_3"},
    {"LATIN4",       "iso8859_4"},
    {"LATIN5",       "iso8859_9"},
    {"LATIN6",       "iso8859_10"},
    {"LATIN7",       "iso8859_13"},
    {"LATIN8",       "iso8859_14"},
    {"LATIN9",       "iso8859_15"},
    {"ISO88591",     "iso8859_1"},
    {"ISO88592",     "iso8859_2"},
    {"ISO88593",     "iso8859_3"},
    {"ISO88595",     "iso8859_5"},
    {"ISO88596",     "iso8859_6"},
    {"ISO88597",     "iso8859_7"},
    {"ISO885913",    "iso8859_13"},
    {"ISO88598",     "iso8859_8"},
    {"ISO88599",     "iso8859_9"},
    {"ISO885914",    "iso8859_14"},
    {"ISO885915",    "iso8859_15"},
    {"UNICODE",      "utf_8"}, /* Not valid in 8.2, backward compatibility */
    {"UTF8",         "utf_8"},
    {"WIN950",       "cp950"},
    {"Windows950",   "cp950"},
    {"BIG5",         "big5"},
    {"EUC_JP",       "euc_jp"},
    {"EUC_KR",       "euc_kr"},
    {"GB18030",      "gb18030"},
    {"GBK",          "gbk"},
    {"WIN936",       "gbk"},
    {"Windows936",   "gbk"},
    {"JOHAB",        "johab"},
    {"KOI8",         "koi8_r"}, /* in PG: KOI8 == KOI8R == KOI8-R == KOI8-U
                                   but in Python there is koi8_r AND koi8_u */
    {"KOI8R",        "koi8_r"},
    {"SJIS",         "cp932"},
    {"Mskanji",      "cp932"},
    {"ShiftJIS",     "cp932"},
    {"WIN932",       "cp932"},
    {"Windows932",   "cp932"},
    {"UHC",          "cp949"},
    {"WIN949",       "cp949"},
    {"Windows949",   "cp949"},
    {"WIN866",       "cp866"},
    {"ALT",          "cp866"},
    {"WIN874",       "cp874"},
    {"WIN1250",      "cp1250"},
    {"WIN1251",      "cp1251"},
    {"WIN",          "cp1251"},
    {"WIN1252",      "cp1252"},
    {"WIN1253",      "cp1253"},
    {"WIN1254",      "cp1254"},
    {"WIN1255",      "cp1255"},
    {"WIN1256",      "cp1256"},
    {"WIN1257",      "cp1257"},
    {"WIN1258",      "cp1258"},
    {"ABC",          "cp1258"},
    {"TCVN",         "cp1258"},
    {"TCVN5712",     "cp1258"},
    {"VSCII",        "cp1258"},

/* those are missing from Python:                */
/*    {"EUC_CN", "?"},                           */
/*    {"EUC_TW", "?"},                           */
/*    {"LATIN10", "?"},                          */
/*    {"ISO885916", "?"},                        */
/*    {"MULE_INTERNAL", "?"},                    */
    {NULL, NULL}
};

static void psyco_encodings_fill(PyObject *dict)
{
    encodingPair *enc;

    for (enc = encodings; enc->pgenc != NULL; enc++) {
        PyObject *value = PyString_FromString(enc->pyenc);
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
      &OperationalError, OperationalError_doc },
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
            str = PyString_FromString(exctable[i].docstr);
            PyDict_SetItemString(dict, "__doc__", str);
        }

        if (exctable[i].base == 0)
            base = PyExc_StandardError;
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
psyco_set_error(PyObject *exc, PyObject *curs, const char *msg,
                const char *pgerror, const char *pgcode)
{
    PyObject *t;

    PyObject *err = PyObject_CallFunction(exc, "s", msg);

    if (err) {
        if (pgerror) {
            t = PyString_FromString(pgerror);
            PyObject_SetAttrString(err, "pgerror", t);
            Py_DECREF(t);
        }

        if (pgcode) {
            t = PyString_FromString(pgcode);
            PyObject_SetAttrString(err, "pgcode", t);
            Py_DECREF(t);
        }

        if (curs)
            PyObject_SetAttrString(err, "cursor", curs);

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
    PyObject *decimalType = NULL;
    static PyObject *cachedType = NULL;

#ifdef HAVE_DECIMAL
    PyObject *decimal = PyImport_ImportModule("decimal");

    /* Use the cached object if running from the main interpreter. */
    int can_cache = psyco_is_main_interp();
    if (can_cache && cachedType) {
        Py_INCREF(cachedType);
        return cachedType;
    }

    /* Get a new reference to the Decimal type. */
    if (decimal) {
        decimalType = PyObject_GetAttrString(decimal, "Decimal");
        Py_DECREF(decimal);
    }
    else {
        PyErr_Clear();
        decimalType = (PyObject *)&PyFloat_Type;
        Py_INCREF(decimalType);
    }

    /* Store the object from future uses. */
    if (can_cache && !cachedType) {
        cachedType = decimalType;
    }

#endif /* HAVE_DECIMAL */

    return decimalType;
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

#ifdef HAVE_PYDATETIME
    {"DateFromPy",  (PyCFunction)psyco_DateFromPy,
     METH_VARARGS, psyco_DateFromPy_doc},
    {"TimeFromPy",  (PyCFunction)psyco_TimeFromPy,
     METH_VARARGS, psyco_TimeFromPy_doc},
    {"TimestampFromPy",  (PyCFunction)psyco_TimestampFromPy,
     METH_VARARGS, psyco_TimestampFromPy_doc},
    {"IntervalFromPy",  (PyCFunction)psyco_IntervalFromPy,
     METH_VARARGS, psyco_IntervalFromPy_doc},
#endif

    {NULL, NULL, 0, NULL}        /* Sentinel */
};

PyMODINIT_FUNC
init_psycopg(void)
{
    static void *PSYCOPG_API[PSYCOPG_API_pointers];

    PyObject *module, *dict;
    PyObject *c_api_object;

#ifdef PSYCOPG_DEBUG
    if (getenv("PSYCOPG_DEBUG"))
      psycopg_debug_enabled = 1;
#endif

    Dprintf("initpsycopg: initializing psycopg %s", PSYCOPG_VERSION);

    /* initialize all the new types and then the module */
    connectionType.ob_type = &PyType_Type;
    cursorType.ob_type     = &PyType_Type;
    typecastType.ob_type   = &PyType_Type;
    qstringType.ob_type    = &PyType_Type;
    binaryType.ob_type     = &PyType_Type;
    isqlquoteType.ob_type  = &PyType_Type;
    asisType.ob_type       = &PyType_Type;
    listType.ob_type       = &PyType_Type;
    chunkType.ob_type      = &PyType_Type;

    if (PyType_Ready(&connectionType) == -1) return;
    if (PyType_Ready(&cursorType) == -1) return;
    if (PyType_Ready(&typecastType) == -1) return;
    if (PyType_Ready(&qstringType) == -1) return;
    if (PyType_Ready(&binaryType) == -1) return;
    if (PyType_Ready(&isqlquoteType) == -1) return;
    if (PyType_Ready(&asisType) == -1) return;
    if (PyType_Ready(&listType) == -1) return;
    if (PyType_Ready(&chunkType) == -1) return;

#ifdef PSYCOPG_EXTENSIONS
    lobjectType.ob_type    = &PyType_Type;
    if (PyType_Ready(&lobjectType) == -1) return;
#endif

#ifdef HAVE_PYBOOL
    pbooleanType.ob_type   = &PyType_Type;
    if (PyType_Ready(&pbooleanType) == -1) return;
#endif

    /* import mx.DateTime module, if necessary */
#ifdef HAVE_MXDATETIME
    mxdatetimeType.ob_type = &PyType_Type;
    if (PyType_Ready(&mxdatetimeType) == -1) return;
    if (mxDateTime_ImportModuleAndAPI() != 0) {
        Dprintf("initpsycopg: why marc hide mx.DateTime again?!");
        PyErr_SetString(PyExc_ImportError, "can't import mx.DateTime module");
        return;
    }
    mxDateTimeP = &mxDateTime;
#endif

    /* import python builtin datetime module, if available */
#ifdef HAVE_PYDATETIME
    pyDateTimeModuleP = PyImport_ImportModule("datetime");
    if (pyDateTimeModuleP == NULL) {
        Dprintf("initpsycopg: can't import datetime module");
        PyErr_SetString(PyExc_ImportError, "can't import datetime module");
        return;
    }
    pydatetimeType.ob_type = &PyType_Type;
    if (PyType_Ready(&pydatetimeType) == -1) return;

    /* now we define the datetime types, this is crazy because python should
       be doing that, not us! */
    pyDateTypeP = PyObject_GetAttrString(pyDateTimeModuleP, "date");
    pyTimeTypeP = PyObject_GetAttrString(pyDateTimeModuleP, "time");
    pyDateTimeTypeP = PyObject_GetAttrString(pyDateTimeModuleP, "datetime");
    pyDeltaTypeP = PyObject_GetAttrString(pyDateTimeModuleP, "timedelta");
#endif

    /* import psycopg2.tz anyway (TODO: replace with C-level module?) */
    pyPsycopgTzModule = PyImport_ImportModule("psycopg2.tz");
    if (pyPsycopgTzModule == NULL) {
        Dprintf("initpsycopg: can't import psycopg2.tz module");
        PyErr_SetString(PyExc_ImportError, "can't import psycopg2.tz module");
        return;
    }
    pyPsycopgTzLOCAL =
        PyObject_GetAttrString(pyPsycopgTzModule, "LOCAL");
    pyPsycopgTzFixedOffsetTimezone =
        PyObject_GetAttrString(pyPsycopgTzModule, "FixedOffsetTimezone");

    /* initialize the module and grab module's dictionary */
    module = Py_InitModule("_psycopg", psycopgMethods);
    dict = PyModule_GetDict(module);

    /* initialize all the module's exported functions */
    /* PyBoxer_API[PyBoxer_Fake_NUM] = (void *)PyBoxer_Fake; */

    /* Create a CObject containing the API pointer array's address */
    c_api_object = PyCObject_FromVoidPtr((void *)PSYCOPG_API, NULL);
    if (c_api_object != NULL)
        PyModule_AddObject(module, "_C_API", c_api_object);

    /* other mixed initializations of module-level variables */
    psycoEncodings = PyDict_New();
    psyco_encodings_fill(psycoEncodings);

    /* set some module's parameters */
    PyModule_AddStringConstant(module, "__version__", PSYCOPG_VERSION);
    PyModule_AddStringConstant(module, "__doc__", "psycopg PostgreSQL driver");
    PyModule_AddObject(module, "apilevel", PyString_FromString(APILEVEL));
    PyModule_AddObject(module, "threadsafety", PyInt_FromLong(THREADSAFETY));
    PyModule_AddObject(module, "paramstyle", PyString_FromString(PARAMSTYLE));

    /* put new types in module dictionary */
    PyModule_AddObject(module, "connection", (PyObject*)&connectionType);
    PyModule_AddObject(module, "cursor", (PyObject*)&cursorType);
    PyModule_AddObject(module, "ISQLQuote", (PyObject*)&isqlquoteType);
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
    connectionType.tp_alloc = PyType_GenericAlloc;
    asisType.tp_alloc = PyType_GenericAlloc;
    qstringType.tp_alloc = PyType_GenericAlloc;
    listType.tp_alloc = PyType_GenericAlloc;
    chunkType.tp_alloc = PyType_GenericAlloc;

#ifdef PSYCOPG_EXTENSIONS
    lobjectType.tp_alloc = PyType_GenericAlloc;
#endif

#ifdef HAVE_PYDATETIME
    pydatetimeType.tp_alloc = PyType_GenericAlloc;
#endif

#ifdef HAVE_MXDATETIME
    mxdatetimeType.tp_alloc = PyType_GenericAlloc;
#endif

    Dprintf("initpsycopg: module initialization complete");
}
