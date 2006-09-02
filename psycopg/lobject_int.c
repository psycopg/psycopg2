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

/* lobject_open - create a new/open an existing lo */

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

    /* if the oid is a real one we try to open with the given mode,
       unless the mode is -1, meaning "don't open!" */
    if (mode != -1) {
        self->fd = lo_open(self->conn->pgconn, self->oid, mode);
        Dprintf("lobject_open: large object opened with fd = %d",
            self->fd);
    }
    else {
        /* this is necessary to make sure no function that needs and
	   fd is called on unopened lobjects */
        self->closed = 1;
    }

 end:
    pthread_mutex_unlock(&(self->conn->lock));
    Py_END_ALLOW_THREADS;

    /* here we check for errors before returning 0 */
    if ((self->fd == -1 && mode != -1) || self->oid == InvalidOid) {
        pq_raise(conn, NULL, NULL, NULL);
        return -1;
    }
    else {
        return 0;
    }
}

/* lobject_unlink - remove an lo from database */

int
lobject_unlink(lobjectObject *self)
{
    int res;

    /* first we make sure the lobject is closed and then we unlink */
    lobject_close(self);
    
    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&(self->conn->lock));

    pq_begin(self->conn);

    res = lo_unlink(self->conn->pgconn, self->oid);

    pthread_mutex_unlock(&(self->conn->lock));
    Py_END_ALLOW_THREADS;

    if (res == -1)
        pq_raise(self->conn, NULL, NULL, NULL);
    return res;
}

/* lobject_close - close an existing lo */

void
lobject_close(lobjectObject *self)
{
    if (self->conn->isolation_level > 0
        && self->conn->mark == self->mark) {
        if (self->fd != -1)
            lo_close(self->conn->pgconn, self->fd);
    }
}

/* lobject_write - write bytes to a lo */

size_t
lobject_write(lobjectObject *self, char *buf, size_t len)
{
    size_t written;

    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&(self->conn->lock));

    written = lo_write(self->conn->pgconn, self->fd, buf, len);

    pthread_mutex_unlock(&(self->conn->lock));
    Py_END_ALLOW_THREADS;

    if (written < 0)
        pq_raise(self->conn, NULL, NULL, NULL);
    return written;
}

/* lobject_read - read bytes from a lo */

size_t
lobject_read(lobjectObject *self, char *buf, size_t len)
{
    size_t readed;

    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&(self->conn->lock));

    readed = lo_read(self->conn->pgconn, self->fd, buf, len);

    pthread_mutex_unlock(&(self->conn->lock));
    Py_END_ALLOW_THREADS;

    if (readed < 0)
        pq_raise(self->conn, NULL, NULL, NULL);
    return readed;
}

/* lobject_seek - move the current position in the lo */

int
lobject_seek(lobjectObject *self, int pos, int whence)
{
    int where;

    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&(self->conn->lock));

    where = lo_lseek(self->conn->pgconn, self->fd, pos, whence);

    pthread_mutex_unlock(&(self->conn->lock));
    Py_END_ALLOW_THREADS;

    if (where < 0)
        pq_raise(self->conn, NULL, NULL, NULL);
    return where;
}

/* lobject_tell - tell the current position in the lo */

int
lobject_tell(lobjectObject *self)
{
    int where;

    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&(self->conn->lock));

    where = lo_tell(self->conn->pgconn, self->fd);

    pthread_mutex_unlock(&(self->conn->lock));
    Py_END_ALLOW_THREADS;

    if (where < 0)
        pq_raise(self->conn, NULL, NULL, NULL);
    return where;
}

/* lobject_export - export to a local file */

int
lobject_export(lobjectObject *self, char *filename)
{
    int res;

    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&(self->conn->lock));

    res = lo_export(self->conn->pgconn, self->oid, filename);

    pthread_mutex_unlock(&(self->conn->lock));
    Py_END_ALLOW_THREADS;

    if (res < 0)
        pq_raise(self->conn, NULL, NULL, NULL);
    return res;
}


#endif

