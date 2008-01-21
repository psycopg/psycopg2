/* cursor.h - definition for the psycopg cursor type
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

#ifndef PSYCOPG_CURSOR_H
#define PSYCOPG_CURSOR_H 1

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <libpq-fe.h>

#include "psycopg/config.h"
#include "psycopg/connection.h"

#ifdef __cplusplus
extern "C" {
#endif

extern HIDDEN PyTypeObject cursorType;

typedef struct {
    PyObject_HEAD

    connectionObject *conn; /* connection owning the cursor */

    int closed:1;            /* 1 if the cursor is closed */
    int notuples:1;          /* 1 if the command was not a SELECT query */
    int needsfetch:1;        /* 1 if a call to pq_fetch is pending */

    long int rowcount;       /* number of rows affected by last execute */
    long int columns;        /* number of columns fetched from the db */
    long int arraysize;      /* how many rows should fetchmany() return */
    long int row;            /* the row counter for fetch*() operations */
    long int mark;           /* transaction marker, copied from conn */

    PyObject *description;   /* read-only attribute: sequence of 7-item
                                sequences.*/

    /* postgres connection stuff */
    PGresult   *pgres;     /* result of last query */
    PyObject   *pgstatus;  /* last message from the server after an execute */
    Oid         lastoid;   /* last oid from an insert or InvalidOid */

    PyObject *casts;      /* an array (tuple) of typecast functions */
    PyObject *caster;     /* the current typecaster object */

    PyObject *copyfile;   /* file-like used during COPY TO/FROM ops */
    Py_ssize_t copysize;   /* size of the copy buffer during COPY TO/FROM ops */
#define DEFAULT_COPYSIZE 16384
#define DEFAULT_COPYBUFF  1024

    PyObject *tuple_factory;    /* factory for result tuples */
    PyObject *tzinfo_factory;   /* factory for tzinfo objects */

    PyObject *query;      /* last query executed */

    char *qattr;          /* quoting attr, used when quoting strings */
    char *notice;         /* a notice from the backend */
    char *name;           /* this cursor name */

    PyObject *string_types;   /* a set of typecasters for string types */
    PyObject *binary_types;   /* a set of typecasters for binary types */

} cursorObject;

/* C-callable functions in cursor_int.c and cursor_ext.c */
HIDDEN void curs_reset(cursorObject *self);

/* exception-raising macros */
#define EXC_IF_CURS_CLOSED(self) \
if ((self)->closed || ((self)->conn && (self)->conn->closed)) { \
    PyErr_SetString(InterfaceError, "cursor already closed");   \
    return NULL; }

#define EXC_IF_NO_TUPLES(self) \
if ((self)->notuples && (self)->name == NULL) {               \
    PyErr_SetString(ProgrammingError, "no results to fetch"); \
    return NULL; }

#define EXC_IF_NO_MARK(self) \
if ((self)->mark != (self)->conn->mark) {                                  \
    PyErr_SetString(ProgrammingError, "named cursor isn't valid anymore"); \
    return NULL; }

#ifdef __cplusplus
}
#endif

#endif /* !defined(PSYCOPG_CURSOR_H) */
