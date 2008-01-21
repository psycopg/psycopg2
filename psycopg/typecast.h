/* typecast.h - definitions for typecasters
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

#ifndef PSYCOPG_TYPECAST_H
#define PSYCOPG_TYPECAST_H 1

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "psycopg/config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* type of type-casting functions (both C and Python) */
typedef PyObject *(*typecast_function)(const char *str, Py_ssize_t len,
                                       PyObject *cursor);

/** typecast type **/

extern HIDDEN PyTypeObject typecastType;

typedef struct {
    PyObject_HEAD

    PyObject *name;    /* the name of this type */
    PyObject *values;  /* the different types this instance can match */

    typecast_function  ccast;  /* the C casting function */
    PyObject          *pcast;  /* the python casting function */
    PyObject          *bcast;  /* base cast, used by array typecasters */
} typecastObject;

/* the initialization values are stored here */

typedef struct {
    char *name;
    long int *values;
    typecast_function cast;

    /* base is the base typecaster for arrays */
    char *base;
} typecastObject_initlist;

/* the type dictionary, much faster to access it globally */
extern HIDDEN PyObject *psyco_types;
extern HIDDEN PyObject *psyco_binary_types;

/* the default casting objects, used when no other objects are available */
extern HIDDEN PyObject *psyco_default_cast;
extern HIDDEN PyObject *psyco_default_binary_cast;

/** exported functions **/

/* used by module.c to init the type system and register types */
HIDDEN int typecast_init(PyObject *dict);
HIDDEN int typecast_add(PyObject *obj, PyObject *dict, int binary);

/* the C callable typecastObject creator function */
HIDDEN PyObject *typecast_from_c(typecastObject_initlist *type, PyObject *d);

/* the python callable typecast creator function */
HIDDEN PyObject *typecast_from_python(
    PyObject *self, PyObject *args, PyObject *keywds);

/* the function used to dispatch typecasting calls */
HIDDEN PyObject *typecast_cast(
    PyObject *self, const char *str, Py_ssize_t len, PyObject *curs);

#endif /* !defined(PSYCOPG_TYPECAST_H) */
