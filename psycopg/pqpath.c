/* pqpath.c - single path into libpq
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

/* IMPORTANT NOTE: no function in this file do its own connection locking
   except for pg_execute and pq_fetch (that are somehow high-level. This means
   that all the othe functions should be called while holding a lock to the
   connection.
*/

#include <Python.h>
#include <string.h>

#define PSYCOPG_MODULE
#include "psycopg/config.h"
#include "psycopg/python.h"
#include "psycopg/psycopg.h"
#include "psycopg/pqpath.h"
#include "psycopg/connection.h"
#include "psycopg/cursor.h"
#include "psycopg/typecast.h"
#include "psycopg/pgtypes.h"
#include "psycopg/pgversion.h"

/* pq_raise - raise a python exception of the right kind

   This function should be called while holding the GIL. */

void
pq_raise(connectionObject *conn, cursorObject *curs, PyObject *exc, char *msg)
{
    PyObject *pgc = (PyObject*)curs;
    
    char *err = NULL;
    char *err2 = NULL;
    char *code = NULL;
    char *buf = NULL;
    
    if ((conn == NULL && curs == NULL) || (curs != NULL && conn == NULL)) {
        PyErr_SetString(Error, "psycopg went psycotic and raised a null error");
        return;
    }
    
    if (curs && curs->pgres) {
        err = PQresultErrorMessage(curs->pgres);
#ifdef HAVE_PQPROTOCOL3
        if (err != NULL && conn->protocol == 3) {
            code = PQresultErrorField(curs->pgres, PG_DIAG_SQLSTATE);
        }
#endif
    }
    if (err == NULL)
        err = PQerrorMessage(conn->pgconn);

    /* if the is no error message we probably called pq_raise without reason:
       we need to set an exception anyway because the caller will probably
       raise and a meaningful message is better than an empty one */
    if (err == NULL) {
        PyErr_SetString(Error, "psycopg went psycotic without error set");
        return;
    }
    
    /* if exc is NULL, analyze the message and try to deduce the right
       exception kind (only if we have a pgres, obviously) */
    if (exc == NULL) {
        if (curs && curs->pgres) {
            if (conn->protocol == 3) {
#ifdef HAVE_PQPROTOCOL3
                char *pgstate =
                    PQresultErrorField(curs->pgres, PG_DIAG_SQLSTATE);
                if (pgstate != NULL && !strncmp(pgstate, "23", 2))
                    exc = IntegrityError;
                else
                    exc = ProgrammingError;
#endif
            }
        }
    }
    
    /* if exc is still NULL psycopg was not built with HAVE_PQPROTOCOL3 or the
       connection is using protocol 2: in both cases we default to comparing
       error messages */
    if (exc == NULL) {
        if (!strncmp(err, "ERROR:  Cannot insert a duplicate key", 37)
            || !strncmp(err, "ERROR:  ExecAppend: Fail to add null", 36)
            || strstr(err, "referential integrity violation"))
            exc = IntegrityError;
        else
            exc = ProgrammingError;
    }
    
    /* try to remove the initial "ERROR: " part from the postgresql error */
    if (err && strlen(err) > 8) err2 = &(err[8]);
    else err2 = err;

    /* if msg is not NULL, add it to the error message, after a '\n' */
    if (msg && code) {
        int len = strlen(code) + strlen(err) + strlen(msg) + 5;
        if ((buf = PyMem_Malloc(len))) {
            snprintf(buf, len, "[%s] %s\n%s", code, err2, msg);
            psyco_set_error(exc, pgc, buf, err, code);
        }
    }
    else if (msg) {
        int len = strlen(err) + strlen(msg) + 2;
        if ((buf = PyMem_Malloc(len))) {
            snprintf(buf, len, "%s\n%s", err2, msg);
            psyco_set_error(exc, pgc, buf, err, code);
        }
    }
    else {
        psyco_set_error(exc, pgc, err2, err, code);        
    }
    
    if (buf != NULL) PyMem_Free(buf);
}

/* pq_set_critical, pq_resolve_critical - manage critical errors

   this function is invoked when a PQexec() call returns NULL, meaning a
   critical condition like out of memory or lost connection. it save the error
   message and mark the connection as 'wanting cleanup'.

   both functions do not call any Py_*_ALLOW_THREADS macros.
   pq_resolve_critical should be called while holding the GIL. */

void
pq_set_critical(connectionObject *conn, const char *msg)
{
    if (msg == NULL) 
        msg = PQerrorMessage(conn->pgconn);
    if (conn->critical) free(conn->critical);
    if (msg && msg[0] != '\0') conn->critical = strdup(msg);
    else conn->critical = NULL;
}

PyObject *
pq_resolve_critical(connectionObject *conn, int close)
{
    Dprintf("pq_resolve_critical: resolving %s", conn->critical);
    
    if (conn->critical) {
        char *msg = &(conn->critical[6]);
        Dprintf("pq_resolve_critical: error = %s", msg);
        /* we can't use pq_raise because the error has already been cleared
           from the connection, so we just raise an OperationalError with the
           critical message */
        PyErr_SetString(OperationalError, msg);
        
        /* we don't want to destroy this connection but just close it */
        if (close == 1) conn_close(conn);
    }
    return NULL;
}

/* pq_clear_async - clear the effects of a previous async query

   note that this function does block because it needs to wait for the full
   result sets of the previous query to clear them.

   
   this function does not call any Py_*_ALLOW_THREADS macros */

void
pq_clear_async(connectionObject *conn)
{
    PGresult *pgres;

    do {
        pgres = PQgetResult(conn->pgconn);
        Dprintf("pq_clear_async: clearing PGresult at %p", pgres);
        IFCLEARPGRES(pgres);
    } while (pgres != NULL);
}

/* pq_begin - send a BEGIN WORK, if necessary

   this function does not call any Py_*_ALLOW_THREADS macros */

int
pq_begin(connectionObject *conn)
{
    const char *query[] = {
        NULL,
        "BEGIN; SET TRANSACTION ISOLATION LEVEL READ COMMITTED",
        "BEGIN; SET TRANSACTION ISOLATION LEVEL SERIALIZABLE"};
    
    int pgstatus, retvalue = -1;
    PGresult *pgres = NULL;

    Dprintf("pq_begin: pgconn = %p, isolevel = %ld, status = %d",
            conn->pgconn, conn->isolation_level, conn->status);

    if (conn->isolation_level == 0 || conn->status != CONN_STATUS_READY) {
        Dprintf("pq_begin: transaction in progress");
        return 0;
    }

    pq_clear_async(conn);
    pgres = PQexec(conn->pgconn, query[conn->isolation_level]);
    if (pgres == NULL) {
        Dprintf("pq_begin: PQexec() failed");
        pq_set_critical(conn, NULL);
        goto cleanup;
    }

    pgstatus = PQresultStatus(pgres);
    if (pgstatus != PGRES_COMMAND_OK ) {
        Dprintf("pq_begin: result is NOT OK");
        pq_set_critical(conn, NULL);
        goto cleanup;
    }
    Dprintf("pq_begin: issued '%s' command", query[conn->isolation_level]);

    retvalue = 0;
    conn->status = CONN_STATUS_BEGIN;

 cleanup:
    IFCLEARPGRES(pgres);
    return retvalue;
}

/* pq_commit - send an END, if necessary

   this function does not call any Py_*_ALLOW_THREADS macros */

int
pq_commit(connectionObject *conn)
{
    const char *query = "END";
    int pgstatus, retvalue = -1;
    PGresult *pgres = NULL;

    Dprintf("pq_commit: pgconn = %p, isolevel = %ld, status = %d",
            conn->pgconn, conn->isolation_level, conn->status);

    if (conn->isolation_level == 0 || conn->status != CONN_STATUS_BEGIN) {
        Dprintf("pq_commit: no transaction to commit");
        return 0;
    }

    pq_clear_async(conn);
    pgres = PQexec(conn->pgconn, query);
    if (pgres == NULL) {
        Dprintf("pq_commit: PQexec() failed");
        pq_set_critical(conn, NULL);
        goto cleanup;
    }

    pgstatus = PQresultStatus(pgres);
    if (pgstatus != PGRES_COMMAND_OK ) {
        Dprintf("pq_commit: result is NOT OK");
        pq_set_critical(conn, NULL);
        goto cleanup;
    }
    Dprintf("pq_commit: issued '%s' command", query);

    retvalue = 0;
    conn->status = CONN_STATUS_READY;

 cleanup:
    IFCLEARPGRES(pgres);
    return retvalue;
}

/* pq_abort - send an ABORT, if necessary

   this function does not call any Py_*_ALLOW_THREADS macros */

int
pq_abort(connectionObject *conn)
{
    const char *query = "ABORT";
    int pgstatus, retvalue = -1;
    PGresult *pgres = NULL;

    Dprintf("pq_abort: pgconn = %p, isolevel = %ld, status = %d",
            conn->pgconn, conn->isolation_level, conn->status);

    if (conn->isolation_level == 0 || conn->status != CONN_STATUS_BEGIN) {
        Dprintf("pq_abort: no transaction to abort");
        return 0;
    }

    pq_clear_async(conn);
    pgres = PQexec(conn->pgconn, query);
    if (pgres == NULL) {
        Dprintf("pq_abort: PQexec() failed");
        pq_set_critical(conn, NULL);
        goto cleanup;
    }

    pgstatus = PQresultStatus(pgres);
    if (pgstatus != PGRES_COMMAND_OK ) {
        Dprintf("pq_abort: result is NOT OK");
        pq_set_critical(conn, NULL);
        goto cleanup;
    }
    Dprintf("pq_abort: issued '%s' command", query);

    retvalue = 0;
    conn->status = CONN_STATUS_READY;

 cleanup:
    IFCLEARPGRES(pgres);
    return retvalue;
}

/* pq_is_busy - consume input and return connection status
 
   a status of 1 means that a call to pq_fetch will block, while a status of 0
   means that there is data available to be collected. -1 means an error, the
   exception will be set accordingly.

   this fucntion locks the connection object
   this function call Py_*_ALLOW_THREADS macros */

int
pq_is_busy(connectionObject *conn)
{
    PGnotify *pgn;
    
    Dprintf("pq_is_busy: consuming input");

    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&(conn->lock));

    if (PQconsumeInput(conn->pgconn) == 0) {
        Dprintf("pq_is_busy: PQconsumeInput() failed");
        pthread_mutex_unlock(&(conn->lock));
        Py_BLOCK_THREADS;
        PyErr_SetString(OperationalError, PQerrorMessage(conn->pgconn));
        return -1;
    }

    pthread_mutex_unlock(&(conn->lock));
    Py_END_ALLOW_THREADS;
    
    /* now check for notifies */
    while ((pgn = PQnotifies(conn->pgconn)) != NULL) {
        PyObject *notify;
        
        Dprintf("curs_is_busy: got NOTIFY from pid %d, msg = %s",
                pgn->be_pid, pgn->relname);

        notify = PyTuple_New(2);
        PyTuple_SET_ITEM(notify, 0, PyInt_FromLong((long)pgn->be_pid));
        PyTuple_SET_ITEM(notify, 1, PyString_FromString(pgn->relname));
        PyList_Append(conn->notifies, notify);
        free(pgn);
    }
    
    return PQisBusy(conn->pgconn);
}

/* pq_execute - execute a query, possibly asyncronously

   this fucntion locks the connection object
   this function call Py_*_ALLOW_THREADS macros */

int
pq_execute(cursorObject *curs, const char *query, int async)
{
    /* if the status of the connection is critical raise an exception and
       definitely close the connection */
    if (curs->conn->critical) {
        pq_resolve_critical(curs->conn, 1);
        return -1;
    }

    /* check status of connection, raise error if not OK */
    if (PQstatus(curs->conn->pgconn) != CONNECTION_OK) {
        Dprintf("pq_execute: connection NOT OK");
        PyErr_SetString(OperationalError, PQerrorMessage(curs->conn->pgconn));
        return -1;
    }
    Dprintf("curs_execute: pg connection at %p OK", curs->conn->pgconn);

    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&(curs->conn->lock));

    pq_begin(curs->conn);

    if (async == 0) {
        IFCLEARPGRES(curs->pgres);
        Dprintf("pq_execute: executing SYNC query:");
        Dprintf("    %-.200s", query);
        curs->pgres = PQexec(curs->conn->pgconn, query);
    }

    else if (async == 1) {
        /* first of all, let see if the previous query has already ended, if
           not what should we do? just block and discard data or execute
           another query? */
        pq_clear_async(curs->conn);
        
        Dprintf("pq_execute: executing ASYNC query:");
        Dprintf("    %-.200s", query);
        
        /* then we can go on and send a new query without fear */
        IFCLEARPGRES(curs->pgres);
        if (PQsendQuery(curs->conn->pgconn, query) == 0) {
            pthread_mutex_unlock(&(curs->conn->lock));
            Py_BLOCK_THREADS;
            PyErr_SetString(OperationalError,
                            PQerrorMessage(curs->conn->pgconn));
            return -1;
        }
        Dprintf("pq_execute: async query sent to backend");
    }
    
    pthread_mutex_unlock(&(curs->conn->lock));
    Py_END_ALLOW_THREADS;
    
    /* if the execute was sync, we call pq_fetch() immediately,
       to respect the old DBAPI-2.0 compatible behaviour */
    if (async == 0) {
        Dprintf("pq_execute: entering syncronous DBAPI compatibility mode");
        if (pq_fetch(curs) == -1) return -1;
    }
    else {
        curs->conn->async_cursor = (PyObject*)curs;
    }
    
    return 1-async;
}


/* pq_fetch - fetch data after a query

   this fucntion locks the connection object
   this function call Py_*_ALLOW_THREADS macros

   return value:
     -1 - some error occurred while calling libpq
      0 - no result from the backend but no libpq errors
      1 - result from backend (possibly data is ready)
*/

static void
_pq_fetch_tuples(cursorObject *curs)
{
    int i, *dsize = NULL;

    int pgnfields = PQnfields(curs->pgres);
    int pgbintuples = PQbinaryTuples(curs->pgres);

    curs->notuples = 0;

    /* create the tuple for description and typecasting */
    Py_XDECREF(curs->description);
    Py_XDECREF(curs->casts);
    curs->description = PyTuple_New(pgnfields);
    curs->casts = PyTuple_New(pgnfields);
    curs->columns = pgnfields;
    
    /* calculate the display size for each column (cpu intensive, can be
       switched off at configuration time) */
#ifdef PSYCOPG_DISPLAY_SIZE
    dsize = (int *)PyMem_Malloc(pgnfields * sizeof(int));
    if (dsize != NULL) {
        int j, len;
        for (i=0; i < pgnfields; i++) {
            dsize[i] = -1;
        }
        for (j = 0; j < curs->rowcount; j++) {
            for (i = 0; i < pgnfields; i++) {
                len = PQgetlength(curs->pgres, j, i);
                if (len > dsize[i]) dsize[i] = len;
            }
        }
    }
#endif

    /* calculate various parameters and typecasters */
    for (i = 0; i < pgnfields; i++) {
        Oid ftype = PQftype(curs->pgres, i);
        int fsize = PQfsize(curs->pgres, i);
        int fmod =  PQfmod(curs->pgres, i);
        
        PyObject *dtitem = PyTuple_New(7);
        PyObject *type = PyInt_FromLong(ftype);
        PyObject *cast = NULL;
        
        PyTuple_SET_ITEM(curs->description, i, dtitem);
        
        /* fill the right cast function by accessing the global dictionary of
           casting objects.  if we got no defined cast use the default
           one */
        if (!(cast = PyDict_GetItem(curs->casts, type))) {
            Dprintf("_pq_fetch_tuples: cast %d not in per-cursor dict", ftype);
            if (!(cast = PyDict_GetItem(psyco_types, type))) {
                Dprintf("_pq_fetch_tuples: cast %d not found, using default",
                        PQftype(curs->pgres,i));
                cast = psyco_default_cast;
            }
        }
        /* else if we got binary tuples and if we got a field that
           is binary use the default cast
           FIXME: what the hell am I trying to do here? This just can't work..
        */
        else if (pgbintuples && cast == psyco_default_binary_cast) {
            Dprintf("_pq_fetch_tuples: Binary cursor and "
                    "binary field: %i using default cast",
                    PQftype(curs->pgres,i));
            cast = psyco_default_cast;
        }
        Dprintf("_pq_fetch_tuples: using cast at %p (%s) for type %d",
                cast, PyString_AS_STRING(((typecastObject*)cast)->name),
                PQftype(curs->pgres,i));
        Py_INCREF(cast);
        PyTuple_SET_ITEM(curs->casts, i, cast);
    

        /* 1/ fill the other fields */
        PyTuple_SET_ITEM(dtitem, 0,
                         PyString_FromString(PQfname(curs->pgres, i)));
        PyTuple_SET_ITEM(dtitem, 1, type);

        /* 2/ display size is the maximum size of this field result tuples. */
        if (dsize && dsize[i] >= 0) {
            PyTuple_SET_ITEM(dtitem, 2, PyInt_FromLong(dsize[i]));
        }
        else {
            Py_INCREF(Py_None);
            PyTuple_SET_ITEM(dtitem, 2, Py_None);
        }

        /* 3/ size on the backend */
        if (fmod > 0) fmod = fmod - sizeof(int);
        if (fsize == -1) {
            if (ftype == NUMERICOID) {
                PyTuple_SET_ITEM(dtitem, 3,
                                 PyInt_FromLong((fmod >> 16) & 0xFFFF));
            }
            else { /* If variable length record, return maximum size */
                PyTuple_SET_ITEM(dtitem, 3, PyInt_FromLong(fmod));
            }
        }
        else {
            PyTuple_SET_ITEM(dtitem, 3, PyInt_FromLong(fsize));
        }

        /* 4,5/ scale and precision */
        if (ftype == NUMERICOID) {
            PyTuple_SET_ITEM(dtitem, 4, PyInt_FromLong((fmod >> 16) & 0xFFFF));
            PyTuple_SET_ITEM(dtitem, 5, PyInt_FromLong((fmod & 0xFFFF) - 4));
        }
        else {
            Py_INCREF(Py_None);
            PyTuple_SET_ITEM(dtitem, 4, Py_None);
            Py_INCREF(Py_None);
            PyTuple_SET_ITEM(dtitem, 5, Py_None);
        }

        /* 6/ FIXME: null_ok??? */
        Py_INCREF(Py_None);
        PyTuple_SET_ITEM(dtitem, 6, Py_None);
    }
    
    if (dsize) PyMem_Free(dsize);
}

#ifdef HAVE_PQPROTOCOL3
static int
_pq_copy_in_v3(cursorObject *curs)
{
    /* COPY FROM implementation when protocol 3 is available: this function
       uses the new PQputCopyData() and can detect errors and set the correct
       exception */
    PyObject *o;
    int length = 0, error = 0;
    
    while (1) {
        o = PyObject_CallMethod(curs->copyfile, "read", "i", curs->copysize);
        if (!o || !PyString_Check(o) || (length = PyString_Size(o)) == -1) {
            error = 1;
        }
        if (length == 0 || error == 1) break;
        
        Py_BEGIN_ALLOW_THREADS;
        if (PQputCopyData(curs->conn->pgconn,
                          PyString_AS_STRING(o), length) == -1) {
            error = 2;
        }
        Py_END_ALLOW_THREADS;

        if (error == 2) break;
        
        Py_DECREF(o);
    }
    
    Py_XDECREF(o);
    
    if (error == 0 || error == 2)
        /* 0 means that the copy went well, 2 that there was an error on the
           backend: in both cases we'll get the error message from the
           PQresult */
        PQputCopyEnd(curs->conn->pgconn, NULL);
    else
        PQputCopyEnd(curs->conn->pgconn, "error during .read() call");

    /* and finally we grab the operation result from the backend */
    IFCLEARPGRES(curs->pgres);
    while ((curs->pgres = PQgetResult(curs->conn->pgconn)) != NULL) {
        if (PQresultStatus(curs->pgres) == PGRES_FATAL_ERROR)
            pq_raise(curs->conn, curs, NULL, NULL);
        IFCLEARPGRES(curs->pgres);
    }

    return 1;
}
#endif
static int
_pq_copy_in(cursorObject *curs)
{
    /* COPY FROM implementation when protocol 3 is not available: this
       function can't fail but the backend will send an ERROR notice that will
       be catched by our notice collector */
    PyObject *o;

    while (1) {
        o = PyObject_CallMethod(curs->copyfile, "readline", NULL);
        if (!o || o == Py_None || PyString_GET_SIZE(o) == 0) break;
        if (PQputline(curs->conn->pgconn, PyString_AS_STRING(o)) != 0) {
            Py_DECREF(o);
            return -1;
        }
        Py_DECREF(o);
    }
    Py_XDECREF(o);
    PQputline(curs->conn->pgconn, "\\.\n");
    PQendcopy(curs->conn->pgconn);

    /* if for some reason we're using a protocol 3 libpq to connect to a
       protocol 2 backend we still need to cycle on the result set */
    IFCLEARPGRES(curs->pgres);
    while ((curs->pgres = PQgetResult(curs->conn->pgconn)) != NULL) {
        if (PQresultStatus(curs->pgres) == PGRES_FATAL_ERROR)
            pq_raise(curs->conn, curs, NULL, NULL);
        IFCLEARPGRES(curs->pgres);
    }

    return 1;
}

#ifdef HAVE_PQPROTOCOL3
static int
_pq_copy_out_v3(cursorObject *curs)
{
    char *buffer;
    int len;
    
    while (1) {
        Py_BEGIN_ALLOW_THREADS;
        len = PQgetCopyData(curs->conn->pgconn, &buffer, 0);
        Py_END_ALLOW_THREADS;
            
        if (len > 0 && buffer) {
            PyObject_CallMethod(curs->copyfile, "write", "s#", buffer, len);
            PQfreemem(buffer);
        }
        /* we break on len == 0 but note that that should *not* happen,
           because we are not doing an async call (if it happens blame
           postgresql authors :/) */
        else if (len <= 0) break;
    }
    
    if (len == -2) {
        pq_raise(curs->conn, NULL, NULL, NULL);
        return -1;
    }

    /* and finally we grab the operation result from the backend */
    IFCLEARPGRES(curs->pgres);
    while ((curs->pgres = PQgetResult(curs->conn->pgconn)) != NULL) {
        if (PQresultStatus(curs->pgres) == PGRES_FATAL_ERROR)
            pq_raise(curs->conn, curs, NULL, NULL);
        IFCLEARPGRES(curs->pgres);
    }
    return 1;
}
#endif

static int
_pq_copy_out(cursorObject *curs)
{
    char buffer[4096];
    int status, len;

    while (1) {
        Py_BEGIN_ALLOW_THREADS;
        status = PQgetline(curs->conn->pgconn, buffer, 4096);
        Py_END_ALLOW_THREADS;
        if (status == 0) {
            if (buffer[0] == '\\' && buffer[1] == '.') break;

            len = strlen(buffer);
            buffer[len++] = '\n';
        }
        else if (status == 1) {
            len = 4096-1;
        }
        else {
            return -1;
        }
        
        PyObject_CallMethod(curs->copyfile, "write", "s#", buffer, len);
    }

    status = 1;
    if (PQendcopy(curs->conn->pgconn) != 0)
        status = -1;
    
    /* if for some reason we're using a protocol 3 libpq to connect to a
       protocol 2 backend we still need to cycle on the result set */
    IFCLEARPGRES(curs->pgres);
    while ((curs->pgres = PQgetResult(curs->conn->pgconn)) != NULL) {
        if (PQresultStatus(curs->pgres) == PGRES_FATAL_ERROR)
            pq_raise(curs->conn, curs, NULL, NULL);
        IFCLEARPGRES(curs->pgres);
    }

    return status;
}

int
pq_fetch(cursorObject *curs)
{
    int pgstatus, ex = -1;

    /* even if we fail, we remove any information about the previous query */
    curs_reset(curs);
    
    /* we check the result from the previous execute; if the result is not
       already there, we need to consume some input and go to sleep until we
       get something edible to eat */
    if (!curs->pgres) {
        
        Dprintf("pq_fetch: no data: entering polling loop");
        
        while (pq_is_busy(curs->conn) > 0) {
            fd_set rfds;
            struct timeval tv;
            int sval, sock;

            Py_BEGIN_ALLOW_THREADS;
            pthread_mutex_lock(&(curs->conn->lock));

            sock = PQsocket(curs->conn->pgconn);
            FD_ZERO(&rfds);
            FD_SET(sock, &rfds);

            /* set a default timeout of 5 seconds
               TODO: make use of the timeout, maybe allowing the user to
               make a non-blocking (timeouted) call to fetchXXX */
            tv.tv_sec = 5;
            tv.tv_usec = 0;

            Dprintf("pq_fetch: entering PDflush() loop");
            while (PQflush(curs->conn->pgconn) != 0);
            sval = select(sock+1, &rfds, NULL, NULL, &tv);

            pthread_mutex_unlock(&(curs->conn->lock));
            Py_END_ALLOW_THREADS;
        }

        Dprintf("pq_fetch: data is probably ready");
        IFCLEARPGRES(curs->pgres);
        curs->pgres = PQgetResult(curs->conn->pgconn);
    }

    /* check for PGRES_FATAL_ERROR result */
    /* FIXME: I am not sure we need to check for critical error here.
    if (curs->pgres == NULL) {
        Dprintf("pq_fetch: got a NULL pgres, checking for critical");
        pq_set_critical(curs->conn);
        if (curs->conn->critical) {
            pq_resolve_critical(curs->conn);
            return -1;
        }
        else {
            return 0;
        }
    }
    */
    
    if (curs->pgres == NULL) return 0;
    
    pgstatus = PQresultStatus(curs->pgres);
    Dprintf("pq_fetch: pgstatus = %s", PQresStatus(pgstatus));

    /* backend status message */
    Py_XDECREF(curs->pgstatus);
    curs->pgstatus = PyString_FromString(PQcmdStatus(curs->pgres));

    switch(pgstatus) {

    case PGRES_COMMAND_OK:
        Dprintf("pq_fetch: command returned OK (no tuples)");
        curs->rowcount = atoi(PQcmdTuples(curs->pgres));
        curs->lastoid = PQoidValue(curs->pgres);
        CLEARPGRES(curs->pgres);
        ex = 1;
        break;

    case PGRES_COPY_OUT:
        Dprintf("pq_fetch: data from a COPY TO (no tuples)");
#ifdef HAVE_PQPROTOCOL3
        if (curs->conn->protocol == 3)
            ex = _pq_copy_out_v3(curs);
        else
#endif
            ex = _pq_copy_out(curs);
        curs->rowcount = -1;
        /* error caught by out glorious notice handler */
        if (PyErr_Occurred()) ex = -1;
        IFCLEARPGRES(curs->pgres);
        break;
        
    case PGRES_COPY_IN:
        Dprintf("pq_fetch: data from a COPY FROM (no tuples)");
#ifdef HAVE_PQPROTOCOL3
        if (curs->conn->protocol == 3)        
            ex = _pq_copy_in_v3(curs);
        else
#endif
            ex = _pq_copy_in(curs);
        curs->rowcount = -1;
        /* error caught by out glorious notice handler */
        if (PyErr_Occurred()) ex = -1;
        IFCLEARPGRES(curs->pgres);
        break;
        
    case PGRES_TUPLES_OK:
        Dprintf("pq_fetch: data from a SELECT (got tuples)");
        curs->rowcount = PQntuples(curs->pgres);
        _pq_fetch_tuples(curs); ex = 0;
        /* don't clear curs->pgres, because it contains the results! */
        break;
        
    default:
        Dprintf("pq_fetch: uh-oh, something FAILED");
        pq_raise(curs->conn, curs, NULL, NULL);
        IFCLEARPGRES(curs->pgres);
        ex = -1;
        break;
    }

    Dprintf("pq_fetch: fetching done; check for critical errors");
    
    /* error checking, close the connection if necessary (some critical errors
       are not really critical, like a COPY FROM error: if that's the case we
       raise the exception but we avoid to close the connection) */
    if (curs->conn->critical) {
        if (ex == -1) {
            pq_resolve_critical(curs->conn, 1);
        }
        else {
            pq_resolve_critical(curs->conn, 0);
        }
        return -1;
    }
    
    return ex;
}
