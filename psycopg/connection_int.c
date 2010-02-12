/* connection_int.c - code used by the connection object
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

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <string.h>

#define PSYCOPG_MODULE
#include "psycopg/config.h"
#include "psycopg/psycopg.h"
#include "psycopg/connection.h"
#include "psycopg/cursor.h"
#include "psycopg/pqpath.h"

/* conn_notice_callback - process notices */

static void
conn_notice_callback(void *args, const char *message)
{
    struct connectionObject_notice *notice;
    connectionObject *self = (connectionObject *)args;

    Dprintf("conn_notice_callback: %s", message);

    /* unfortunately the old protocol return COPY FROM errors only as notices,
       so we need to filter them looking for such errors (but we do it
       only if the protocol if <3, else we don't need that)
       
       NOTE: if we get here and the connection is unlocked then there is a
       problem but this should happen because the notice callback is only
       called from libpq and when we're inside libpq the connection is usually
       locked.
    */

    if (self->protocol < 3 && strncmp(message, "ERROR", 5) == 0)
        pq_set_critical(self, message);
    else {
        notice = (struct connectionObject_notice *)
                malloc(sizeof(struct connectionObject_notice));
        notice->message = strdup(message);
        notice->next = self->notice_pending;
        self->notice_pending = notice;
    }
}

void
conn_notice_process(connectionObject *self)
{
    struct connectionObject_notice *notice;
    PyObject *msg;

    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&self->lock);

    notice =  self->notice_pending;

    while (notice != NULL) {
        Py_BLOCK_THREADS;

        msg = PyString_FromString(notice->message);

        Dprintf("conn_notice_process: %s", notice->message);

        PyList_Append(self->notice_list, msg);
        Py_DECREF(msg);

        /* Remove the oldest item if the queue is getting too long. */
        if (PyList_GET_SIZE(self->notice_list) > CONN_NOTICES_LIMIT)
            PySequence_DelItem(self->notice_list, 0);

        Py_UNBLOCK_THREADS;

        notice = notice->next;
    }

    pthread_mutex_unlock(&self->lock);
    Py_END_ALLOW_THREADS;

    conn_notice_clean(self);
}

void
conn_notice_clean(connectionObject *self)
{
    struct connectionObject_notice *tmp, *notice;
    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&self->lock);

    notice = self->notice_pending;

    while (notice != NULL) {
        tmp = notice;
        notice = notice->next;
        free((void*)tmp->message);
        free(tmp);
    }
    
    self->notice_pending = NULL;
    
    pthread_mutex_unlock(&self->lock);
    Py_END_ALLOW_THREADS;    
}

/* conn_setup - setup and read basic information about the connection */

int
conn_setup(connectionObject *self, PGconn *pgconn)
{
    PGresult *pgres;
    const char *data, *tmp;
    const char *scs;    /* standard-conforming strings */
    size_t i;

    /* we need the initial date style to be ISO, for typecasters; if the user
       later change it, she must know what she's doing... */
    static const char datestyle[] = "SET DATESTYLE TO 'ISO'";
    static const char encoding[]  = "SHOW client_encoding";
    static const char isolevel[]  = "SHOW default_transaction_isolation";

    static const char lvl1a[] = "read uncommitted";
    static const char lvl1b[] = "read committed";
    static const char lvl2a[] = "repeatable read";
    static const char lvl2b[] = "serializable";

    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&self->lock);
    Py_BLOCK_THREADS;

    if (self->encoding) free(self->encoding);
    self->equote = 0;
    self->isolation_level = 0;

    /*
     * The presence of the 'standard_conforming_strings' parameter
     * means that the server _accepts_ the E'' quote.
     *
     * If the paramer is off, the PQescapeByteaConn returns
     * backslash escaped strings (e.g. '\001' -> "\\001"),
     * so the E'' quotes are required to avoid warnings
     * if 'escape_string_warning' is set.
     *
     * If the parameter is on, the PQescapeByteaConn returns
     * not escaped strings (e.g. '\001' -> "\001"), relying on the
     * fact that the '\' will pass untouched the string parser.
     * In this case the E'' quotes are NOT to be used.
     *
     * The PSYCOPG_OWN_QUOTING implementation always returns escaped strings.
     */
    scs = PQparameterStatus(pgconn, "standard_conforming_strings");
    Dprintf("conn_connect: server standard_conforming_strings parameter: %s",
        scs ? scs : "unavailable");

#ifndef PSYCOPG_OWN_QUOTING
    self->equote = (scs && (0 == strcmp("off", scs)));
#else
    self->equote = (scs != NULL);
#endif
    Dprintf("conn_connect: server requires E'' quotes: %s",
        self->equote ? "YES" : "NO");

    Py_UNBLOCK_THREADS;
    pgres = PQexec(pgconn, datestyle);
    Py_BLOCK_THREADS;

    if (pgres == NULL || PQresultStatus(pgres) != PGRES_COMMAND_OK ) {
        PyErr_SetString(OperationalError, "can't set datestyle to ISO");
        PQfinish(pgconn);
        IFCLEARPGRES(pgres);
        Py_UNBLOCK_THREADS;
        pthread_mutex_unlock(&self->lock);
        Py_BLOCK_THREADS;
        return -1;
    }
    CLEARPGRES(pgres);

    Py_UNBLOCK_THREADS;
    pgres = PQexec(pgconn, encoding);
    Py_BLOCK_THREADS;

    if (pgres == NULL || PQresultStatus(pgres) != PGRES_TUPLES_OK) {
        PyErr_SetString(OperationalError, "can't fetch client_encoding");
        PQfinish(pgconn);
        IFCLEARPGRES(pgres);
        Py_UNBLOCK_THREADS;
        pthread_mutex_unlock(&self->lock);
        Py_BLOCK_THREADS;
        return -1;
    }
    tmp = PQgetvalue(pgres, 0, 0);
    self->encoding = malloc(strlen(tmp)+1);
    if (self->encoding == NULL) {
        PyErr_NoMemory();
        PQfinish(pgconn);
        IFCLEARPGRES(pgres);
        Py_UNBLOCK_THREADS;
        pthread_mutex_unlock(&self->lock);
        Py_BLOCK_THREADS;
        return -1;
    }
    for (i=0 ; i < strlen(tmp) ; i++)
        self->encoding[i] = toupper(tmp[i]);
    self->encoding[i] = '\0';
    CLEARPGRES(pgres);

    Py_UNBLOCK_THREADS;
    pgres = PQexec(pgconn, isolevel);
    Py_BLOCK_THREADS;

    if (pgres == NULL || PQresultStatus(pgres) != PGRES_TUPLES_OK) {
        PyErr_SetString(OperationalError,
                         "can't fetch default_isolation_level");
        PQfinish(pgconn);
        IFCLEARPGRES(pgres);
        Py_UNBLOCK_THREADS;
        pthread_mutex_unlock(&self->lock);
        Py_BLOCK_THREADS;
        return -1;
    }
    data = PQgetvalue(pgres, 0, 0);
    if ((strncmp(lvl1a, data, strlen(lvl1a)) == 0)
        || (strncmp(lvl1b, data, strlen(lvl1b)) == 0))
        self->isolation_level = 1;
    else if ((strncmp(lvl2a, data, strlen(lvl2a)) == 0)
        || (strncmp(lvl2b, data, strlen(lvl2b)) == 0))
        self->isolation_level = 2;
    else
        self->isolation_level = 2;
    CLEARPGRES(pgres);

    Py_UNBLOCK_THREADS;
    pthread_mutex_unlock(&self->lock);
    Py_END_ALLOW_THREADS;

    return 0;
}

/* conn_connect - execute a connection to the database */

int
conn_connect(connectionObject *self)
{
    PGconn *pgconn;

    Py_BEGIN_ALLOW_THREADS;
    pgconn = PQconnectdb(self->dsn);
    Py_END_ALLOW_THREADS;

    Dprintf("conn_connect: new postgresql connection at %p", pgconn);

    if (pgconn == NULL)
    {
        Dprintf("conn_connect: PQconnectdb(%s) FAILED", self->dsn);
        PyErr_SetString(OperationalError, "PQconnectdb() failed");
        return -1;
    }
    else if (PQstatus(pgconn) == CONNECTION_BAD)
    {
        Dprintf("conn_connect: PQconnectdb(%s) returned BAD", self->dsn);
        PyErr_SetString(OperationalError, PQerrorMessage(pgconn));
        PQfinish(pgconn);
        return -1;
    }

    PQsetNoticeProcessor(pgconn, conn_notice_callback, (void*)self);

    if (conn_setup(self, pgconn) == -1)
        return -1;

    if (PQsetnonblocking(pgconn, 1) != 0) {
        Dprintf("conn_connect: PQsetnonblocking() FAILED");
        PyErr_SetString(OperationalError, "PQsetnonblocking() failed");
        PQfinish(pgconn);
        return -1;
    }

#ifdef HAVE_PQPROTOCOL3
    self->protocol = PQprotocolVersion(pgconn);
#else
    self->protocol = 2;
#endif
    Dprintf("conn_connect: using protocol %d", self->protocol);

    self->server_version = (int)PQserverVersion(pgconn);

    self->pgconn = pgconn;
    return 0;
}

/* conn_close - do anything needed to shut down the connection */

void
conn_close(connectionObject *self)
{
    /* sets this connection as closed even for other threads; also note that
       we need to check the value of pgconn, because we get called even when
       the connection fails! */
    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&self->lock);

    if (self->closed == 0)
        self->closed = 1;

    /* execute a forced rollback on the connection (but don't check the
       result, we're going to close the pq connection anyway */
    if (self->pgconn && self->closed == 1) {
        PGresult *pgres = NULL;
        char *error = NULL;

        if (pq_abort_locked(self, &pgres, &error) < 0) {
            IFCLEARPGRES(pgres);
            if (error)
                free (error);
        }
    }
    if (self->pgconn) {
        PQfinish(self->pgconn);
        Dprintf("conn_close: PQfinish called");
        self->pgconn = NULL;
   }
   
    pthread_mutex_unlock(&self->lock);
    Py_END_ALLOW_THREADS;
}

/* conn_commit - commit on a connection */

int
conn_commit(connectionObject *self)
{
    int res;

    res = pq_commit(self);
    return res;
}

/* conn_rollback - rollback a connection */

int
conn_rollback(connectionObject *self)
{
    int res;

    res = pq_abort(self);
    return res;
}

/* conn_switch_isolation_level - switch isolation level on the connection */

int
conn_switch_isolation_level(connectionObject *self, int level)
{
    PGresult *pgres = NULL;
    char *error = NULL;
    int res = 0;

    /* if the current isolation level is equal to the requested one don't switch */
    if (self->isolation_level == level) return 0;

    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&self->lock);

    /* if the current isolation level is > 0 we need to abort the current
       transaction before changing; that all folks! */
    if (self->isolation_level != level && self->isolation_level > 0) {
        res = pq_abort_locked(self, &pgres, &error);
    }
    self->isolation_level = level;

    Dprintf("conn_switch_isolation_level: switched to level %d", level);

    pthread_mutex_unlock(&self->lock);
    Py_END_ALLOW_THREADS;

    if (res < 0)
        pq_complete_error(self, &pgres, &error);

    return res;
}

/* conn_set_client_encoding - switch client encoding on connection */

int
conn_set_client_encoding(connectionObject *self, const char *enc)
{
    PGresult *pgres = NULL;
    char *error = NULL;
    char query[48];
    int res = 0;

    /* If the current encoding is equal to the requested one we don't
       issue any query to the backend */
    if (strcmp(self->encoding, enc) == 0) return 0;

    /* TODO: check for async query here and raise error if necessary */

    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&self->lock);

    /* set encoding, no encoding string is longer than 24 bytes */
    PyOS_snprintf(query, 47, "SET client_encoding = '%s'", enc);

    /* abort the current transaction, to set the encoding ouside of
       transactions */
    res = pq_abort_locked(self, &pgres, &error);

    if (res == 0) {
        res = pq_execute_command_locked(self, query, &pgres, &error);
        if (res == 0) {
            /* no error, we can proceeed and store the new encoding */
            if (self->encoding) free(self->encoding);
            self->encoding = strdup(enc);
            Dprintf("conn_set_client_encoding: set encoding to %s",
                    self->encoding);
        }
    }


    pthread_mutex_unlock(&self->lock);
    Py_END_ALLOW_THREADS;

    if (res < 0)
        pq_complete_error(self, &pgres, &error);

    return res;
}
