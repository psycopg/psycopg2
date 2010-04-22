/* connection.h - definition for the psycopg connection type
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
#define CONN_STATUS_SETUP 0
#define CONN_STATUS_READY 1
#define CONN_STATUS_BEGIN 2
/* async connection building statuses */
#define CONN_STATUS_CONNECTING            20
#define CONN_STATUS_DATESTYLE             21
#define CONN_STATUS_CLIENT_ENCODING       22

/* async query execution status */
#define ASYNC_DONE  0
#define ASYNC_READ  1
#define ASYNC_WRITE 2

/* polling result */
#define PSYCO_POLL_OK    0
#define PSYCO_POLL_READ  1
#define PSYCO_POLL_WRITE 2
#define PSYCO_POLL_ERROR 3

/* Hard limit on the notices stored by the Python connection */
#define CONN_NOTICES_LIMIT 50

/* we need the initial date style to be ISO, for typecasters; if the user
   later change it, she must know what she's doing... these are the queries we
   need to issue */
#define psyco_datestyle "SET DATESTYLE TO 'ISO'"
#define psyco_client_encoding  "SHOW client_encoding"
#define psyco_transaction_isolation "SHOW default_transaction_isolation"

extern HIDDEN PyTypeObject connectionType;

struct connectionObject_notice {
    struct connectionObject_notice *next;
    const char *message;
};

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

    long int async;           /* 1 means the connection is async */
    int protocol;             /* protocol version */
    int server_version;       /* server version */

    PGconn *pgconn;           /* the postgresql connection */

    PyObject *async_cursor;   /* a cursor executing an asynchronous query */
    int async_status;         /* asynchronous execution status */

    /* notice processing */
    PyObject *notice_list;
    PyObject *notice_filter;
    struct connectionObject_notice *notice_pending;

    /* notifies */
    PyObject *notifies;

    /* per-connection typecasters */
    PyObject *string_types;   /* a set of typecasters for string types */
    PyObject *binary_types;   /* a set of typecasters for binary types */

    int equote;               /* use E''-style quotes for escaped strings */
} connectionObject;

/* C-callable functions in connection_int.c and connection_ext.c */
HIDDEN int  conn_get_standard_conforming_strings(PGconn *pgconn);
HIDDEN char *conn_get_encoding(PGresult *pgres);
HIDDEN int  conn_get_isolation_level(PGresult *pgres);
HIDDEN int  conn_get_protocol_version(PGconn *pgconn);
HIDDEN void conn_notice_process(connectionObject *self);
HIDDEN void conn_notice_clean(connectionObject *self);
HIDDEN void conn_notifies_process(connectionObject *self);
HIDDEN int  conn_setup(connectionObject *self, PGconn *pgconn);
HIDDEN int  conn_connect(connectionObject *self, long int async);
HIDDEN void conn_close(connectionObject *self);
HIDDEN int  conn_commit(connectionObject *self);
HIDDEN int  conn_rollback(connectionObject *self);
HIDDEN int  conn_switch_isolation_level(connectionObject *self, int level);
HIDDEN int  conn_set_client_encoding(connectionObject *self, const char *enc);
HIDDEN int  conn_poll(connectionObject *self);

/* exception-raising macros */
#define EXC_IF_CONN_CLOSED(self) if ((self)->closed > 0) { \
    PyErr_SetString(InterfaceError, "connection already closed"); \
    return NULL; }

#define EXC_IF_CONN_ASYNC(self, cmd) if ((self)->async == 1) { \
    PyErr_SetString(ProgrammingError, #cmd " cannot be used "  \
    "in asynchronous mode");                                   \
    return NULL; }

#ifdef __cplusplus
}
#endif

#endif /* !defined(PSYCOPG_CONNECTION_H) */
