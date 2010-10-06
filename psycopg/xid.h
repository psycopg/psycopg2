/* xid.h - definition for the psycopg cursor type
 *
 * Copyright (C) 2008  James Henstridge <james@jamesh.id.au>
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

#ifndef PSYCOPG_XID_H
#define PSYCOPG_XID_H 1

#include <Python.h>

#include "psycopg/config.h"

extern HIDDEN PyTypeObject XidType;

typedef struct {
  PyObject_HEAD

  /* the PosgreSQL string transaction ID */
  char *pg_xact_id;

  /* the Python-style three-part transaction ID */
  PyObject *format_id;
  PyObject *gtrid;
  PyObject *bqual;

  /* Additional information PostgreSQL exposes about prepared transactions */
  PyObject *prepared;
  PyObject *owner;
  PyObject *database;
} XidObject;

#endif /* PSYCOPG_XID_H */
