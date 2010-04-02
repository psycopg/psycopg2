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


/* conn_notifies_process - make received notification available
 *
 * The function should be called with the connection lock and holding the GIL.
 */

void
conn_notifies_process(connectionObject *self)
{
    PGnotify *pgn;

    while ((pgn = PQnotifies(self->pgconn)) != NULL) {
        PyObject *notify;

        Dprintf("conn_notifies_process: got NOTIFY from pid %d, msg = %s",
                (int) pgn->be_pid, pgn->relname);

        notify = PyTuple_New(2);
        PyTuple_SET_ITEM(notify, 0, PyInt_FromLong((long)pgn->be_pid));
        PyTuple_SET_ITEM(notify, 1, PyString_FromString(pgn->relname));
        PyList_Append(self->notifies, notify);
        Py_DECREF(notify);
        PQfreemem(pgn);
    }
}


/*
 * the conn_get_* family of functions makes it easier to obtain the connection
 * parameters from query results or by interrogating the connection itself
*/

int
conn_get_standard_conforming_strings(PGconn *pgconn)
{
    int equote;
    const char *scs;
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
    equote = (scs && (0 == strcmp("off", scs)));
#else
    equote = (scs != NULL);
#endif
    Dprintf("conn_connect: server requires E'' quotes: %s",
            equote ? "YES" : "NO");

    return equote;
}

char *
conn_get_encoding(PGresult *pgres)
{
    char *tmp, *encoding;
    size_t i;

    tmp = PQgetvalue(pgres, 0, 0);
    encoding = malloc(strlen(tmp)+1);
    if (encoding == NULL) {
        PyErr_NoMemory();
        IFCLEARPGRES(pgres);
        return NULL;
    }
    for (i=0 ; i < strlen(tmp) ; i++)
        encoding[i] = toupper(tmp[i]);
    encoding[i] = '\0';
    CLEARPGRES(pgres);

    return encoding;
}

int
conn_get_isolation_level(PGresult *pgres)
{
    static const char lvl1a[] = "read uncommitted";
    static const char lvl1b[] = "read committed";
    char *isolation_level = PQgetvalue(pgres, 0, 0);

    CLEARPGRES(pgres);

    if ((strncmp(lvl1a, isolation_level, strlen(isolation_level)) == 0)
        || (strncmp(lvl1b, isolation_level, strlen(isolation_level)) == 0))
        return 1;
    else /* if it's not one of the lower ones, it's SERIALIZABLE */
        return 2;
}

int
conn_get_protocol_version(PGconn *pgconn)
{
#ifdef HAVE_PQPROTOCOL3
        return PQprotocolVersion(pgconn);
#else
        return 2;
#endif
}

/* conn_setup - setup and read basic information about the connection */

int
conn_setup(connectionObject *self, PGconn *pgconn)
{
    PGresult *pgres;

    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&self->lock);
    Py_BLOCK_THREADS;

    if (self->encoding) free(self->encoding);
    self->equote = 0;
    self->isolation_level = 0;

    self->equote = conn_get_standard_conforming_strings(pgconn);

    Py_UNBLOCK_THREADS;
    pgres = PQexec(pgconn, psyco_datestyle);
    Py_BLOCK_THREADS;

    if (pgres == NULL || PQresultStatus(pgres) != PGRES_COMMAND_OK ) {
        PyErr_SetString(OperationalError, "can't set datestyle to ISO");
        IFCLEARPGRES(pgres);
        Py_UNBLOCK_THREADS;
        pthread_mutex_unlock(&self->lock);
        Py_BLOCK_THREADS;
        return -1;
    }
    CLEARPGRES(pgres);

    Py_UNBLOCK_THREADS;
    pgres = PQexec(pgconn, psyco_client_encoding);
    Py_BLOCK_THREADS;

    if (pgres == NULL || PQresultStatus(pgres) != PGRES_TUPLES_OK) {
        PyErr_SetString(OperationalError, "can't fetch client_encoding");
        IFCLEARPGRES(pgres);
        Py_UNBLOCK_THREADS;
        pthread_mutex_unlock(&self->lock);
        Py_BLOCK_THREADS;
        return -1;
    }

    /* conn_get_encoding returns a malloc'd string */
    self->encoding = conn_get_encoding(pgres);
    if (self->encoding == NULL) {
        Py_UNBLOCK_THREADS;
        pthread_mutex_unlock(&self->lock);
        Py_BLOCK_THREADS;
        return -1;
    }

    Py_UNBLOCK_THREADS;
    pgres = PQexec(pgconn, psyco_transaction_isolation);
    Py_BLOCK_THREADS;

    if (pgres == NULL || PQresultStatus(pgres) != PGRES_TUPLES_OK) {
        PyErr_SetString(OperationalError,
                         "can't fetch default_isolation_level");
        IFCLEARPGRES(pgres);
        Py_UNBLOCK_THREADS;
        pthread_mutex_unlock(&self->lock);
        Py_BLOCK_THREADS;
        return -1;
    }
    self->isolation_level = conn_get_isolation_level(pgres);

    Py_UNBLOCK_THREADS;
    pthread_mutex_unlock(&self->lock);
    Py_END_ALLOW_THREADS;

    return 0;
}

/* conn_connect - execute a connection to the database */

int
conn_sync_connect(connectionObject *self)
{
    PGconn *pgconn;

    Py_BEGIN_ALLOW_THREADS;
    self->pgconn = pgconn = PQconnectdb(self->dsn);
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
        return -1;
    }

    PQsetNoticeProcessor(pgconn, conn_notice_callback, (void*)self);

    if (conn_setup(self, pgconn) == -1)
        return -1;

    if (pq_set_non_blocking(self, 1, 1) != 0) {
        return -1;
    }

    self->protocol = conn_get_protocol_version(pgconn);
    Dprintf("conn_connect: using protocol %d", self->protocol);

    self->server_version = (int)PQserverVersion(pgconn);

    return 0;
}

static int
conn_async_connect(connectionObject *self)
{
    PGconn *pgconn;

    self->pgconn = pgconn = PQconnectStart(self->dsn);

    Dprintf("conn_connect: new postgresql connection at %p", pgconn);

    if (pgconn == NULL)
    {
        Dprintf("conn_connect: PQconnectStart(%s) FAILED", self->dsn);
        PyErr_SetString(OperationalError, "PQconnectStart() failed");
        return -1;
    }
    else if (PQstatus(pgconn) == CONNECTION_BAD)
    {
        Dprintf("conn_connect: PQconnectdb(%s) returned BAD", self->dsn);
        PyErr_SetString(OperationalError, PQerrorMessage(pgconn));
        return -1;
    }

    PQsetNoticeProcessor(pgconn, conn_notice_callback, (void*)self);

    self->status = CONN_STATUS_SETUP;

    return 0;
}

int
conn_connect(connectionObject *self, long int async)
{
   if (async == 1) {
      Dprintf("con_connect: connecting in ASYNC mode");
      return conn_async_connect(self);
    }
    else {
      Dprintf("con_connect: connecting in SYNC mode");
      return conn_sync_connect(self);
    }
}

/* conn_poll_send - handle connection polling when flushing output */

PyObject *
conn_poll_send(connectionObject *self)
{
    const char *query = NULL;
    int next_status;
    int ret;

    Dprintf("conn_poll_send: status %d", self->status);

    switch (self->status) {
    case CONN_STATUS_SEND_DATESTYLE:
        /* set the datestyle */
        query = psyco_datestyle;
        next_status = CONN_STATUS_SENT_DATESTYLE;
        break;
    case CONN_STATUS_SEND_CLIENT_ENCODING:
        /* get the client_encoding */
        query = psyco_client_encoding;
        next_status = CONN_STATUS_SENT_CLIENT_ENCODING;
        break;
    case CONN_STATUS_SENT_DATESTYLE:
    case CONN_STATUS_SENT_CLIENT_ENCODING:
        /* the query has only been partially sent */
        query = NULL;
        next_status = self->status;
        break;
    default:
        /* unexpected state, error out */
        PyErr_Format(OperationalError,
                     "unexpected state: %d", self->status);
        return NULL;
    }

    Dprintf("conn_poll_send: sending query %-.200s", query);

    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&(self->lock));

    if (query != NULL) {
        if (PQsendQuery(self->pgconn, query) != 1) {
            pthread_mutex_unlock(&(self->lock));
            Py_BLOCK_THREADS;
            PyErr_SetString(OperationalError,
                            PQerrorMessage(self->pgconn));
            return NULL;
        }
    }

    if (PQflush(self->pgconn) == 0) {
        /* the query got fully sent to the server */
        Dprintf("conn_poll_send: query got flushed immediately");
        /* the return value will be POLL_READ */
        ret = PSYCO_POLL_READ;

        /* advance the next status, since we skip over the "waiting for the
           query to be sent" status */
        switch (next_status) {
        case CONN_STATUS_SENT_DATESTYLE:
            next_status = CONN_STATUS_GET_DATESTYLE;
            break;
        case CONN_STATUS_SENT_CLIENT_ENCODING:
            next_status = CONN_STATUS_GET_CLIENT_ENCODING;
            break;
        }
    }
    else {
        /* query did not get sent completely, tell the client to wait for the
           socket to become writable */
        ret = PSYCO_POLL_WRITE;
    }

    self->status = next_status;
    Dprintf("conn_poll_send: next status is %d, returning %d",
            self->status, ret);

    pthread_mutex_unlock(&(self->lock));
    Py_END_ALLOW_THREADS;

    return PyInt_FromLong(ret);
}

/* curs_poll_fetch - handle connection polling when reading result */

PyObject *
conn_poll_fetch(connectionObject *self)
{
    PGresult *pgres;
    int is_busy;
    int next_status;
    int ret;

    Dprintf("conn_poll_fetch: status %d", self->status);

    /* consume the input */
    is_busy = pq_is_busy(self);
    if (is_busy == -1) {
        /* there was an error, raise the exception */
        return NULL;
    }
    else if (is_busy == 1) {
        /* the connection is busy, tell the user to wait more */
        Dprintf("conn_poll_fetch: connection busy, returning %d",
                PSYCO_POLL_READ);
        return PyInt_FromLong(PSYCO_POLL_READ);
    }

    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&(self->lock));

    /* connection no longer busy, process input */
    pgres = PQgetResult(self->pgconn);

    /* do the rest while holding the GIL, we won't be calling into any
       blocking API */
    pthread_mutex_unlock(&(self->lock));
    Py_END_ALLOW_THREADS;

    Dprintf("conn_poll_fetch: got result %p", pgres);

    /* we expect COMMAND_OK (from SET) or TUPLES_OK (from SHOW) */
    if (pgres == NULL || (PQresultStatus(pgres) != PGRES_COMMAND_OK &&
                          PQresultStatus(pgres) != PGRES_TUPLES_OK)) {
        PyErr_SetString(OperationalError, "can't issue "
                        "initial connection queries");
        IFCLEARPGRES(pgres);
        return NULL;
    }

    if (self->status == CONN_STATUS_GET_DATESTYLE) {
        /* got the result from SET DATESTYLE*/
        Dprintf("conn_poll_fetch: datestyle set");
        next_status = CONN_STATUS_SEND_CLIENT_ENCODING;
    }
    else if (self->status == CONN_STATUS_GET_CLIENT_ENCODING) {
        /* got the client_encoding */
        self->encoding = conn_get_encoding(pgres);
        if (self->encoding == NULL) {
            return NULL;
        }
        Dprintf("conn_poll_fetch: got client_encoding %s", self->encoding);

        /* since this is the last step, set the other instance variables now */
        self->equote = conn_get_standard_conforming_strings(self->pgconn);
        self->protocol = conn_get_protocol_version(self->pgconn);
        self->server_version = (int) PQserverVersion(self->pgconn);
        /*
         * asynchronous connections always use isolation level 0, the user is
         * expected to manage the transactions himself, by sending
         * (asynchronously) BEGIN and COMMIT statements.
         */
        self->isolation_level = 0;

        Py_BEGIN_ALLOW_THREADS;
        pthread_mutex_lock(&(self->lock));

        /* set the connection to nonblocking */
        if (PQsetnonblocking(self->pgconn, 1) != 0) {
            Dprintf("conn_async_connect: PQsetnonblocking() FAILED");
            Py_BLOCK_THREADS;
            PyErr_SetString(OperationalError, "PQsetnonblocking() failed");
            return NULL;
        }

        pthread_mutex_unlock(&(self->lock));
        Py_END_ALLOW_THREADS;

        /* next status is going to READY */
        next_status = CONN_STATUS_READY;
    }
    else {
        /* unexpected state, error out */
        PyErr_Format(OperationalError,
                     "unexpected state: %d", self->status);
        return NULL;
    }

    /* clear any leftover result, there should be none, but the protocol
       requires calling PQgetResult until you get a NULL */
    pq_clear_async(self);

    self->status = next_status;

    /* if the curent status is READY it means we got the result of the
       last initialization query, so we return POLL_OK, otherwise we need to
       send another query, so return POLL_WRITE */
    ret = self->status == CONN_STATUS_READY ? PSYCO_POLL_OK : PSYCO_POLL_WRITE;
    Dprintf("conn_poll_fetch: next status is %d, returning %d",
            self->status, ret);
    return PyInt_FromLong(ret);
}

/* conn_poll_ready - handle connection polling when it is already open */

PyObject *
conn_poll_ready(connectionObject *self)
{
    int is_busy;

    /* if there is an asynchronous query underway, poll it */
    if (self->async_cursor != NULL) {
        if (self->async_status == ASYNC_WRITE) {
            return curs_poll_send((cursorObject *) self->async_cursor);
        }
        else {
            /* this gets called both for ASYNC_READ and ASYNC_DONE, because
               even if the async query is complete, we still might want to
               check for NOTIFYs */
            return curs_poll_fetch((cursorObject *) self->async_cursor);
        }
    }

    /* otherwise just check for NOTIFYs */
    is_busy = pq_is_busy(self);
    if (is_busy == -1) {
        /* there was an error, raise the exception */
        return NULL;
    }
    else if (is_busy == 1) {
        /* the connection is busy, tell the user to wait more */
        Dprintf("conn_poll_ready: returning %d", PSYCO_POLL_READ);
        return PyInt_FromLong(PSYCO_POLL_READ);
    }
    else {
        /* connection is idle */
        Dprintf("conn_poll_ready: returning %d", PSYCO_POLL_OK);
        return PyInt_FromLong(PSYCO_POLL_OK);
    }
}

/* conn_poll_green - poll a *sync* connection with external wait */

PyObject *
conn_poll_green(connectionObject *self)
{
    int res = PSYCO_POLL_ERROR;

    switch (self->status) {
    case CONN_STATUS_SETUP:
        Dprintf("conn_poll: status = CONN_STATUS_SETUP");
        self->status = CONN_STATUS_ASYNC;
        res =  PSYCO_POLL_WRITE;
        break;

    case CONN_STATUS_ASYNC:
        Dprintf("conn_poll: status = CONN_STATUS_ASYNC");
        switch (PQconnectPoll(self->pgconn)) {
        case PGRES_POLLING_OK:
            res = PSYCO_POLL_OK;
            break;
        case PGRES_POLLING_READING:
            res = PSYCO_POLL_READ;
            break;
        case PGRES_POLLING_WRITING:
            res = PSYCO_POLL_WRITE;
            break;
        case PGRES_POLLING_FAILED:
        case PGRES_POLLING_ACTIVE:
            res = PSYCO_POLL_ERROR;
            break;
        }
        break;

    default:
        Dprintf("conn_poll: in unexpected state");
        res = PSYCO_POLL_ERROR;
    }

    return PyInt_FromLong(res);
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
