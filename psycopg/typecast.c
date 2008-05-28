/* typecast.c - basic utility functions related to typecasting
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
#include <structmember.h>

#define PSYCOPG_MODULE
#include "psycopg/config.h"
#include "psycopg/psycopg.h"
#include "psycopg/python.h"
#include "psycopg/typecast.h"
#include "psycopg/cursor.h"

/* usefull function used by some typecasters */

static const char *
skip_until_space(const char *s)
{
    while (*s && *s != ' ') s++;
    return s;
}

static const char *
skip_until_space2(const char *s, Py_ssize_t *len)
{
    while (*len > 0 && *s && *s != ' ') {
        s++; (*len)--;
    }
    return s;
}

static int
typecast_parse_date(const char* s, const char** t, Py_ssize_t* len,
                     int* year, int* month, int* day)
{
    int acc = -1, cz = 0;

    Dprintf("typecast_parse_date: len = " FORMAT_CODE_PY_SSIZE_T ", s = %s",
      *len, s);

    while (cz < 3 && *len > 0 && *s) {
        switch (*s) {
        case '-':
        case ' ':
        case 'T':
            if (cz == 0) *year = acc;
            else if (cz == 1) *month = acc;
            else if (cz == 2) *day = acc;
            acc = -1; cz++;
            break;
        default:
            acc = (acc == -1 ? 0 : acc*10) + ((int)*s - (int)'0');
            break;
        }

        s++; (*len)--;
    }

    if (acc != -1) {
        *day = acc;
        cz += 1;
    }

    /* Is this a BC date?  If so, adjust the year value.  Note that
     * mx.DateTime numbers BC dates from zero rather than one.  The
     * Python datetime module does not support BC dates at all. */
    if (*len >= 2 && s[*len-2] == 'B' && s[*len-1] == 'C')
        *year = 1 - (*year);

    if (t != NULL) *t = s;

    return cz;
}

static int
typecast_parse_time(const char* s, const char** t, Py_ssize_t* len,
                     int* hh, int* mm, int* ss, int* us, int* tz)
{
    int acc = -1, cz = 0;
    int tzs = 1, tzhh = 0, tzmm = 0;
    int usd = 0;

    /* sets microseconds and timezone to 0 because they may be missing */
    *us = *tz = 0;

    Dprintf("typecast_parse_time: len = " FORMAT_CODE_PY_SSIZE_T ", s = %s",
      *len, s);

    while (cz < 6 && *len > 0 && *s) {
        switch (*s) {
        case ':':
            if (cz == 0) *hh = acc;
            else if (cz == 1) *mm = acc;
            else if (cz == 2) *ss = acc;
            else if (cz == 3) *us = acc;
            else if (cz == 4) tzhh = acc;
            acc = -1; cz++;
            break;
        case '.':
            /* we expect seconds and if we don't get them we return an error */
            if (cz != 2) return -1;
            *ss = acc;
            acc = -1; cz++;
            break;
        case '+':
        case '-':
            /* seconds or microseconds here, anything else is an error */
            if (cz < 2 || cz > 3) return -1;
            if (*s == '-') tzs = -1;
            if      (cz == 2) *ss = acc;
            else if (cz == 3) *us = acc;
            acc = -1; cz = 4;
            break;
        case ' ':
        case 'B':
        case 'C':
            /* Ignore the " BC" suffix, if passed -- it is handled
             * when parsing the date portion. */
            break;
        default:
            acc = (acc == -1 ? 0 : acc*10) + ((int)*s - (int)'0');
            if (cz == 3) usd += 1;
            break;
        }

        s++; (*len)--;
    }

    if (acc != -1) {
        if (cz == 0)      { *hh = acc; cz += 1; }
        else if (cz == 1) { *mm = acc; cz += 1; }
        else if (cz == 2) { *ss = acc; cz += 1; }
        else if (cz == 3) { *us = acc; cz += 1; }
        else if (cz == 4) { tzhh = acc; cz += 1; }
        else if (cz == 5)   tzmm = acc;
    }
    if (t != NULL) *t = s;

    *tz = tzs * tzhh*60 + tzmm;

    if (*us != 0) {
        while (usd++ < 6) *us *= 10;
    }

    return cz;
}

/** include casting objects **/
#include "psycopg/typecast_basic.c"
#include "psycopg/typecast_binary.c"

#ifdef HAVE_MXDATETIME
#include "psycopg/typecast_mxdatetime.c"
#endif

#ifdef HAVE_PYDATETIME
#include "psycopg/typecast_datetime.c"
#endif

#include "psycopg/typecast_array.c"
#include "psycopg/typecast_builtins.c"


/* a list of initializers, used to make the typecasters accessible anyway */
#ifdef HAVE_PYDATETIME
static typecastObject_initlist typecast_pydatetime[] = {
    {"PYDATETIME", typecast_DATETIME_types, typecast_PYDATETIME_cast},
    {"PYTIME", typecast_TIME_types, typecast_PYTIME_cast},
    {"PYDATE", typecast_DATE_types, typecast_PYDATE_cast},
    {"PYINTERVAL", typecast_INTERVAL_types, typecast_PYINTERVAL_cast},
    {NULL, NULL, NULL}
};
#endif

/* a list of initializers, used to make the typecasters accessible anyway */
#ifdef HAVE_MXDATETIME
static typecastObject_initlist typecast_mxdatetime[] = {
    {"MXDATETIME", typecast_DATETIME_types, typecast_MXDATE_cast},
    {"MXTIME", typecast_TIME_types, typecast_MXTIME_cast},
    {"MXDATE", typecast_DATE_types, typecast_MXDATE_cast},
    {"MXINTERVAL", typecast_INTERVAL_types, typecast_MXINTERVAL_cast},
    {NULL, NULL, NULL}
};
#endif


/** the type dictionary and associated functions **/

PyObject *psyco_types;
PyObject *psyco_default_cast;
PyObject *psyco_binary_types;
PyObject *psyco_default_binary_cast;

static long int typecast_default_DEFAULT[] = {0};
static typecastObject_initlist typecast_default = {
    "DEFAULT", typecast_default_DEFAULT, typecast_STRING_cast};


/* typecast_init - initialize the dictionary and create default types */

int
typecast_init(PyObject *dict)
{
    int i;

    /* create type dictionary and put it in module namespace */
    psyco_types = PyDict_New();
    psyco_binary_types = PyDict_New();

    if (!psyco_types || !psyco_binary_types) {
        Py_XDECREF(psyco_types);
        Py_XDECREF(psyco_binary_types);
        return -1;
    }

    PyDict_SetItemString(dict, "string_types", psyco_types);
    PyDict_SetItemString(dict, "binary_types", psyco_binary_types);

    /* insert the cast types into the 'types' dictionary and register them in
       the module dictionary */
    for (i = 0; typecast_builtins[i].name != NULL; i++) {
        typecastObject *t;

        Dprintf("typecast_init: initializing %s", typecast_builtins[i].name);

        t = (typecastObject *)typecast_from_c(&(typecast_builtins[i]), dict);
        if (t == NULL) return -1;
        if (typecast_add((PyObject *)t, NULL, 0) != 0) return -1;

        PyDict_SetItem(dict, t->name, (PyObject *)t);

        /* export binary object */
        if (typecast_builtins[i].values == typecast_BINARY_types) {
            psyco_default_binary_cast = (PyObject *)t;
        }
    }

    /* create and save a default cast object (but does not register it) */
    psyco_default_cast = typecast_from_c(&typecast_default, dict);

    /* register the date/time typecasters with their original names */
#ifdef HAVE_MXDATETIME
    for (i = 0; typecast_mxdatetime[i].name != NULL; i++) {
        typecastObject *t;
        Dprintf("typecast_init: initializing %s", typecast_mxdatetime[i].name);
        t = (typecastObject *)typecast_from_c(&(typecast_mxdatetime[i]), dict);
        if (t == NULL) return -1;
        PyDict_SetItem(dict, t->name, (PyObject *)t);
    }
#endif
#ifdef HAVE_PYDATETIME
    for (i = 0; typecast_pydatetime[i].name != NULL; i++) {
        typecastObject *t;
        Dprintf("typecast_init: initializing %s", typecast_pydatetime[i].name);
        t = (typecastObject *)typecast_from_c(&(typecast_pydatetime[i]), dict);
        if (t == NULL) return -1;
        PyDict_SetItem(dict, t->name, (PyObject *)t);
    }
#endif

    return 0;
}

/* typecast_add - add a type object to the dictionary */
int
typecast_add(PyObject *obj, PyObject *dict, int binary)
{
    PyObject *val;
    Py_ssize_t len, i;

    typecastObject *type = (typecastObject *)obj;

    Dprintf("typecast_add: object at %p, values refcnt = "
        FORMAT_CODE_PY_SSIZE_T,
        obj, type->values->ob_refcnt
      );

    if (dict == NULL)
        dict = (binary ? psyco_binary_types : psyco_types);

    len = PyTuple_Size(type->values);
    for (i = 0; i < len; i++) {
        val = PyTuple_GetItem(type->values, i);
        Dprintf("typecast_add:     adding val: %ld", PyInt_AsLong(val));
        PyDict_SetItem(dict, val, obj);
    }

    Dprintf("typecast_add:     base caster: %p", type->bcast);

    return 0;
}


/** typecast type **/

#define OFFSETOF(x) offsetof(typecastObject, x)

static int
typecast_cmp(PyObject *obj1, PyObject* obj2)
{
    typecastObject *self = (typecastObject*)obj1;
    typecastObject *other = NULL;
    PyObject *number = NULL;
    Py_ssize_t i, j;
    int res = -1;

    if (PyObject_TypeCheck(obj2, &typecastType)) {
        other = (typecastObject*)obj2;
    }
    else {
        number = PyNumber_Int(obj2);
    }

    Dprintf("typecast_cmp: other = %p, number = %p", other, number);

    for (i=0; i < PyObject_Length(self->values) && res == -1; i++) {
        long int val = PyInt_AsLong(PyTuple_GET_ITEM(self->values, i));

        if (other != NULL) {
            for (j=0; j < PyObject_Length(other->values); j++) {
                if (PyInt_AsLong(PyTuple_GET_ITEM(other->values, j)) == val) {
                    res = 0; break;
                }
            }
        }

        else if (number != NULL) {
            if (PyInt_AsLong(number) == val) {
                res = 0; break;
            }
        }
    }

    Py_XDECREF(number);
    return res;
}

static PyObject*
typecast_richcompare(PyObject *obj1, PyObject* obj2, int opid)
{
    PyObject *result = NULL;
    int res = typecast_cmp(obj1, obj2);

    if (PyErr_Occurred()) return NULL;

    if ((opid == Py_EQ && res == 0) || (opid != Py_EQ && res != 0))
        result = Py_True;
    else
        result = Py_False;

    Py_INCREF(result);
    return result;
}

static struct PyMemberDef typecastObject_members[] = {
    {"name", T_OBJECT, OFFSETOF(name), RO},
    {"values", T_OBJECT, OFFSETOF(values), RO},
    {NULL}
};

static void
typecast_dealloc(PyObject *obj)
{
    typecastObject *self = (typecastObject*)obj;

    Py_CLEAR(self->values);
    Py_CLEAR(self->name);
    Py_CLEAR(self->pcast);

    obj->ob_type->tp_free(obj);
}

static int
typecast_traverse(PyObject *obj, visitproc visit, void *arg)
{
    typecastObject *self = (typecastObject*)obj;

    Py_VISIT(self->values);
    Py_VISIT(self->name);
    Py_VISIT(self->pcast);
    return 0;
}

static void
typecast_del(void *self)
{
    PyObject_GC_Del(self);
}

static PyObject *
typecast_call(PyObject *obj, PyObject *args, PyObject *kwargs)
{
    PyObject *string, *cursor;

    if (!PyArg_ParseTuple(args, "OO", &string, &cursor)) {
        return NULL;
    }

    return typecast_cast(obj,
                         PyString_AsString(string), PyString_Size(string),
                         cursor);
}

PyTypeObject typecastType = {
    PyObject_HEAD_INIT(NULL)
    0,
    "psycopg2._psycopg.type",
    sizeof(typecastObject),
    0,

    typecast_dealloc, /*tp_dealloc*/
    0,          /*tp_print*/
    0,          /*tp_getattr*/
    0,          /*tp_setattr*/
    typecast_cmp, /*tp_compare*/
    0,          /*tp_repr*/
    0,          /*tp_as_number*/
    0,          /*tp_as_sequence*/
    0,          /*tp_as_mapping*/
    0,          /*tp_hash */

    typecast_call, /*tp_call*/
    0,          /*tp_str*/
    0,          /*tp_getattro*/
    0,          /*tp_setattro*/
    0,          /*tp_as_buffer*/

    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_RICHCOMPARE |
      Py_TPFLAGS_HAVE_GC, /*tp_flags*/
    "psycopg type-casting object", /*tp_doc*/

    typecast_traverse, /*tp_traverse*/
    0,          /*tp_clear*/

    typecast_richcompare, /*tp_richcompare*/
    0,          /*tp_weaklistoffset*/

    0,          /*tp_iter*/
    0,          /*tp_iternext*/

    /* Attribute descriptor and subclassing stuff */

    0, /*tp_methods*/
    typecastObject_members, /*tp_members*/
    0,          /*tp_getset*/
    0,          /*tp_base*/
    0,          /*tp_dict*/

    0,          /*tp_descr_get*/
    0,          /*tp_descr_set*/
    0,          /*tp_dictoffset*/

    0, /*tp_init*/
    0, /*tp_alloc  will be set to PyType_GenericAlloc in module init*/
    0, /*tp_new*/
    typecast_del, /*tp_free  Low-level free-memory routine */
    0,          /*tp_is_gc For PyObject_IS_GC */
    0,          /*tp_bases*/
    0,          /*tp_mro method resolution order */
    0,          /*tp_cache*/
    0,          /*tp_subclasses*/
    0           /*tp_weaklist*/
};

static PyObject *
typecast_new(PyObject *name, PyObject *values, PyObject *cast, PyObject *base)
{
    typecastObject *obj;

    obj = PyObject_GC_New(typecastObject, &typecastType);
    if (obj == NULL) return NULL;

    Dprintf("typecast_new: new type at = %p, refcnt = " FORMAT_CODE_PY_SSIZE_T,
      obj, obj->ob_refcnt);

    Py_INCREF(values);
    obj->values = values;

    if (name) {
        Py_INCREF(name);
        obj->name = name;
    }
    else {
        Py_INCREF(Py_None);
        obj->name = Py_None;
    }

    obj->pcast = NULL;
    obj->ccast = NULL;
    obj->bcast = base;

    if (obj->bcast) Py_INCREF(obj->bcast);

    /* FIXME: raise an exception when None is passed as Python caster */
    if (cast && cast != Py_None) {
        Py_INCREF(cast);
        obj->pcast = cast;
    }

    PyObject_GC_Track(obj);

    Dprintf("typecast_new: typecast object created at %p", obj);

    return (PyObject *)obj;
}

PyObject *
typecast_from_python(PyObject *self, PyObject *args, PyObject *keywds)
{
    PyObject *v, *name = NULL, *cast = NULL, *base = NULL;

    static char *kwlist[] = {"values", "name", "castobj", "baseobj", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywds, "O!|O!OO", kwlist,
                                     &PyTuple_Type, &v,
                                     &PyString_Type, &name,
                                     &cast, &base)) {
        return NULL;
    }

    return typecast_new(name, v, cast, base);
}

PyObject *
typecast_from_c(typecastObject_initlist *type, PyObject *dict)
{
    PyObject *name = NULL, *values = NULL, *base = NULL;
    typecastObject *obj = NULL;
    Py_ssize_t i, len = 0;

    /* before doing anything else we look for the base */
    if (type->base) {
        /* NOTE: base is a borrowed reference! */
        base = PyDict_GetItemString(dict, type->base);
        if (!base) {
            PyErr_Format(Error, "typecast base not found: %s", type->base);
            goto end;
        }
    }

    name = PyString_FromString(type->name);
    if (!name) goto end;

    while (type->values[len] != 0) len++;

    values = PyTuple_New(len);
    if (!values) goto end;

    for (i = 0; i < len ; i++) {
        PyTuple_SET_ITEM(values, i, PyInt_FromLong(type->values[i]));
    }

    obj = (typecastObject *)typecast_new(name, values, NULL, base);

    if (obj) {
        obj->ccast = type->cast;
        obj->pcast = NULL;
    }

 end:
    Py_XDECREF(values);
    Py_XDECREF(name);
    return (PyObject *)obj;
}

PyObject *
typecast_cast(PyObject *obj, const char *str, Py_ssize_t len, PyObject *curs)
{
    PyObject *old, *res = NULL;
    typecastObject *self = (typecastObject *)obj;

    /* we don't incref, the caster *can't* die at this point */
    old = ((cursorObject*)curs)->caster;
    ((cursorObject*)curs)->caster = obj;

    if (self->ccast) {
        res = self->ccast(str, len, curs);
    }
    else if (self->pcast) {
        res = PyObject_CallFunction(self->pcast, "s#O", str, len, curs);
    }
    else {
        PyErr_SetString(Error, "internal error: no casting function found");
    }

    ((cursorObject*)curs)->caster = old;

    return res;
}
