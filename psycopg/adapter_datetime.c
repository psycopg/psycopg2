/* adapter_datetime.c - python date/time objects
 *
 * Copyright (C) 2004 Federico Di Gregorio <fog@debian.org>
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
#include <stringobject.h>
#include <datetime.h>

#include <time.h>
#include <string.h>

#define PSYCOPG_MODULE
#include "psycopg/config.h"
#include "psycopg/python.h"
#include "psycopg/psycopg.h"
#include "psycopg/adapter_datetime.h"
#include "psycopg/microprotocols_proto.h"


/* the pointer to the datetime module API is initialized by the module init
   code, we just need to grab it */
extern HIDDEN PyObject* pyDateTimeModuleP;
extern HIDDEN PyObject *pyDateTypeP;
extern HIDDEN PyObject *pyTimeTypeP;
extern HIDDEN PyObject *pyDateTimeTypeP;
extern HIDDEN PyObject *pyDeltaTypeP;

extern HIDDEN PyObject *pyPsycopgTzModule;
extern HIDDEN PyObject *pyPsycopgTzLOCAL;

/* datetime_str, datetime_getquoted - return result of quoting */

static PyObject *
pydatetime_str(pydatetimeObject *self)
{
    if (self->type <= PSYCO_DATETIME_TIMESTAMP) {
        PyObject *res = NULL;
        PyObject *iso = PyObject_CallMethod(self->wrapped, "isoformat", NULL);
        if (iso) {
            res = PyString_FromFormat("'%s'", PyString_AsString(iso));
            Py_DECREF(iso);
        }
        return res;
    }
    else {
        PyDateTime_Delta *obj = (PyDateTime_Delta*)self->wrapped;

        char buffer[8]; 
        int i;
        int a = obj->microseconds;

        for (i=0; i < 6 ; i++) {
            buffer[5-i] = '0' + (a % 10);
            a /= 10;
        }
        buffer[6] = '\0';

        return PyString_FromFormat("'%d days %d.%s seconds'",
                                   obj->days, obj->seconds, buffer);
    }
}

static PyObject *
pydatetime_getquoted(pydatetimeObject *self, PyObject *args)
{
    if (!PyArg_ParseTuple(args, "")) return NULL;
    return pydatetime_str(self);
}

static PyObject *
pydatetime_conform(pydatetimeObject *self, PyObject *args)
{
    PyObject *res, *proto;

    if (!PyArg_ParseTuple(args, "O", &proto)) return NULL;

    if (proto == (PyObject*)&isqlquoteType)
        res = (PyObject*)self;
    else
        res = Py_None;

    Py_INCREF(res);
    return res;
}

/** the DateTime wrapper object **/

/* object member list */

static struct PyMemberDef pydatetimeObject_members[] = {
    {"adapted", T_OBJECT, offsetof(pydatetimeObject, wrapped), RO},
    {"type", T_INT, offsetof(pydatetimeObject, type), RO},
    {NULL}
};

/* object method table */

static PyMethodDef pydatetimeObject_methods[] = {
    {"getquoted", (PyCFunction)pydatetime_getquoted, METH_VARARGS,
     "getquoted() -> wrapped object value as SQL date/time"},
    {"__conform__", (PyCFunction)pydatetime_conform, METH_VARARGS, NULL},
    {NULL}  /* Sentinel */
};

/* initialization and finalization methods */

static int
pydatetime_setup(pydatetimeObject *self, PyObject *obj, int type)
{
    Dprintf("pydatetime_setup: init datetime object at %p, refcnt = "
        FORMAT_CODE_PY_SSIZE_T,
        self, ((PyObject *)self)->ob_refcnt);

    self->type = type;
    Py_INCREF(obj);
    self->wrapped = obj;

    Dprintf("pydatetime_setup: good pydatetime object at %p, refcnt = "
        FORMAT_CODE_PY_SSIZE_T,
        self, ((PyObject *)self)->ob_refcnt);
    return 0;
}

static int
pydatetime_traverse(PyObject *obj, visitproc visit, void *arg)
{
    pydatetimeObject *self = (pydatetimeObject *)obj;

    Py_VISIT(self->wrapped);
    return 0;
}

static void
pydatetime_dealloc(PyObject* obj)
{
    pydatetimeObject *self = (pydatetimeObject *)obj;

    Py_CLEAR(self->wrapped);

    Dprintf("mpydatetime_dealloc: deleted pydatetime object at %p, "
            "refcnt = " FORMAT_CODE_PY_SSIZE_T, obj, obj->ob_refcnt);

    obj->ob_type->tp_free(obj);
}

static int
pydatetime_init(PyObject *obj, PyObject *args, PyObject *kwds)
{
    PyObject *dt;
    int type = -1; /* raise an error if type was not passed! */

    if (!PyArg_ParseTuple(args, "O|i", &dt, &type))
        return -1;

    return pydatetime_setup((pydatetimeObject *)obj, dt, type);
}

static PyObject *
pydatetime_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    return type->tp_alloc(type, 0);
}

static void
pydatetime_del(PyObject* self)
{
    PyObject_GC_Del(self);
}

static PyObject *
pydatetime_repr(pydatetimeObject *self)
{
    return PyString_FromFormat("<psycopg2._psycopg.datetime object at %p>",
                                self);
}

/* object type */

#define pydatetimeType_doc \
"datetime(datetime, type) -> new datetime wrapper object"

PyTypeObject pydatetimeType = {
    PyObject_HEAD_INIT(NULL)
    0,
    "psycopg2._psycopg.datetime",
    sizeof(pydatetimeObject),
    0,
    pydatetime_dealloc, /*tp_dealloc*/
    0,          /*tp_print*/
    0,          /*tp_getattr*/
    0,          /*tp_setattr*/

    0,          /*tp_compare*/
    (reprfunc)pydatetime_repr, /*tp_repr*/
    0,          /*tp_as_number*/
    0,          /*tp_as_sequence*/
    0,          /*tp_as_mapping*/
    0,          /*tp_hash */

    0,          /*tp_call*/
    (reprfunc)pydatetime_str, /*tp_str*/
    0,          /*tp_getattro*/
    0,          /*tp_setattro*/
    0,          /*tp_as_buffer*/

    Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE|Py_TPFLAGS_HAVE_GC, /*tp_flags*/

    pydatetimeType_doc, /*tp_doc*/

    pydatetime_traverse, /*tp_traverse*/
    0,          /*tp_clear*/

    0,          /*tp_richcompare*/
    0,          /*tp_weaklistoffset*/

    0,          /*tp_iter*/
    0,          /*tp_iternext*/

    /* Attribute descriptor and subclassing stuff */

    pydatetimeObject_methods, /*tp_methods*/
    pydatetimeObject_members, /*tp_members*/
    0,          /*tp_getset*/
    0,          /*tp_base*/
    0,          /*tp_dict*/

    0,          /*tp_descr_get*/
    0,          /*tp_descr_set*/
    0,          /*tp_dictoffset*/

    pydatetime_init, /*tp_init*/
    0, /*tp_alloc  will be set to PyType_GenericAlloc in module init*/
    pydatetime_new, /*tp_new*/
    (freefunc)pydatetime_del, /*tp_free  Low-level free-memory routine */
    0,          /*tp_is_gc For PyObject_IS_GC */
    0,          /*tp_bases*/
    0,          /*tp_mro method resolution order */
    0,          /*tp_cache*/
    0,          /*tp_subclasses*/
    0           /*tp_weaklist*/
};


/** module-level functions **/

#ifdef PSYCOPG_DEFAULT_PYDATETIME

PyObject *
psyco_Date(PyObject *self, PyObject *args)
{
    PyObject *res = NULL;
    int year, month, day;

    PyObject* obj = NULL;

    if (!PyArg_ParseTuple(args, "iii", &year, &month, &day))
        return NULL;

    obj = PyObject_CallFunction(pyDateTypeP, "iii", year, month, day);

    if (obj) {
        res = PyObject_CallFunction((PyObject *)&pydatetimeType,
                                    "Oi", obj, PSYCO_DATETIME_DATE);
        Py_DECREF(obj);
    }

    return res;
}

PyObject *
psyco_Time(PyObject *self, PyObject *args)
{
    PyObject *res = NULL;
    PyObject *tzinfo = NULL;
    int hours, minutes=0;
    double micro, second=0.0;

    PyObject* obj = NULL;

    if (!PyArg_ParseTuple(args, "iid|O", &hours, &minutes, &second,
                          &tzinfo))
        return NULL;

    micro = (second - floor(second)) * 1000000.0;
    second = floor(second);

    if (tzinfo == NULL)
       obj = PyObject_CallFunction(pyTimeTypeP, "iiii",
            hours, minutes, (int)second, (int)round(micro));
    else
       obj = PyObject_CallFunction(pyTimeTypeP, "iiiiO",
            hours, minutes, (int)second, (int)round(micro), tzinfo);

    if (obj) {
        res = PyObject_CallFunction((PyObject *)&pydatetimeType,
                                    "Oi", obj, PSYCO_DATETIME_TIME);
        Py_DECREF(obj);
    }

    return res;
}

PyObject *
psyco_Timestamp(PyObject *self, PyObject *args)
{
    PyObject *res = NULL;
    PyObject *tzinfo = NULL;
    int year, month, day;
    int hour=0, minute=0; /* default to midnight */
    double micro, second=0.0;

    PyObject* obj = NULL;

    if (!PyArg_ParseTuple(args, "lii|iidO", &year, &month, &day,
                          &hour, &minute, &second, &tzinfo))
        return NULL;

    micro = (second - floor(second)) * 1000000.0;
    second = floor(second);

    if (tzinfo == NULL)
        obj = PyObject_CallFunction(pyDateTimeTypeP, "iiiiiii",
            year, month, day, hour, minute, (int)second,
            (int)round(micro));
    else
        obj = PyObject_CallFunction(pyDateTimeTypeP, "iiiiiiiO",
            year, month, day, hour, minute, (int)second,
            (int)round(micro), tzinfo);

    if (obj) {
        res = PyObject_CallFunction((PyObject *)&pydatetimeType,
                                    "Oi", obj, PSYCO_DATETIME_TIMESTAMP);
        Py_DECREF(obj);
    }

    return res;
}

PyObject *
psyco_DateFromTicks(PyObject *self, PyObject *args)
{
    PyObject *res = NULL;
    struct tm tm;
    time_t t;
    double ticks;

    if (!PyArg_ParseTuple(args, "d", &ticks))
        return NULL;

    t = (time_t)round(ticks);
    if (localtime_r(&t, &tm)) {
        args = Py_BuildValue("iii", tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday);
        if (args) {
            res = psyco_Date(self, args);
            Py_DECREF(args);
        }
    }
    return res;
}

PyObject *
psyco_TimeFromTicks(PyObject *self, PyObject *args)
{
    PyObject *res = NULL;
    struct tm tm;
    time_t t;
    double ticks;

    if (!PyArg_ParseTuple(args,"d", &ticks))
        return NULL;

    t = (time_t)round(ticks);
    ticks -= (double)t;
    if (localtime_r(&t, &tm)) {
        args = Py_BuildValue("iid", tm.tm_hour, tm.tm_min,
                          (double)tm.tm_sec + ticks);
        if (args) {
            res = psyco_Time(self, args);
            Py_DECREF(args);
        }
    }
    return res;
}

PyObject *
psyco_TimestampFromTicks(PyObject *self, PyObject *args)
{
    PyObject *res = NULL;
    struct tm tm;
    time_t t;
    double ticks;

    if (!PyArg_ParseTuple(args, "d", &ticks))
        return NULL;

    t = (time_t)round(ticks);
    ticks -= (double)t;
    if (localtime_r(&t, &tm)) {
        PyObject *value = Py_BuildValue("iiiiidO",
            tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
            tm.tm_hour, tm.tm_min,
            (double)tm.tm_sec + ticks,
            pyPsycopgTzLOCAL);
        if (value) {
            /* FIXME: not decref'ing the value here is a memory leak
	       but, on the other hand, if we decref we get a clean nice
	       segfault (on my 64 bit Python 2.4 box). So this leaks
	       will stay until after 2.0.7 when we'll try to plug it */
            res = psyco_Timestamp(self, value);
        }
    }
    
    return res;
}

#endif

PyObject *
psyco_DateFromPy(PyObject *self, PyObject *args)
{
    PyObject *obj;

    if (!PyArg_ParseTuple(args, "O!", pyDateTypeP, &obj))
        return NULL;

    return PyObject_CallFunction((PyObject *)&pydatetimeType, "Oi", obj,
                                 PSYCO_DATETIME_DATE);
}

PyObject *
psyco_TimeFromPy(PyObject *self, PyObject *args)
{
    PyObject *obj;

    if (!PyArg_ParseTuple(args, "O!", pyTimeTypeP, &obj))
        return NULL;

    return PyObject_CallFunction((PyObject *)&pydatetimeType, "Oi", obj,
                                 PSYCO_DATETIME_TIME);
}

PyObject *
psyco_TimestampFromPy(PyObject *self, PyObject *args)
{
    PyObject *obj;

    if (!PyArg_ParseTuple(args, "O!", pyDateTimeTypeP, &obj))
        return NULL;

    return PyObject_CallFunction((PyObject *)&pydatetimeType, "Oi", obj,
                                 PSYCO_DATETIME_TIMESTAMP);
}

PyObject *
psyco_IntervalFromPy(PyObject *self, PyObject *args)
{
    PyObject *obj;

    if (!PyArg_ParseTuple(args, "O!", pyDeltaTypeP, &obj))
        return NULL;

    return PyObject_CallFunction((PyObject *)&pydatetimeType, "Oi", obj,
                                 PSYCO_DATETIME_INTERVAL);
}
