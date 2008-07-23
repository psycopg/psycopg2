/* python.h - python version compatibility stuff
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

#ifndef PSYCOPG_PYTHON_H
#define PSYCOPG_PYTHON_H 1

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <structmember.h>

/* python < 2.2 does not have PyMemeberDef */
#if PY_MAJOR_VERSION == 2 && PY_MINOR_VERSION < 2
#define PyMemberDef memberlist
#endif

/* PyObject_TypeCheck introduced in 2.2 */
#ifndef PyObject_TypeCheck
#define PyObject_TypeCheck(o, t) ((o)->ob_type == (t))
#endif

/* python 2.2 does not have freefunc (it has destructor instead) */
#if PY_MAJOR_VERSION == 2 && PY_MINOR_VERSION < 3
#define freefunc destructor
#endif

/* Py_VISIT and Py_CLEAR introduced in Python 2.4 */
#ifndef Py_VISIT
#define Py_VISIT(op)                                    \
        do {                                            \
                if (op) {                               \
                        int vret = visit((op), arg);    \
                        if (vret)                       \
                                return vret;            \
                }                                       \
        } while (0)
#endif

#ifndef Py_CLEAR
#define Py_CLEAR(op)                            \
        do {                                    \
                if (op) {                       \
                        PyObject *tmp = (PyObject *)(op);       \
                        (op) = NULL;            \
                        Py_DECREF(tmp);         \
                }                               \
        } while (0)
#endif

#endif /* !defined(PSYCOPG_PYTHON_H) */
