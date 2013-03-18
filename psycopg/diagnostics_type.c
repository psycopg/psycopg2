/* diagnostics.c - present information from libpq error responses
 *
 * Copyright (C) 2013 Matthew Woodcraft <matthew@woodcraft.me.uk>
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

#include "psycopg/diagnostics.h"
#include "psycopg/cursor.h"

/* These are new in PostgreSQL 9.3. Defining them here so that psycopg2 can
 * use them with a 9.3+ server even if compiled against pre-9.3 headers. */
#ifndef PG_DIAG_SCHEMA_NAME
#define PG_DIAG_SCHEMA_NAME     's'
#endif
#ifndef PG_DIAG_TABLE_NAME
#define PG_DIAG_TABLE_NAME      't'
#endif
#ifndef PG_DIAG_COLUMN_NAME
#define PG_DIAG_COLUMN_NAME     'c'
#endif
#ifndef PG_DIAG_DATATYPE_NAME
#define PG_DIAG_DATATYPE_NAME   'd'
#endif
#ifndef PG_DIAG_CONSTRAINT_NAME
#define PG_DIAG_CONSTRAINT_NAME 'n'
#endif


/* Retrieve an error string from the exception's cursor.
 *
 * If the cursor or its result isn't available, return None.
 */
static PyObject *
psyco_diagnostics_get_field(diagnosticsObject *self, void *closure)
{
    // closure contains the field code.
    PyObject *rv = NULL;
    PyObject *curs = NULL;
    const char* errortext;
    if (!(curs = PyObject_GetAttrString(self->err, "cursor")) ||
        !PyObject_TypeCheck(curs, &cursorType) ||
        ((cursorObject *)curs)->pgres == NULL) {
        goto exit;
    }
    errortext = PQresultErrorField(
        ((cursorObject *)curs)->pgres, (Py_intptr_t) closure);
    if (errortext) {
        rv = conn_text_from_chars(((cursorObject *)curs)->conn, errortext);
    }
exit:
    if (!rv) {
        rv = Py_None;
        Py_INCREF(rv);
    }
    Py_XDECREF(curs);
    return rv;
}


/* object calculated member list */
static struct PyGetSetDef diagnosticsObject_getsets[] = {
    { "severity", (getter)psyco_diagnostics_get_field, NULL,
      NULL, (void*) PG_DIAG_SEVERITY },
    { "sqlstate", (getter)psyco_diagnostics_get_field, NULL,
      NULL, (void*) PG_DIAG_SQLSTATE },
    { "message_primary", (getter)psyco_diagnostics_get_field, NULL,
      NULL, (void*) PG_DIAG_MESSAGE_PRIMARY },
    { "message_detail", (getter)psyco_diagnostics_get_field, NULL,
      NULL, (void*) PG_DIAG_MESSAGE_DETAIL },
    { "message_hint", (getter)psyco_diagnostics_get_field, NULL,
      NULL, (void*) PG_DIAG_MESSAGE_HINT },
    { "statement_position", (getter)psyco_diagnostics_get_field, NULL,
      NULL, (void*) PG_DIAG_STATEMENT_POSITION },
    { "internal_position", (getter)psyco_diagnostics_get_field, NULL,
      NULL, (void*) PG_DIAG_INTERNAL_POSITION },
    { "internal_query", (getter)psyco_diagnostics_get_field, NULL,
      NULL, (void*) PG_DIAG_INTERNAL_QUERY },
    { "context", (getter)psyco_diagnostics_get_field, NULL,
      NULL, (void*) PG_DIAG_CONTEXT },
    { "schema_name", (getter)psyco_diagnostics_get_field, NULL,
      NULL, (void*) PG_DIAG_SCHEMA_NAME },
    { "table_name", (getter)psyco_diagnostics_get_field, NULL,
      NULL, (void*) PG_DIAG_TABLE_NAME },
    { "column_name", (getter)psyco_diagnostics_get_field, NULL,
      NULL, (void*) PG_DIAG_COLUMN_NAME },
    { "datatype_name", (getter)psyco_diagnostics_get_field, NULL,
      NULL, (void*) PG_DIAG_DATATYPE_NAME },
    { "constraint_name", (getter)psyco_diagnostics_get_field, NULL,
      NULL, (void*) PG_DIAG_CONSTRAINT_NAME },
    { "source_file", (getter)psyco_diagnostics_get_field, NULL,
      NULL, (void*) PG_DIAG_SOURCE_FILE },
    { "source_line", (getter)psyco_diagnostics_get_field, NULL,
      NULL, (void*) PG_DIAG_SOURCE_LINE },
    { "source_function", (getter)psyco_diagnostics_get_field, NULL,
      NULL, (void*) PG_DIAG_SOURCE_FUNCTION },
    {NULL}
};

/* initialization and finalization methods */

static int
diagnostics_setup(diagnosticsObject *self, PyObject *err)
{
    self->err = err;
    Py_INCREF(err);

    return 0;
}

static void
diagnostics_dealloc(PyObject* obj)
{
    diagnosticsObject *self = (diagnosticsObject *)obj;

    Py_XDECREF(self->err);

    Py_TYPE(obj)->tp_free(obj);
}

static int
diagnostics_init(PyObject *obj, PyObject *args, PyObject *kwds)
{
    PyObject *err = NULL;

    if (!PyArg_ParseTuple(args, "O", &err))
        return -1;

    return diagnostics_setup((diagnosticsObject *)obj, err);
}

static PyObject *
diagnostics_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    return type->tp_alloc(type, 0);
}

static void
diagnostics_del(PyObject* self)
{
    PyObject_Del(self);
}


/* object type */

#define diagnosticsType_doc \
"Details from a database error report."

PyTypeObject diagnosticsType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "psycopg2._psycopg.Diagnostics",
    sizeof(diagnosticsObject),
    0,
    diagnostics_dealloc, /*tp_dealloc*/
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
    diagnosticsType_doc, /*tp_doc*/

    0,          /*tp_traverse*/
    0,          /*tp_clear*/

    0,          /*tp_richcompare*/
    0,          /*tp_weaklistoffset*/

    0,          /*tp_iter*/
    0,          /*tp_iternext*/

    /* Attribute descriptor and subclassing stuff */

    0,          /*tp_methods*/
    0,          /*tp_members*/
    diagnosticsObject_getsets, /*tp_getset*/
    0,          /*tp_base*/
    0,          /*tp_dict*/

    0,          /*tp_descr_get*/
    0,          /*tp_descr_set*/
    0,          /*tp_dictoffset*/

    diagnostics_init, /*tp_init*/
    0, /*tp_alloc  will be set to PyType_GenericAlloc in module init*/
    diagnostics_new, /*tp_new*/
    (freefunc)diagnostics_del, /*tp_free  Low-level free-memory routine */
    0,          /*tp_is_gc For PyObject_IS_GC */
    0,          /*tp_bases*/
    0,          /*tp_mro method resolution order */
    0,          /*tp_cache*/
    0,          /*tp_subclasses*/
    0           /*tp_weaklist*/
};
