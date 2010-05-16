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
#include "psycopg/green.h"

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
    int ret;

#ifdef HAVE_PQPROTOCOL3
    ret = PQprotocolVersion(pgconn);
#else
    ret = 2;
#endif

    Dprintf("conn_connect: using protocol %d", ret);
    return ret;
}

int
conn_get_server_version(PGconn *pgconn)
{
    return (int)PQserverVersion(pgconn);
}


/* conn_setup - setup and read basic information about the connection */

int
conn_setup(connectionObject *self, PGconn *pgconn)
{
    PGresult *pgres;
    int green;

    self->equote = conn_get_standard_conforming_strings(pgconn);
    self->server_version = conn_get_server_version(pgconn);
    self->protocol = conn_get_protocol_version(self->pgconn);

    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&self->lock);
    Py_BLOCK_THREADS;

    green = psyco_green();

    if (green && (pq_set_non_blocking(self, 1, 1) != 0)) {
        return -1;
    }

    if (!green) {
        Py_UNBLOCK_THREADS;
        pgres = PQexec(pgconn, psyco_datestyle);
        Py_BLOCK_THREADS;
    } else {
        pgres = psyco_exec_green(self, psyco_datestyle);
    }

    if (pgres == NULL || PQresultStatus(pgres) != PGRES_COMMAND_OK ) {
        PyErr_SetString(OperationalError, "can't set datestyle to ISO");
        IFCLEARPGRES(pgres);
        Py_UNBLOCK_THREADS;
        pthread_mutex_unlock(&self->lock);
        Py_BLOCK_THREADS;
        return -1;
    }
    CLEARPGRES(pgres);

    if (!green) {
        Py_UNBLOCK_THREADS;
        pgres = PQexec(pgconn, psyco_client_encoding);
        Py_BLOCK_THREADS;
    } else {
        pgres = psyco_exec_green(self, psyco_client_encoding);
    }

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
    CLEARPGRES(pgres);

    if (!green) {
        Py_UNBLOCK_THREADS;
        pgres = PQexec(pgconn, psyco_transaction_isolation);
        Py_BLOCK_THREADS;
    } else {
        pgres = psyco_exec_green(self, psyco_transaction_isolation);
    }

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

static int
_conn_sync_connect(connectionObject *self)
{
    PGconn *pgconn;
    int green;

    /* store this value to prevent inconsistencies due to a change
     * in the middle of the function. */
    green = psyco_green();
    if (!green) {
        Py_BEGIN_ALLOW_THREADS;
        self->pgconn = pgconn = PQconnectdb(self->dsn);
        Py_END_ALLOW_THREADS;
        Dprintf("conn_connect: new postgresql connection at %p", pgconn);
    }
    else {
        Py_BEGIN_ALLOW_THREADS;
        self->pgconn = pgconn = PQconnectStart(self->dsn);
        Py_END_ALLOW_THREADS;
        Dprintf("conn_connect: new green postgresql connection at %p", pgconn);
    }

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

    /* if the connection is green, wait to finish connection */
    if (green) {
        if (0 != psyco_wait(self)) {
            return -1;
        }
    }

    /* From here the connection is considered ready: with the new status,
     * poll() will use PQisBusy instead of PQconnectPoll.
     */
    self->status = CONN_STATUS_READY;

    if (conn_setup(self, self->pgconn) == -1) {
        return -1;
    }

    return 0;
}

static int
_conn_async_connect(connectionObject *self)
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

    /* The connection will be completed banging on poll():
     * First with _conn_poll_connecting() that will finish connection,
     * then with _conn_poll_setup_async() that will do the same job
     * of setup_async(). */

    return 0;
}

int
conn_connect(connectionObject *self, long int async)
{
   if (async == 1) {
      Dprintf("con_connect: connecting in ASYNC mode");
      return _conn_async_connect(self);
    }
    else {
      Dprintf("con_connect: connecting in SYNC mode");
      return _conn_sync_connect(self);
    }
}


/* poll during a connection attempt until the connection has established. */

static int
_conn_poll_connecting(connectionObject *self)
{
    int res = PSYCO_POLL_ERROR;

    Dprintf("conn_poll: poll connecting");
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
        PyErr_SetString(OperationalError, "asynchronous connection failed");
        res = PSYCO_POLL_ERROR;
        break;
    }

    return res;
}


/* Advance to the next state after an attempt of flushing output */

static int
_conn_poll_advance_write(connectionObject *self, int flush)
{
    int res;

    Dprintf("conn_poll: poll writing");
    switch (flush) {
    case  0:  /* success */
        /* we've finished pushing the query to the server. Let's start
          reading the results. */
        Dprintf("conn_poll: async_status -> ASYNC_READ");
        self->async_status = ASYNC_READ;
        res = PSYCO_POLL_READ;
        break;
    case  1:  /* would block */
        res = PSYCO_POLL_WRITE;
        break;
    case -1:  /* error */
        PyErr_SetString(OperationalError, PQerrorMessage(self->pgconn));
        res = PSYCO_POLL_ERROR;
        break;
    default:
        Dprintf("conn_poll: unexpected result from flush: %d", flush);
        res = PSYCO_POLL_ERROR;
        break;
    }
    return res;
}

/* Advance to the next state after a call to a pq_is_busy* function */
static int
_conn_poll_advance_read(connectionObject *self, int busy)
{
    int res;

    Dprintf("conn_poll: poll reading");
    switch (busy) {
    case 0: /* result is ready */
        res = PSYCO_POLL_OK;
        Dprintf("conn_poll: async_status -> ASYNC_DONE");
        self->async_status = ASYNC_DONE;
        break;
    case 1: /* result not ready: fd would block */
        res = PSYCO_POLL_READ;
        break;
    case -1: /* ouch, error */
        res = PSYCO_POLL_ERROR;
        break;
    default:
        Dprintf("conn_poll: unexpected result from pq_is_busy: %d", busy);
        res = PSYCO_POLL_ERROR;
        break;
    }
    return res;
}

/* Poll the connection for the send query/retrieve result phase

  Advance the async_status (usually going WRITE -> READ -> DONE) but don't
  mess with the connection status. */

static int
_conn_poll_query(connectionObject *self)
{
    int res = PSYCO_POLL_ERROR;

    switch (self->async_status) {
    case ASYNC_WRITE:
        Dprintf("conn_poll: async_status = ASYNC_WRITE");
        res = _conn_poll_advance_write(self, PQflush(self->pgconn));
        break;

    case ASYNC_READ:
        Dprintf("conn_poll: async_status = ASYNC_READ");
        if (self->async) {
            res = _conn_poll_advance_read(self, pq_is_busy(self));
        }
        else {
            /* we are a green connection being polled as result of a query.
              this means that our caller has the lock and we are being called
              from the callback. If we tried to acquire the lock now it would
              be a deadlock. */
            res = _conn_poll_advance_read(self, pq_is_busy_locked(self));
        }
        break;

    case ASYNC_DONE:
        Dprintf("conn_poll: async_status = ASYNC_DONE");
        /* We haven't asked anything: just check for notifications. */
        res = _conn_poll_advance_read(self, pq_is_busy(self));
        break;

    default:
        Dprintf("conn_poll: in unexpected async status: %d",
                self->async_status);
        res = PSYCO_POLL_ERROR;
        break;
    }

    return res;
}

/* Advance to the next state during an async connection setup
 *
 * If the connection is green, this is performed by the regular
 * sync code so the queries are sent by conn_setup() while in
 * CONN_STATUS_READY state.
 */
static int
_conn_poll_setup_async(connectionObject *self)
{
    int res = PSYCO_POLL_ERROR;
    PGresult *pgres;

    switch (self->status) {
    case CONN_STATUS_CONNECTING:
        /* Set the connection to nonblocking now. */
        if (pq_set_non_blocking(self, 1, 1) != 0) {
            break;
        }

        self->equote = conn_get_standard_conforming_strings(self->pgconn);
        self->protocol = conn_get_protocol_version(self->pgconn);
        self->server_version = conn_get_server_version(self->pgconn);

        /* asynchronous connections always use isolation level 0, the user is
         * expected to manage the transactions himself, by sending
         * (asynchronously) BEGIN and COMMIT statements.
         */
        self->isolation_level = 0;

        Dprintf("conn_poll: status -> CONN_STATUS_DATESTYLE");
        self->status = CONN_STATUS_DATESTYLE;
        if (0 == pq_send_query(self, psyco_datestyle)) {
            PyErr_SetString(OperationalError, PQerrorMessage(self->pgconn));
            break;
        }
        Dprintf("conn_poll: async_status -> ASYNC_WRITE");
        self->async_status = ASYNC_WRITE;
        res = PSYCO_POLL_WRITE;
        break;

    case CONN_STATUS_DATESTYLE:
        res = _conn_poll_query(self);
        if (res == PSYCO_POLL_OK) {
            res = PSYCO_POLL_ERROR;
            pgres = pq_get_last_result(self);
            if (pgres == NULL || PQresultStatus(pgres) != PGRES_COMMAND_OK ) {
                PyErr_SetString(OperationalError, "can't set datestyle to ISO");
                break;
            }
            CLEARPGRES(pgres);

            Dprintf("conn_poll: status -> CONN_STATUS_CLIENT_ENCODING");
            self->status = CONN_STATUS_CLIENT_ENCODING;
            if (0 == pq_send_query(self, psyco_client_encoding)) {
                PyErr_SetString(OperationalError, PQerrorMessage(self->pgconn));
                break;
            }
            Dprintf("conn_poll: async_status -> ASYNC_WRITE");
            self->async_status = ASYNC_WRITE;
            res = PSYCO_POLL_WRITE;
        }
        break;

    case CONN_STATUS_CLIENT_ENCODING:
        res = _conn_poll_query(self);
        if (res == PSYCO_POLL_OK) {
            res = PSYCO_POLL_ERROR;
            pgres = pq_get_last_result(self);
            if (pgres == NULL || PQresultStatus(pgres) != PGRES_TUPLES_OK) {
                PyErr_SetString(OperationalError, "can't fetch client_encoding");
                break;
            }

            /* conn_get_encoding returns a malloc'd string */
            self->encoding = conn_get_encoding(pgres);
            CLEARPGRES(pgres);
            if (self->encoding == NULL) { break; }

            Dprintf("conn_poll: status -> CONN_STATUS_READY");
            self->status = CONN_STATUS_READY;
            res = PSYCO_POLL_OK;
        }
        break;
    }

    return res;
}


/* conn_poll - Main polling switch
 *
 * The function is called in all the states and connection types and invokes
 * the right "next step".
 */

int
conn_poll(connectionObject *self)
{
    int res = PSYCO_POLL_ERROR;
    Dprintf("conn_poll: status = %d", self->status);

    switch (self->status) {
    case CONN_STATUS_SETUP:
        Dprintf("conn_poll: status -> CONN_STATUS_CONNECTING");
        self->status = CONN_STATUS_CONNECTING;
        res = PSYCO_POLL_WRITE;
        break;

    case CONN_STATUS_CONNECTING:
        res = _conn_poll_connecting(self);
        if (res == PSYCO_POLL_OK && self->async) {
            res = _conn_poll_setup_async(self);
        }
        break;

    case CONN_STATUS_DATESTYLE:
    case CONN_STATUS_CLIENT_ENCODING:
        res = _conn_poll_setup_async(self);
        break;

    case CONN_STATUS_READY:
    case CONN_STATUS_BEGIN:
        res = _conn_poll_query(self);

        if (res == PSYCO_POLL_OK && self->async_cursor) {
            /* An async query has just finished: parse the tuple in the
             * target cursor. */
            cursorObject *curs = (cursorObject *)self->async_cursor;
            IFCLEARPGRES(curs->pgres);
            curs->pgres = pq_get_last_result(self);

            /* fetch the tuples (if there are any) and build the result. We
             * don't care if pq_fetch return 0 or 1, but if there was an error,
             * we want to signal it to the caller. */
            if (pq_fetch(curs) == -1) {
               res = PSYCO_POLL_ERROR;
            }

            /* We have finished with our async_cursor */
            Py_XDECREF(self->async_cursor);
            self->async_cursor = NULL;
        }
        break;

    default:
        Dprintf("conn_poll: in unexpected state");
        res = PSYCO_POLL_ERROR;
    }

    return res;
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

    /* execute a forced rollback on the connection (but don't check the
       result, we're going to close the pq connection anyway */
    if (self->pgconn && self->closed == 1) {
        PGresult *pgres = NULL;
        char *error = NULL;

        if (pq_abort_locked(self, &pgres, &error, &_save) < 0) {
            IFCLEARPGRES(pgres);
            if (error)
                free (error);
        }
    }

    if (self->closed == 0)
        self->closed = 1;

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
        res = pq_abort_locked(self, &pgres, &error, &_save);
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
    res = pq_abort_locked(self, &pgres, &error, &_save);

    if (res == 0) {
        res = pq_execute_command_locked(self, query, &pgres, &error, &_save);
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
