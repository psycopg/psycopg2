/* microprotocols.c - minimalist and non-validating protocols implementation
 *
 * Copyright (C) 2003-2004 Federico Di Gregorio <fog@debian.org>
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

#include <Python.h>
#include <structmember.h>

#define PSYCOPG_MODULE
#include "psycopg/config.h"
#include "psycopg/python.h"
#include "psycopg/psycopg.h"
#include "psycopg/cursor.h"
#include "psycopg/microprotocols.h"
#include "psycopg/microprotocols_proto.h"


/** the adapters registry **/

PyObject *psyco_adapters;

/* microprotocols_init - initialize the adapters dictionary */

int
microprotocols_init(PyObject *dict)
{
    /* create adapters dictionary and put it in module namespace */
    if ((psyco_adapters = PyDict_New()) == NULL) {
        return -1;
    }                         

    PyDict_SetItemString(dict, "adapters", psyco_adapters);
    
    return 0;
}


/* microprotocols_add - add a reverse type-caster to the dictionary */

int
microprotocols_add(PyTypeObject *type, PyObject *proto, PyObject *cast)
{
    if (proto == NULL) proto = (PyObject*)&isqlquoteType;

    Dprintf("microprotocols_add: cast %p for (%s, ?)",
            cast, type->tp_name);

    PyDict_SetItem(psyco_adapters,
                   Py_BuildValue("(OO)", (PyObject*)type, proto),
                   cast);
    return 0;
}

/* microprotocols_adapt - adapt an object to the built-in protocol */

PyObject *
microprotocols_adapt(PyObject *obj, PyObject *proto, PyObject *alt)
{
    PyObject *adapter, *key;

    /* we don't check for exact type conformance as specified in PEP 246
       because the ISQLQuote type is abstract and there is no way to get a
       quotable object to be its instance */

    /* look for an adapter in the registry */
    key = Py_BuildValue("(OO)", (PyObject*)obj->ob_type, proto);
    adapter = PyDict_GetItem(psyco_adapters, key);
    Py_DECREF(key);
    if (adapter) {
        PyObject *adapted = PyObject_CallFunctionObjArgs(adapter, obj, NULL);
        return adapted;
    }

    /* try to have the protocol adapt this object*/
    if (PyObject_HasAttrString(proto, "__adapt__")) {
        PyObject *adapted = PyObject_CallMethod(proto, "__adapt__", "O", obj);
        if (adapted && adapted != Py_None) return adapted;
        if (PyErr_Occurred() && !PyErr_ExceptionMatches(PyExc_TypeError))
            return NULL;
    }

    /* and finally try to have the object adapt itself */
    if (PyObject_HasAttrString(obj, "__conform__")) {
        PyObject *adapted = PyObject_CallMethod(proto, "__conform__","O", obj);
        if (adapted && adapted != Py_None) return adapted;
        if (PyErr_Occurred() && !PyErr_ExceptionMatches(PyExc_TypeError))
            return NULL;
    }
            
    /* else set the right exception and return NULL */
    PyErr_SetString(ProgrammingError, "can't adapt");
    return NULL;
}

/** module-level functions **/

PyObject *
psyco_microprotocols_adapt(cursorObject *self, PyObject *args)
{
    PyObject *obj, *alt = NULL;
    PyObject *proto = (PyObject*)&isqlquoteType;

    if (!PyArg_ParseTuple(args, "O|OO", &obj, &proto, &alt)) return NULL;
    return microprotocols_adapt(obj, proto, alt);
}
