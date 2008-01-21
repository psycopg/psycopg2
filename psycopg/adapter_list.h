/* adapter_list.h - definition for the python list types
 *
 * Copyright (C) 2004-2005 Federico Di Gregorio <fog@debian.org>
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

#ifndef PSYCOPG_LIST_H
#define PSYCOPG_LIST_H 1

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "psycopg/config.h"

#ifdef __cplusplus
extern "C" {
#endif

extern HIDDEN PyTypeObject listType;

typedef struct {
    PyObject_HEAD

    PyObject *wrapped;
    PyObject *connection;
    char     *encoding;
} listObject;

HIDDEN PyObject *psyco_List(PyObject *module, PyObject *args);
#define psyco_List_doc \
    "List(list, enc) -> new quoted list"

#ifdef __cplusplus
}
#endif

#endif /* !defined(PSYCOPG_LIST_H) */
