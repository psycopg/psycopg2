/* microprotocols.c - definitions for minimalist and non-validating protocols
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

#ifndef PSYCOPG_MICROPROTOCOLS_H
#define PSYCOPG_MICROPROTOCOLS_H 1

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "psycopg/config.h"
#include "psycopg/connection.h"
#include "psycopg/cursor.h"

#ifdef __cplusplus
extern "C" {
#endif

/** adapters registry **/

extern HIDDEN PyObject *psyco_adapters;

/** the names of the three mandatory methods **/

#define MICROPROTOCOLS_GETQUOTED_NAME "getquoted"
#define MICROPROTOCOLS_GETSTRING_NAME "getstring"
#define MICROPROTOCOLS_GETBINARY_NAME "getbinary"

/** exported functions **/

/* used by module.c to init the microprotocols system */
HIDDEN int microprotocols_init(PyObject *dict);
HIDDEN int microprotocols_add(
    PyTypeObject *type, PyObject *proto, PyObject *cast);

HIDDEN PyObject *microprotocols_adapt(
    PyObject *obj, PyObject *proto, PyObject *alt);
HIDDEN PyObject *microprotocol_getquoted(
    PyObject *obj, connectionObject *conn);

HIDDEN PyObject *
    psyco_microprotocols_adapt(cursorObject *self, PyObject *args);
#define psyco_microprotocols_adapt_doc \
    "adapt(obj, protocol, alternate) -> object -- adapt obj to given protocol"

#endif /* !defined(PSYCOPG_MICROPROTOCOLS_H) */
