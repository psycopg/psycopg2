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

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <string.h>

#define PSYCOPG_MODULE
#include "psycopg/config.h"
#include "psycopg/psycopg.h"
#include "psycopg/connection.h"
#include "psycopg/lobject.h"
#include "psycopg/pqpath.h"

#ifdef PSYCOPG_EXTENSIONS

static void
collect_error(connectionObject *conn, char **error)
{
    const char *msg = PQerrorMessage(conn->pgconn);

    if (msg)
        *error = strdup(msg);
}

/* lobject_open - create a new/open an existing lo */

int
lobject_open(lobjectObject *self, connectionObject *conn,
              Oid oid, int mode, Oid new_oid, const char *new_file)
{
    int retvalue = -1;
    PGresult *pgres = NULL;
    char *error = NULL;

    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&(self->conn->lock));

    retvalue = pq_begin_locked(self->conn, &pgres, &error);
    if (retvalue < 0)
        goto end;

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

        if (self->oid == InvalidOid) {
            collect_error(self->conn, &error);
            retvalue = -1;
            goto end;
        }

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

        if (self->fd == -1) {
            collect_error(self->conn, &error);
            retvalue = -1;
            goto end;
        }
    }
    /* set the mode for future reference */
    switch (mode) {
    case -1:
        self->smode = "n"; break;
    case INV_READ:
        self->smode = "r"; break;
    case INV_WRITE:
        self->smode = "w"; break;
    case INV_READ+INV_WRITE:
        self->smode = "rw"; break;
    }
    retvalue = 0;

 end:
    pthread_mutex_unlock(&(self->conn->lock));
    Py_END_ALLOW_THREADS;

    if (retvalue < 0)
        pq_complete_error(self->conn, &pgres, &error);
    return retvalue;
}

/* lobject_close - close an existing lo */

static int
lobject_close_locked(lobjectObject *self, char **error)
{
    int retvalue;

    if (self->conn->isolation_level == 0 ||
        self->conn->mark != self->mark ||
        self->fd == -1)
        return 0;

    retvalue = lo_close(self->conn->pgconn, self->fd);
    self->fd = -1;
    if (retvalue < 0)
        collect_error(self->conn, error);

    return retvalue;
}

int
lobject_close(lobjectObject *self)
{
    PGresult *pgres = NULL;
    char *error = NULL;
    int retvalue;

    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&(self->conn->lock));

    retvalue = lobject_close_locked(self, &error);

    pthread_mutex_unlock(&(self->conn->lock));
    Py_END_ALLOW_THREADS;

    if (retvalue < 0)
        pq_complete_error(self->conn, &pgres, &error);
    return retvalue;
}

/* lobject_unlink - remove an lo from database */

int
lobject_unlink(lobjectObject *self)
{
    PGresult *pgres = NULL;
    char *error = NULL;
    int retvalue = -1;

    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&(self->conn->lock));

    retvalue = pq_begin_locked(self->conn, &pgres, &error);
    if (retvalue < 0)
        goto end;

    /* first we make sure the lobject is closed and then we unlink */
    retvalue = lobject_close_locked(self, &error);
    if (retvalue < 0)
        goto end;

    retvalue = lo_unlink(self->conn->pgconn, self->oid);
    if (retvalue < 0)
        collect_error(self->conn, &error);

 end:
    pthread_mutex_unlock(&(self->conn->lock));
    Py_END_ALLOW_THREADS;

    if (retvalue < 0)
        pq_complete_error(self->conn, &pgres, &error);
    return retvalue;
}

/* lobject_write - write bytes to a lo */

Py_ssize_t
lobject_write(lobjectObject *self, const char *buf, size_t len)
{
    Py_ssize_t written;
    PGresult *pgres = NULL;
    char *error = NULL;

    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&(self->conn->lock));

    written = lo_write(self->conn->pgconn, self->fd, buf, len);
    if (written < 0)
        collect_error(self->conn, &error);

    pthread_mutex_unlock(&(self->conn->lock));
    Py_END_ALLOW_THREADS;

    if (written < 0)
        pq_complete_error(self->conn, &pgres, &error);
    return written;
}

/* lobject_read - read bytes from a lo */

Py_ssize_t
lobject_read(lobjectObject *self, char *buf, size_t len)
{
    Py_ssize_t n_read;
    PGresult *pgres = NULL;
    char *error = NULL;

    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&(self->conn->lock));

    n_read = lo_read(self->conn->pgconn, self->fd, buf, len);
    if (n_read < 0)
        collect_error(self->conn, &error);

    pthread_mutex_unlock(&(self->conn->lock));
    Py_END_ALLOW_THREADS;

    if (n_read < 0)
        pq_complete_error(self->conn, &pgres, &error);
    return n_read;
}

/* lobject_seek - move the current position in the lo */

int
lobject_seek(lobjectObject *self, int pos, int whence)
{
    PGresult *pgres = NULL;
    char *error = NULL;
    int where;

    Dprintf("lobject_seek: fd = %d, pos = %d, whence = %d",
            self->fd, pos, whence);

    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&(self->conn->lock));

    where = lo_lseek(self->conn->pgconn, self->fd, pos, whence);
    Dprintf("lobject_seek: where = %d", where);
    if (where < 0)
        collect_error(self->conn, &error);

    pthread_mutex_unlock(&(self->conn->lock));
    Py_END_ALLOW_THREADS;

    if (where < 0)
        pq_complete_error(self->conn, &pgres, &error);
    return where;
}

/* lobject_tell - tell the current position in the lo */

int
lobject_tell(lobjectObject *self)
{
    PGresult *pgres = NULL;
    char *error = NULL;
    int where;

    Dprintf("lobject_tell: fd = %d", self->fd);

    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&(self->conn->lock));

    where = lo_tell(self->conn->pgconn, self->fd);
    Dprintf("lobject_tell: where = %d", where);
    if (where < 0)
        collect_error(self->conn, &error);

    pthread_mutex_unlock(&(self->conn->lock));
    Py_END_ALLOW_THREADS;

    if (where < 0)
        pq_complete_error(self->conn, &pgres, &error);
    return where;
}

/* lobject_export - export to a local file */

int
lobject_export(lobjectObject *self, const char *filename)
{
    PGresult *pgres = NULL;
    char *error = NULL;
    int retvalue;

    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&(self->conn->lock));

    retvalue = pq_begin_locked(self->conn, &pgres, &error);
    if (retvalue < 0)
        goto end;

    retvalue = lo_export(self->conn->pgconn, self->oid, filename);
    if (retvalue < 0)
        collect_error(self->conn, &error);

 end:
    pthread_mutex_unlock(&(self->conn->lock));
    Py_END_ALLOW_THREADS;

    if (retvalue < 0)
        pq_complete_error(self->conn, &pgres, &error);
    return retvalue;
}


#endif

