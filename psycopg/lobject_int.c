/* lobject_int.c - code used by the lobject object
 *
 * Copyright (C) 2006 Federico Di Gregorio <fog@debian.org>
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
#include <string.h>

#define PSYCOPG_MODULE
#include "psycopg/config.h"
#include "psycopg/psycopg.h"
#include "psycopg/connection.h"
#include "psycopg/lobject.h"
#include "psycopg/pqpath.h"

#ifdef PSYCOPG_EXTENSIONS

int
lobject_open(lobjectObject *self, connectionObject *conn,
              Oid oid, int mode, Oid new_oid, char *new_file)
{
    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&(self->conn->lock));

    pq_begin(self->conn);

    /* if the oid is InvalidOid we create a new lob before opening it
       or we import a file from the FS, depending on the value of
       new_name */
    if (oid == InvalidOid) {
        if (new_file)
            self->oid = lo_import(self->conn->pgconn, new_file);
        else
            self->oid = lo_create(self->conn->pgconn, new_oid);

        Dprintf("lobject_open: large object created with oid = %d",
                self->oid);

        if (self->oid == InvalidOid) goto end;
        
        mode = INV_WRITE;
    }
    else {
        self->oid = oid;
        if (mode == 0) mode = INV_READ;
    }

    /* if the oid is a real one we try to open with the given mode */
    self->fd = lo_open(self->conn->pgconn, self->oid, mode);
    Dprintf("lobject_open: large object opened with fd = %d",
            self->fd);
 
 end:
    pthread_mutex_unlock(&(self->conn->lock));
    Py_END_ALLOW_THREADS;

    /* here we check for errors before returning 0 */
    if (self->fd == -1 || self->oid == InvalidOid) {
        pq_raise(conn, NULL, NULL, NULL);
        return -1;
    }
    else {
        return 0;
    }
}

#endif

