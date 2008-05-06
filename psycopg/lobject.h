/* lobject.h - definition for the psycopg lobject type
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

#ifndef PSYCOPG_LOBJECT_H
#define PSYCOPG_LOBJECT_H 1

#include <Python.h>
#include <libpq-fe.h>
#include <libpq/libpq-fs.h>

#include "psycopg/config.h"
#include "psycopg/connection.h"

#ifdef __cplusplus
extern "C" {
#endif

extern HIDDEN PyTypeObject lobjectType;

typedef struct {
    PyObject HEAD;

    connectionObject *conn;  /* connection owning the lobject */
    long int mark;           /* copied from conn->mark */

    const char *smode;       /* string mode if lobject was opened */

    int fd;                  /* the file descriptor for file-like ops */
    Oid oid;                 /* the oid for this lobject */
} lobjectObject;

/* functions exported from lobject_int.c */

HIDDEN int lobject_open(lobjectObject *self, connectionObject *conn,
                        Oid oid, int mode, Oid new_oid,
                        const char *new_file);
HIDDEN int lobject_unlink(lobjectObject *self);
HIDDEN int lobject_export(lobjectObject *self, const char *filename);

HIDDEN Py_ssize_t lobject_read(lobjectObject *self, char *buf, size_t len);
HIDDEN Py_ssize_t lobject_write(lobjectObject *self, const char *buf,
                                size_t len);
HIDDEN int lobject_seek(lobjectObject *self, int pos, int whence);
HIDDEN int lobject_tell(lobjectObject *self);
HIDDEN int lobject_close(lobjectObject *self);

#define lobject_is_closed(self) \
    ((self)->fd < 0 || !(self)->conn || (self)->conn->closed)

/* exception-raising macros */

#define EXC_IF_LOBJ_CLOSED(self) \
  if (lobject_is_closed(self)) { \
    PyErr_SetString(InterfaceError, "lobject already closed");  \
    return NULL; }

#define EXC_IF_LOBJ_LEVEL0(self) \
if (self->conn->isolation_level == 0) {                             \
    psyco_set_error(ProgrammingError, (PyObject*)self,              \
        "can't use a lobject outside of transactions", NULL, NULL); \
    return NULL;                                                    \
}
#define EXC_IF_LOBJ_UNMARKED(self) \
if (self->conn->mark != self->mark) {                  \
    psyco_set_error(ProgrammingError, (PyObject*)self, \
        "lobject isn't valid anymore", NULL, NULL);    \
    return NULL;                                       \
}

#ifdef __cplusplus
}
#endif

#endif /* !defined(PSYCOPG_LOBJECT_H) */
