/* cursor_int.c - code used by the cursor object
 *
 * Copyright (C) 2003-2010 Federico Di Gregorio <fog@debian.org>
 *
 * This file is part of psycopg.
 *
 * psycopg2 is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link this program with the OpenSSL library (or with
 * modified versions of OpenSSL that use the same license as OpenSSL),
 * and distribute linked combinations including the two.
 *
 * You must obey the GNU Lesser General Public License in all respects for
 * all of the code used other than OpenSSL.
 *
 * psycopg2 is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */

#define PSYCOPG_MODULE
#include "psycopg/psycopg.h"

#include "psycopg/cursor.h"
#include "psycopg/pqpath.h"
#include "psycopg/typecast.h"

/* curs_get_cast - return the type caster for an oid.
 *
 * Return the most specific type caster, from cursor to connection to global.
 * If no type caster is found, return the default one.
 *
 * Return a borrowed reference.
 */

BORROWED PyObject *
curs_get_cast(cursorObject *self, PyObject *oid)
{
    PyObject *cast;

    /* cursor lookup */
    if (self->string_types != NULL && self->string_types != Py_None) {
        cast = PyDict_GetItem(self->string_types, oid);
        Dprintf("curs_get_cast:        per-cursor dict: %p", cast);
        if (cast) { return cast; }
    }

    /* connection lookup */
    cast = PyDict_GetItem(self->conn->string_types, oid);
    Dprintf("curs_get_cast:        per-connection dict: %p", cast);
    if (cast) { return cast; }

    /* global lookup */
    cast = PyDict_GetItem(psyco_types, oid);
    Dprintf("curs_get_cast:        global dict: %p", cast);
    if (cast) { return cast; }

    /* fallback */
    return psyco_default_cast;
}

#include <string.h>


/* curs_reset - reset the cursor to a clean state */

void
curs_reset(cursorObject *self)
{
    /* initialize some variables to default values */
    self->notuples = 1;
    self->rowcount = -1;
    self->row = 0;

    Py_CLEAR(self->description);
    Py_CLEAR(self->casts);
}
