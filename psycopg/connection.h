/* connection.h - definition for the psycopg connection type
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

#ifndef PSYCOPG_CONNECTION_H
#define PSYCOPG_CONNECTION_H 1

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <libpq-fe.h>

#include "psycopg/config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* connection status */
#define CONN_STATUS_READY 1
#define CONN_STATUS_BEGIN 2
#define CONN_STATUS_SYNC  3
#define CONN_STATUS_ASYNC 4

/* Hard limit on the notices stored by the Python connection */
#define CONN_NOTICES_LIMIT 50

extern HIDDEN PyTypeObject connectionType;

typedef struct {
    PyObject_HEAD

    pthread_mutex_t lock;   /* the global connection lock */

    char *dsn;              /* data source name */
    char *critical;         /* critical error on this connection */
    char *encoding;         /* current backend encoding */

    long int closed;          /* 1 means connection has been closed;
                                 2 that something horrible happened */
    long int isolation_level; /* isolation level for this connection */
    long int mark;            /* number of commits/rollbacks done so far */
    int status;               /* status of the connection */
    int protocol;             /* protocol version */

    PGconn *pgconn;         /* the postgresql connection */

    PyObject *async_cursor;

    /* notice processing */
    PyObject *notice_list;
    PyObject *notice_filter;

    /* notifies */
    PyObject *notifies;

    /* per-connection typecasters */
    PyObject *string_types;   /* a set of typecasters for string types */
    PyObject *binary_types;   /* a set of typecasters for binary types */

    int equote;               /* use E''-style quotes for escaped strings */

} connectionObject;

/* C-callable functions in connection_int.c and connection_ext.c */
HIDDEN int  conn_connect(connectionObject *self);
HIDDEN void conn_close(connectionObject *self);
HIDDEN int  conn_commit(connectionObject *self);
HIDDEN int  conn_rollback(connectionObject *self);
HIDDEN int  conn_switch_isolation_level(connectionObject *self, int level);
HIDDEN int  conn_set_client_encoding(connectionObject *self, const char *enc);

/* exception-raising macros */
#define EXC_IF_CONN_CLOSED(self) if ((self)->closed > 0) { \
    PyErr_SetString(InterfaceError, "connection already closed"); \
    return NULL; }

#ifdef __cplusplus
}
#endif

#endif /* !defined(PSYCOPG_CONNECTION_H) */
