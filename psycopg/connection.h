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

#include <Python.h>
#include <libpq-fe.h>

#ifdef __cplusplus
extern "C" {
#endif

/* connection status */
#define CONN_STATUS_READY 1
#define CONN_STATUS_BEGIN 2
#define CONN_STATUS_SYNC  3
#define CONN_STATUS_ASYNC 4
    
extern PyTypeObject connectionType;

typedef struct {
    PyObject HEAD;

    pthread_mutex_t lock;   /* the global connection lock */

    char *dsn;              /* data source name */
    char *critical;         /* critical error on this connection */
    char *encoding;         /* current backend encoding */
    
    long int closed;          /* 2 means connection has been closed */
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

    /* errors (DBAPI-2.0 extension) */
    PyObject *exc_Error;
    PyObject *exc_Warning;
    PyObject *exc_InterfaceError;
    PyObject *exc_DatabaseError;
    PyObject *exc_InternalError;
    PyObject *exc_OperationalError;
    PyObject *exc_ProgrammingError;
    PyObject *exc_IntegrityError;
    PyObject *exc_DataError;
    PyObject *exc_NotSupportedError;
    
} connectionObject;
    
/* C-callable functions in connection_int.c and connection_ext.c */
extern int  conn_connect(connectionObject *self);
extern void conn_close(connectionObject *self);
extern int  conn_commit(connectionObject *self);
extern int  conn_rollback(connectionObject *self);
extern int  conn_switch_isolation_level(connectionObject *self, int level);
extern int  conn_set_client_encoding(connectionObject *self, char *enc); 

/* exception-raising macros */
#define EXC_IF_CONN_CLOSED(self) if ((self)->closed > 0) { \
    PyErr_SetString(InterfaceError, "connection already closed"); \
    return NULL; }
    
#ifdef __cplusplus
}
#endif

#endif /* !defined(PSYCOPG_CONNECTION_H) */
