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

#define PSYCOPG_MODULE
#include "psycopg/psycopg.h"

#include "psycopg/connection.h"
#include "psycopg/cursor.h"
#include "psycopg/pqpath.h"
#include "psycopg/green.h"
#include "psycopg/notify.h"

#include <string.h>

/* Mapping from isolation level name to value exposed by Python.
 *
 * Note: ordering matters: to get a valid pre-PG 8 level from one not valid,
 * we increase a pointer in this list by one position. */
const IsolationLevel conn_isolevels[] = {
    {"",                    ISOLATION_LEVEL_AUTOCOMMIT},
    {"read uncommitted",    ISOLATION_LEVEL_READ_UNCOMMITTED},
    {"read committed",      ISOLATION_LEVEL_READ_COMMITTED},
    {"repeatable read",     ISOLATION_LEVEL_REPEATABLE_READ},
    {"serializable",        ISOLATION_LEVEL_SERIALIZABLE},
    {"default",            -1},     /* never to be found on the server */
    { NULL }
};


/* Return a new "string" from a char* from the database.
 *
 * On Py2 just get a string, on Py3 decode it in the connection codec.
 *
 * Use a fallback if the connection is NULL.
 */
PyObject *
conn_text_from_chars(connectionObject *self, const char *str)
{
#if PY_MAJOR_VERSION < 3
        return PyString_FromString(str);
#else
        const char *codec = self ? self->codec : "ascii";
        return PyUnicode_Decode(str, strlen(str), codec, "replace");
#endif
}

/* conn_notice_callback - process notices */

static void
conn_notice_callback(void *args, const char *message)
{
    struct connectionObject_notice *notice;
    connectionObject *self = (connectionObject *)args;

    Dprintf("conn_notice_callback: %s", message);

    /* NOTE: if we get here and the connection is unlocked then there is a
       problem but this should happen because the notice callback is only
       called from libpq and when we're inside libpq the connection is usually
       locked.
    */
    notice = (struct connectionObject_notice *)
        malloc(sizeof(struct connectionObject_notice));
    if (NULL == notice) {
        /* Discard the notice in case of failed allocation. */
        return;
    }
    notice->message = strdup(message);
    if (NULL == notice->message) {
        free(notice);
        return;
    }
    notice->next = self->notice_pending;
    self->notice_pending = notice;
}

/* Expose the notices received as Python objects.
 *
 * The function should be called with the connection lock and the GIL.
 */
void
conn_notice_process(connectionObject *self)
{
    struct connectionObject_notice *notice;
    Py_ssize_t nnotices;

    if (NULL == self->notice_pending) {
        return;
    }

    notice =  self->notice_pending;
    nnotices = PyList_GET_SIZE(self->notice_list);

    while (notice != NULL) {
        PyObject *msg;
        msg = conn_text_from_chars(self, notice->message);
        Dprintf("conn_notice_process: %s", notice->message);

        /* Respect the order in which notices were produced,
           because in notice_list they are reversed (see ticket #9) */
        if (msg) {
            PyList_Insert(self->notice_list, nnotices, msg);
            Py_DECREF(msg);
        }
        else {
            /* We don't really have a way to report errors, so gulp it.
             * The function should only fail for out of memory, so we are
             * likely going to die anyway. */
            PyErr_Clear();
        }

        notice = notice->next;
    }

    /* Remove the oldest item if the queue is getting too long. */
    nnotices = PyList_GET_SIZE(self->notice_list);
    if (nnotices > CONN_NOTICES_LIMIT) {
        PySequence_DelSlice(self->notice_list,
            0, nnotices - CONN_NOTICES_LIMIT);
    }

    conn_notice_clean(self);
}

void
conn_notice_clean(connectionObject *self)
{
    struct connectionObject_notice *tmp, *notice;

    notice = self->notice_pending;

    while (notice != NULL) {
        tmp = notice;
        notice = notice->next;
        free((void*)tmp->message);
        free(tmp);
    }

    self->notice_pending = NULL;
}


/* conn_notifies_process - make received notification available
 *
 * The function should be called with the connection lock and holding the GIL.
 */

void
conn_notifies_process(connectionObject *self)
{
    PGnotify *pgn = NULL;
    PyObject *notify = NULL;
    PyObject *pid = NULL, *channel = NULL, *payload = NULL;

    while ((pgn = PQnotifies(self->pgconn)) != NULL) {

        Dprintf("conn_notifies_process: got NOTIFY from pid %d, msg = %s",
                (int) pgn->be_pid, pgn->relname);

        if (!(pid = PyInt_FromLong((long)pgn->be_pid))) { goto error; }
        if (!(channel = conn_text_from_chars(self, pgn->relname))) { goto error; }
        if (!(payload = conn_text_from_chars(self, pgn->extra))) { goto error; }

        if (!(notify = PyObject_CallFunctionObjArgs((PyObject *)&notifyType,
                pid, channel, payload, NULL))) {
            goto error;
        }

        Py_DECREF(pid); pid = NULL;
        Py_DECREF(channel); channel = NULL;
        Py_DECREF(payload); payload = NULL;

        PyList_Append(self->notifies, (PyObject *)notify);

        Py_DECREF(notify); notify = NULL;
        PQfreemem(pgn); pgn = NULL;
    }
    return;  /* no error */

error:
    if (pgn) { PQfreemem(pgn); }
    Py_XDECREF(notify);
    Py_XDECREF(pid);
    Py_XDECREF(channel);
    Py_XDECREF(payload);

    /* TODO: callers currently don't expect an error from us */
    PyErr_Clear();

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
     * If the parameter is off, the PQescapeByteaConn returns
     * backslash escaped strings (e.g. '\001' -> "\\001"),
     * so the E'' quotes are required to avoid warnings
     * if 'escape_string_warning' is set.
     *
     * If the parameter is on, the PQescapeByteaConn returns
     * not escaped strings (e.g. '\001' -> "\001"), relying on the
     * fact that the '\' will pass untouched the string parser.
     * In this case the E'' quotes are NOT to be used.
     */
    scs = PQparameterStatus(pgconn, "standard_conforming_strings");
    Dprintf("conn_connect: server standard_conforming_strings parameter: %s",
        scs ? scs : "unavailable");

    equote = (scs && (0 == strcmp("off", scs)));
    Dprintf("conn_connect: server requires E'' quotes: %s",
            equote ? "YES" : "NO");

    return equote;
}


/* Remove irrelevant chars from encoding name and turn it uppercase.
 *
 * Return a buffer allocated on Python heap into 'clean' and return 0 on
 * success, otherwise return -1 and set an exception.
 */
RAISES_NEG static int
clear_encoding_name(const char *enc, char **clean)
{
    const char *i = enc;
    char *j, *buf;
    int rv = -1;

    /* convert to upper case and remove '-' and '_' from string */
    if (!(j = buf = PyMem_Malloc(strlen(enc) + 1))) {
        PyErr_NoMemory();
        goto exit;
    }

    while (*i) {
        if (!isalnum(*i)) {
            ++i;
        }
        else {
            *j++ = toupper(*i++);
        }
    }
    *j = '\0';

    Dprintf("clear_encoding_name: %s -> %s", enc, buf);
    *clean = buf;
    rv = 0;

exit:
    return rv;
}

/* Convert a PostgreSQL encoding to a Python codec.
 *
 * Set 'codec' to a new copy of the codec name allocated on the Python heap.
 * Return 0 in case of success, else -1 and set an exception.
 *
 * 'enc' should be already normalized (uppercase, no - or _).
 */
RAISES_NEG static int
conn_encoding_to_codec(const char *enc, char **codec)
{
    char *tmp;
    Py_ssize_t size;
    PyObject *pyenc = NULL;
    int rv = -1;

    /* Find the Py codec name from the PG encoding */
    if (!(pyenc = PyDict_GetItemString(psycoEncodings, enc))) {
        PyErr_Format(OperationalError,
            "no Python codec for client encoding '%s'", enc);
        goto exit;
    }

    /* Convert the codec in a bytes string to extract the c string. */
    Py_INCREF(pyenc);
    if (!(pyenc = psycopg_ensure_bytes(pyenc))) {
        goto exit;
    }

    if (-1 == Bytes_AsStringAndSize(pyenc, &tmp, &size)) {
        goto exit;
    }

    /* have our own copy of the python codec name */
    rv = psycopg_strdup(codec, tmp, size);

exit:
    Py_XDECREF(pyenc);
    return rv;
}

/* Read the client encoding from the connection.
 *
 * Store the encoding in the pgconn->encoding field and the name of the
 * matching python codec in codec. The buffers are allocated on the Python
 * heap.
 *
 * Return 0 on success, else nonzero.
 */
RAISES_NEG static int
conn_read_encoding(connectionObject *self, PGconn *pgconn)
{
    char *enc = NULL, *codec = NULL;
    const char *tmp;
    int rv = -1;

    tmp = PQparameterStatus(pgconn, "client_encoding");
    Dprintf("conn_connect: client encoding: %s", tmp ? tmp : "(none)");
    if (!tmp) {
        PyErr_SetString(OperationalError,
            "server didn't return client encoding");
        goto exit;
    }

    if (0 > clear_encoding_name(tmp, &enc)) {
        goto exit;
    }

    /* Look for this encoding in Python codecs. */
    if (0 > conn_encoding_to_codec(enc, &codec)) {
        goto exit;
    }

    /* Good, success: store the encoding/codec in the connection. */
    PyMem_Free(self->encoding);
    self->encoding = enc;
    enc = NULL;

    PyMem_Free(self->codec);
    self->codec = codec;
    codec = NULL;

    rv = 0;

exit:
    PyMem_Free(enc);
    PyMem_Free(codec);
    return rv;
}


RAISES_NEG int
conn_get_isolation_level(connectionObject *self)
{
    PGresult *pgres = NULL;
    char *error = NULL;
    int rv = -1;
    char *lname;
    const IsolationLevel *level;

    /* this may get called by async connections too: here's your result */
    if (self->autocommit) {
        return 0;
    }

    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&self->lock);

    if (!(lname = pq_get_guc_locked(self, "default_transaction_isolation",
            &pgres, &error, &_save))) {
        goto endlock;
    }

    /* find the value for the requested isolation level */
    level = conn_isolevels;
    while ((++level)->name) {
        if (0 == strcasecmp(level->name, lname)) {
            rv = level->value;
            break;
        }
    }
    if (-1 == rv) {
        error = malloc(256);
        PyOS_snprintf(error, 256,
            "unexpected isolation level: '%s'", lname);
    }

    free(lname);

endlock:
    pthread_mutex_unlock(&self->lock);
    Py_END_ALLOW_THREADS;

    if (rv < 0) {
        pq_complete_error(self, &pgres, &error);
    }

    return rv;
}


int
conn_get_protocol_version(PGconn *pgconn)
{
    int ret;
    ret = PQprotocolVersion(pgconn);
    Dprintf("conn_connect: using protocol %d", ret);
    return ret;
}

int
conn_get_server_version(PGconn *pgconn)
{
    return (int)PQserverVersion(pgconn);
}

/* set up the cancel key of the connection.
 * On success return 0, else set an exception and return -1
 */
RAISES_NEG static int
conn_setup_cancel(connectionObject *self, PGconn *pgconn)
{
    if (self->cancel) {
        PQfreeCancel(self->cancel);
    }

    if (!(self->cancel = PQgetCancel(self->pgconn))) {
        PyErr_SetString(OperationalError, "can't get cancellation key");
        return -1;
    }

    return 0;
}


/* Return 1 if the server datestyle allows us to work without problems,
   0 if it needs to be set to something better, e.g. ISO. */
static int
conn_is_datestyle_ok(PGconn *pgconn)
{
    const char *ds;

    ds = PQparameterStatus(pgconn, "DateStyle");
    Dprintf("conn_connect: DateStyle %s", ds);

    /* pgbouncer does not pass on DateStyle */
    if (ds == NULL)
      return 0;

    /* Return true if ds starts with "ISO"
     * e.g. "ISO, DMY" is fine, "German" not. */
    return (ds[0] == 'I' && ds[1] == 'S' && ds[2] == 'O');
}


/* conn_setup - setup and read basic information about the connection */

RAISES_NEG int
conn_setup(connectionObject *self, PGconn *pgconn)
{
    PGresult *pgres = NULL;
    char *error = NULL;

    self->equote = conn_get_standard_conforming_strings(pgconn);
    self->server_version = conn_get_server_version(pgconn);
    self->protocol = conn_get_protocol_version(self->pgconn);
    if (3 != self->protocol) {
        PyErr_SetString(InterfaceError, "only protocol 3 supported");
        return -1;
    }

    if (0 > conn_read_encoding(self, pgconn)) {
        return -1;
    }

    if (0 > conn_setup_cancel(self, pgconn)) {
        return -1;
    }

    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&self->lock);
    Py_BLOCK_THREADS;

    if (!conn_is_datestyle_ok(self->pgconn)) {
        int res;
        Py_UNBLOCK_THREADS;
        res = pq_set_guc_locked(self, "datestyle", "ISO",
            &pgres, &error, &_save);
        Py_BLOCK_THREADS;
        if (res < 0) {
            pq_complete_error(self, &pgres, &error);
            return -1;
        }
    }

    /* for reset */
    self->autocommit = 0;

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
        if (0 > pq_set_non_blocking(self, 1)) {
            return -1;
        }
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

    /* Set the connection to nonblocking now. */
    if (pq_set_non_blocking(self, 1) != 0) {
        return -1;
    }

    /* The connection will be completed banging on poll():
     * First with _conn_poll_connecting() that will finish connection,
     * then with _conn_poll_setup_async() that will do the same job
     * of setup_async(). */

    return 0;
}

int
conn_connect(connectionObject *self, long int async)
{
    int rv;

    if (async == 1) {
      Dprintf("con_connect: connecting in ASYNC mode");
      rv = _conn_async_connect(self);
    }
    else {
      Dprintf("con_connect: connecting in SYNC mode");
      rv = _conn_sync_connect(self);
    }

    if (rv != 0) {
        /* connection failed, so let's close ourselves */
        self->closed = 2;
    }

    return rv;
}


/* poll during a connection attempt until the connection has established. */

static int
_conn_poll_connecting(connectionObject *self)
{
    int res = PSYCO_POLL_ERROR;
    const char *msg;

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
        msg = PQerrorMessage(self->pgconn);
        if (!(msg && *msg)) {
            msg = "asynchronous connection failed";
        }
        PyErr_SetString(OperationalError, msg);
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
        self->equote = conn_get_standard_conforming_strings(self->pgconn);
        self->protocol = conn_get_protocol_version(self->pgconn);
        self->server_version = conn_get_server_version(self->pgconn);
        if (3 != self->protocol) {
            PyErr_SetString(InterfaceError, "only protocol 3 supported");
            break;
        }
        if (0 > conn_read_encoding(self, self->pgconn)) {
            break;
        }
        if (0 > conn_setup_cancel(self, self->pgconn)) {
            return -1;
        }

        /* asynchronous connections always use isolation level 0, the user is
         * expected to manage the transactions himself, by sending
         * (asynchronously) BEGIN and COMMIT statements.
         */
        self->autocommit = 1;

        /* If the datestyle is ISO or anything else good,
         * we can skip the CONN_STATUS_DATESTYLE step. */
        if (!conn_is_datestyle_ok(self->pgconn)) {
            Dprintf("conn_poll: status -> CONN_STATUS_DATESTYLE");
            self->status = CONN_STATUS_DATESTYLE;
            if (0 == pq_send_query(self, psyco_datestyle)) {
                PyErr_SetString(OperationalError, PQerrorMessage(self->pgconn));
                break;
            }
            Dprintf("conn_poll: async_status -> ASYNC_WRITE");
            self->async_status = ASYNC_WRITE;
            res = PSYCO_POLL_WRITE;
        }
        else {
            Dprintf("conn_poll: status -> CONN_STATUS_READY");
            self->status = CONN_STATUS_READY;
            res = PSYCO_POLL_OK;
        }
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
        res = _conn_poll_setup_async(self);
        break;

    case CONN_STATUS_READY:
    case CONN_STATUS_BEGIN:
    case CONN_STATUS_PREPARED:
        res = _conn_poll_query(self);

        if (res == PSYCO_POLL_OK && self->async && self->async_cursor) {
            /* An async query has just finished: parse the tuple in the
             * target cursor. */
            cursorObject *curs;
            PyObject *py_curs = PyWeakref_GetObject(self->async_cursor);
            if (Py_None == py_curs) {
                pq_clear_async(self);
                PyErr_SetString(InterfaceError,
                    "the asynchronous cursor has disappeared");
                res = PSYCO_POLL_ERROR;
                break;
            }

            curs = (cursorObject *)py_curs;
            CLEARPGRES(curs->pgres);
            curs->pgres = pq_get_last_result(self);

            /* fetch the tuples (if there are any) and build the result. We
             * don't care if pq_fetch return 0 or 1, but if there was an error,
             * we want to signal it to the caller. */
            if (pq_fetch(curs, 0) == -1) {
               res = PSYCO_POLL_ERROR;
            }

            /* We have finished with our async_cursor */
            Py_CLEAR(self->async_cursor);
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
    /* a connection with closed == 2 still requires cleanup */
    if (self->closed == 1) {
        return;
    }

    /* sets this connection as closed even for other threads; */
    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&self->lock);

    conn_close_locked(self);

    pthread_mutex_unlock(&self->lock);
    Py_END_ALLOW_THREADS;
}

/* conn_close_locked - shut down the connection with the lock already taken */

void conn_close_locked(connectionObject *self)
{
    if (self->closed == 1) {
        return;
    }

    /* We used to call pq_abort_locked here, but the idea of issuing a
     * rollback on close/GC has been considered inappropriate.
     *
     * Dropping the connection on the server has the same effect as the
     * transaction is automatically rolled back. Some middleware, such as
     * PgBouncer, have problem with connections closed in the middle of the
     * transaction though: to avoid these problems the transaction should be
     * closed only in status CONN_STATUS_READY.
     */
    self->closed = 1;

    /* we need to check the value of pgconn, because we get called even when
     * the connection fails! */
    if (self->pgconn) {
        PQfinish(self->pgconn);
        self->pgconn = NULL;
        Dprintf("conn_close: PQfinish called");
    }
}

/* conn_commit - commit on a connection */

RAISES_NEG int
conn_commit(connectionObject *self)
{
    int res;

    res = pq_commit(self);
    return res;
}

/* conn_rollback - rollback a connection */

RAISES_NEG int
conn_rollback(connectionObject *self)
{
    int res;

    res = pq_abort(self);
    return res;
}

RAISES_NEG int
conn_set_session(connectionObject *self,
        const char *isolevel, const char *readonly, const char *deferrable,
        int autocommit)
{
    PGresult *pgres = NULL;
    char *error = NULL;
    int res = -1;

    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&self->lock);

    if (isolevel) {
        Dprintf("conn_set_session: setting isolation to %s", isolevel);
        if ((res = pq_set_guc_locked(self,
                "default_transaction_isolation", isolevel,
                &pgres, &error, &_save))) {
            goto endlock;
        }
    }

    if (readonly) {
        Dprintf("conn_set_session: setting read only to %s", readonly);
        if ((res = pq_set_guc_locked(self,
                "default_transaction_read_only", readonly,
                &pgres, &error, &_save))) {
            goto endlock;
        }
    }

    if (deferrable) {
        Dprintf("conn_set_session: setting deferrable to %s", deferrable);
        if ((res = pq_set_guc_locked(self,
                "default_transaction_deferrable", deferrable,
                &pgres, &error, &_save))) {
            goto endlock;
        }
    }

    if (self->autocommit != autocommit) {
        Dprintf("conn_set_session: setting autocommit to %d", autocommit);
        self->autocommit = autocommit;
    }

    res = 0;

endlock:
    pthread_mutex_unlock(&self->lock);
    Py_END_ALLOW_THREADS;

    if (res < 0) {
        pq_complete_error(self, &pgres, &error);
    }

    return res;
}

int
conn_set_autocommit(connectionObject *self, int value)
{
    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&self->lock);

    self->autocommit = value;

    pthread_mutex_unlock(&self->lock);
    Py_END_ALLOW_THREADS;

    return 0;
}

/* conn_switch_isolation_level - switch isolation level on the connection */

RAISES_NEG int
conn_switch_isolation_level(connectionObject *self, int level)
{
    PGresult *pgres = NULL;
    char *error = NULL;
    int curr_level;
    int ret = -1;

    /* use only supported levels on older PG versions */
    if (self->server_version < 80000) {
        if (level == ISOLATION_LEVEL_READ_UNCOMMITTED)
            level = ISOLATION_LEVEL_READ_COMMITTED;
        else if (level == ISOLATION_LEVEL_REPEATABLE_READ)
            level = ISOLATION_LEVEL_SERIALIZABLE;
    }

    if (-1 == (curr_level = conn_get_isolation_level(self))) {
        return -1;
    }

    if (curr_level == level) {
        /* no need to change level */
        return 0;
    }

    /* Emulate the previous semantic of set_isolation_level() using the
     * functions currently available. */

    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&self->lock);

    /* terminate the current transaction if any */
    if ((ret = pq_abort_locked(self, &pgres, &error, &_save))) {
        goto endlock;
    }

    if (level == 0) {
        if ((ret = pq_set_guc_locked(self,
                "default_transaction_isolation", "default",
                &pgres, &error, &_save))) {
            goto endlock;
        }
        self->autocommit = 1;
    }
    else {
        /* find the name of the requested level */
        const IsolationLevel *isolevel = conn_isolevels;
        while ((++isolevel)->name) {
            if (level == isolevel->value) {
                break;
            }
        }
        if (!isolevel->name) {
            ret = -1;
            error = strdup("bad isolation level value");
            goto endlock;
        }

        if ((ret = pq_set_guc_locked(self,
                "default_transaction_isolation", isolevel->name,
                &pgres, &error, &_save))) {
            goto endlock;
        }
        self->autocommit = 0;
    }

    Dprintf("conn_switch_isolation_level: switched to level %d", level);

endlock:
    pthread_mutex_unlock(&self->lock);
    Py_END_ALLOW_THREADS;

    if (ret < 0) {
        pq_complete_error(self, &pgres, &error);
    }

    return ret;
}


/* conn_set_client_encoding - switch client encoding on connection */

RAISES_NEG int
conn_set_client_encoding(connectionObject *self, const char *enc)
{
    PGresult *pgres = NULL;
    char *error = NULL;
    int res = -1;
    char *codec = NULL;
    char *clean_enc = NULL;

    /* If the current encoding is equal to the requested one we don't
       issue any query to the backend */
    if (strcmp(self->encoding, enc) == 0) return 0;

    /* We must know what python codec this encoding is. */
    if (0 > clear_encoding_name(enc, &clean_enc)) { goto exit; }
    if (0 > conn_encoding_to_codec(clean_enc, &codec)) { goto exit; }

    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&self->lock);

    /* abort the current transaction, to set the encoding ouside of
       transactions */
    if ((res = pq_abort_locked(self, &pgres, &error, &_save))) {
        goto endlock;
    }

    if ((res = pq_set_guc_locked(self, "client_encoding", clean_enc,
            &pgres, &error, &_save))) {
        goto endlock;
    }

    /* no error, we can proceed and store the new encoding */
    {
        char *tmp = self->encoding;
        self->encoding = clean_enc;
        PyMem_Free(tmp);
        clean_enc = NULL;
    }

    /* Store the python codec too. */
    {
        char *tmp = self->codec;
        self->codec = codec;
        PyMem_Free(tmp);
        codec = NULL;
    }

    Dprintf("conn_set_client_encoding: set encoding to %s (codec: %s)",
            self->encoding, self->codec);

endlock:
    pthread_mutex_unlock(&self->lock);
    Py_END_ALLOW_THREADS;

    if (res < 0)
        pq_complete_error(self, &pgres, &error);

exit:
    PyMem_Free(clean_enc);
    PyMem_Free(codec);

    return res;
}


/* conn_tpc_begin -- begin a two-phase commit.
 *
 * The state of a connection in the middle of a TPC is exactly the same
 * of a normal transaction, in CONN_STATUS_BEGIN, but with the tpc_xid
 * member set to the xid used. This allows to reuse all the code paths used
 * in regular transactions, as PostgreSQL won't even know we are in a TPC
 * until PREPARE. */

RAISES_NEG int
conn_tpc_begin(connectionObject *self, xidObject *xid)
{
    PGresult *pgres = NULL;
    char *error = NULL;

    Dprintf("conn_tpc_begin: starting transaction");

    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&self->lock);

    if (pq_begin_locked(self, &pgres, &error, &_save) < 0) {
        pthread_mutex_unlock(&(self->lock));
        Py_BLOCK_THREADS;
        pq_complete_error(self, &pgres, &error);
        return -1;
    }

    pthread_mutex_unlock(&self->lock);
    Py_END_ALLOW_THREADS;

    /* The transaction started ok, let's store this xid. */
    Py_INCREF(xid);
    self->tpc_xid = xid;

    return 0;
}


/* conn_tpc_command -- run one of the TPC-related PostgreSQL commands.
 *
 * The function doesn't change the connection state as it can be used
 * for many commands and for recovered transactions. */

RAISES_NEG int
conn_tpc_command(connectionObject *self, const char *cmd, xidObject *xid)
{
    PGresult *pgres = NULL;
    char *error = NULL;
    PyObject *tid = NULL;
    const char *ctid;
    int rv = -1;

    Dprintf("conn_tpc_command: %s", cmd);

    /* convert the xid into PostgreSQL transaction id while keeping the GIL */
    if (!(tid = psycopg_ensure_bytes(xid_get_tid(xid)))) { goto exit; }
    if (!(ctid = Bytes_AsString(tid))) { goto exit; }

    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&self->lock);

    if (0 > (rv = pq_tpc_command_locked(self, cmd, ctid,
                                        &pgres, &error, &_save))) {
        pthread_mutex_unlock(&self->lock);
        Py_BLOCK_THREADS;
        pq_complete_error(self, &pgres, &error);
        goto exit;
    }

    pthread_mutex_unlock(&self->lock);
    Py_END_ALLOW_THREADS;

exit:
    Py_XDECREF(tid);
    return rv;
}

/* conn_tpc_recover -- return a list of pending TPC Xid */

PyObject *
conn_tpc_recover(connectionObject *self)
{
    int status;
    PyObject *xids = NULL;
    PyObject *rv = NULL;
    PyObject *tmp;

    /* store the status to restore it. */
    status = self->status;

    if (!(xids = xid_recover((PyObject *)self))) { goto exit; }

    if (status == CONN_STATUS_READY && self->status == CONN_STATUS_BEGIN) {
        /* recover began a transaction: let's abort it. */
        if (!(tmp = PyObject_CallMethod((PyObject *)self, "rollback", NULL))) {
            goto exit;
        }
        Py_DECREF(tmp);
    }

    /* all fine */
    rv = xids;
    xids = NULL;

exit:
    Py_XDECREF(xids);

    return rv;

}
