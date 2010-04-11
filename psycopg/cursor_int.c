/* cursor_int.c - code used by the cursor object
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
#include "psycopg/cursor.h"
#include "psycopg/pqpath.h"

/* curs_reset - reset the cursor to a clean state */

void
curs_reset(cursorObject *self)
{
    PyObject *tmp;

    /* initialize some variables to default values */
    self->notuples = 1;
    self->rowcount = -1;
    self->row = 0;

    tmp = self->description;
    Py_INCREF(Py_None);
    self->description = Py_None;
    Py_XDECREF(tmp);

    tmp = self->casts;
    self->casts = NULL;
    Py_XDECREF(tmp);
}

/*
 * curs_get_last_result
 *
 * read all results from the connection, save the last one
 * returns 0 if all results were read, 1 if there are remaining results, but
 * their retrieval would block, -1 if there was an error
 */

int
curs_get_last_result(cursorObject *self) {
    PGresult *pgres;

    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&(self->conn->lock));
    /* read one result, there can be multiple if the client sent multiple
       statements */
    while ((pgres = PQgetResult(self->conn->pgconn)) != NULL) {
        if (PQisBusy(self->conn->pgconn) == 1) {
            /* there is another result waiting, need to tell the client to
               wait more */
            Dprintf("curs_get_last_result: got result, but more are pending");
            IFCLEARPGRES(self->pgres);
            self->pgres = pgres;
            pthread_mutex_unlock(&(self->conn->lock));
            Py_BLOCK_THREADS;
            return 1;
        }
        Dprintf("curs_get_last_result: got result %p", pgres);
        IFCLEARPGRES(self->pgres);
        self->pgres = pgres;
    }
    Py_XDECREF(self->conn->async_cursor);
    self->conn->async_cursor = NULL;
    pthread_mutex_unlock(&(self->conn->lock));
    Py_END_ALLOW_THREADS;
    /* fetch the tuples (if there are any) and build the result. We don't care
       if pq_fetch return 0 or 1, but if there was an error, we want to signal
       it to the caller. */
    return pq_fetch(self) == -1 ? -1 : 0;
}

/* curs_poll_send - handle cursor polling when flushing output */

PyObject *
curs_poll_send(cursorObject *self)
{
    int res;

    /* flush queued output to the server */
    res = pq_flush(self->conn);

    if (res == 1) {
        /* some data still waiting to be flushed */
        Dprintf("cur_poll_send: returning %d", PSYCO_POLL_WRITE);
        return PyInt_FromLong(PSYCO_POLL_WRITE);
    }
    else if (res == 0) {
        /* all data flushed, start waiting for results */
        Dprintf("cur_poll_send: returning %d", PSYCO_POLL_READ);
        self->conn->async_status = ASYNC_READ;
        return PyInt_FromLong(PSYCO_POLL_READ);
    }
    else {
        /* unexpected result */
        PyErr_SetString(OperationalError, PQerrorMessage(self->conn->pgconn));
        return NULL;
    }
}

/* curs_poll_fetch - handle cursor polling when reading result */

PyObject *
curs_poll_fetch(cursorObject *self)
{
    int is_busy;
    int last_result;

    /* consume the input */
    is_busy = pq_is_busy(self->conn);
    if (is_busy == -1) {
        /* there was an error, raise the exception */
        return NULL;
    }
    else if (is_busy == 1) {
        /* the connection is busy, tell the user to wait more */
        Dprintf("cur_poll_fetch: returning %d", PSYCO_POLL_READ);
        return PyInt_FromLong(PSYCO_POLL_READ);
    }

    /* try to fetch the data only if this was a poll following a read
       request; else just return POLL_OK to the user: this is necessary
       because of asynchronous NOTIFYs that can be sent by the backend
       even if the user didn't asked for them */

    if (self->conn->async_status == ASYNC_READ)
        last_result = curs_get_last_result(self);
    else
        last_result = 0;

    if (last_result == 0) {
        Dprintf("cur_poll_fetch: returning %d", PSYCO_POLL_OK);
        /* self->conn->async_status cannot be ASYNC_WRITE here, because we
           never execute curs_poll_fetch in ASYNC_WRITE state, so we can
           safely set it to ASYNC_DONE because we either fetched the result or
           there is no result to fetch */
        self->conn->async_status = ASYNC_DONE;
        return PyInt_FromLong(PSYCO_POLL_OK);
    }
    else if (last_result == 1) {
        Dprintf("cur_poll_fetch: got result, but data remaining, "
                "returning %d", PSYCO_POLL_READ);
        return PyInt_FromLong(PSYCO_POLL_READ);
    }
    else {
        return NULL;
    }
}
