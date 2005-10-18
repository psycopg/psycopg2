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

#include <Python.h>

#ifdef __cplusplus
extern "C" {
#endif

/* type of type-casting functions (both C and Python) */
typedef PyObject *(*typecast_function)(char *, int len, PyObject *);

/** typecast type **/

extern PyTypeObject typecastType;

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
extern PyObject *psyco_types;
extern PyObject *psyco_binary_types;
    
/* the default casting objects, used when no other objects are available */
extern PyObject *psyco_default_cast;
extern PyObject *psyco_default_binary_cast;

/** exported functions **/

/* used by module.c to init the type system and register types */
extern int typecast_init(PyObject *dict);
extern int typecast_add(PyObject *obj, int binary);

/* the C callable typecastObject creator function */
extern PyObject *typecast_from_c(typecastObject_initlist *type, PyObject *d);

/* the python callable typecast creator function */
extern PyObject *typecast_from_python(
    PyObject *self, PyObject *args, PyObject *keywds);

/* the function used to dispatch typecasting calls */
extern PyObject *typecast_cast(
    PyObject *self, char *str, int len, PyObject *curs);
    
#endif /* !defined(PSYCOPG_TYPECAST_H) */
