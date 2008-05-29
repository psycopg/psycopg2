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

#define PY_SSIZE_T_CLEAN
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


/* Strip off the severity from a Postgres error message. */
static const char *
strip_severity(const char *msg)
{
    if (!msg)
        return NULL;

    if (strlen(msg) > 8 && (!strncmp(msg, "ERROR:  ", 8) ||
                            !strncmp(msg, "FATAL:  ", 8) ||
                            !strncmp(msg, "PANIC:  ", 8)))
        return &msg[8];
    else
        return msg;
}

/* Returns the Python exception corresponding to an SQLSTATE error
   code.  A list of error codes can be found at:

   http://www.postgresql.org/docs/current/static/errcodes-appendix.html */
static PyObject *
exception_from_sqlstate(const char *sqlstate)
{
    switch (sqlstate[0]) {
    case '0':
        switch (sqlstate[1]) {
        case 'A': /* Class 0A - Feature Not Supported */
            return NotSupportedError;
        }
        break;
    case '2':
        switch (sqlstate[1]) {
        case '1': /* Class 21 - Cardinality Violation */
            return ProgrammingError;
        case '2': /* Class 22 - Data Exception */
            return DataError;
        case '3': /* Class 23 - Integrity Constraint Violation */
            return IntegrityError;
        case '4': /* Class 24 - Invalid Cursor State */
        case '5': /* Class 25 - Invalid Transaction State */
            return InternalError;
        case '6': /* Class 26 - Invalid SQL Statement Name */
        case '7': /* Class 27 - Triggered Data Change Violation */
        case '8': /* Class 28 - Invalid Authorization Specification */
            return OperationalError;
        case 'B': /* Class 2B - Dependent Privilege Descriptors Still Exist */
        case 'D': /* Class 2D - Invalid Transaction Termination */
        case 'F': /* Class 2F - SQL Routine Exception */
            return InternalError;
        }
        break;
    case '3':
        switch (sqlstate[1]) {
        case '4': /* Class 34 - Invalid Cursor Name */
            return OperationalError;
        case '8': /* Class 38 - External Routine Exception */
        case '9': /* Class 39 - External Routine Invocation Exception */
        case 'B': /* Class 3B - Savepoint Exception */
            return InternalError;
        case 'D': /* Class 3D - Invalid Catalog Name */
        case 'F': /* Class 3F - Invalid Schema Name */
            return ProgrammingError;
        }
        break;
    case '4':
        switch (sqlstate[1]) {
        case '0': /* Class 40 - Transaction Rollback */
#ifdef PSYCOPG_EXTENSIONS
            return TransactionRollbackError;
#else
            return OperationalError;
#endif
        case '2': /* Class 42 - Syntax Error or Access Rule Violation */
        case '4': /* Class 44 - WITH CHECK OPTION Violation */
            return ProgrammingError;
        }
        break;
    case '5':
        /* Class 53 - Insufficient Resources
           Class 54 - Program Limit Exceeded
           Class 55 - Object Not In Prerequisite State
           Class 57 - Operator Intervention
           Class 58 - System Error (errors external to PostgreSQL itself) */
#ifdef PSYCOPG_EXTENSIONS
        if (!strcmp(sqlstate, "57014"))
            return QueryCanceledError;
        else
#endif
            return OperationalError;
    case 'F': /* Class F0 - Configuration File Error */
        return InternalError;
    case 'P': /* Class P0 - PL/pgSQL Error */
        return InternalError;
    case 'X': /* Class XX - Internal Error */
        return InternalError;
    }
    /* return DatabaseError as a fallback */
    return DatabaseError;
}

/* pq_raise - raise a python exception of the right kind

   This function should be called while holding the GIL. */

static void
pq_raise(connectionObject *conn, cursorObject *curs, PGresult *pgres)
{
    PyObject *pgc = (PyObject*)curs;
    PyObject *exc = NULL;
    const char *err = NULL;
    const char *err2 = NULL;
    const char *code = NULL;

    if (conn == NULL) {
        PyErr_SetString(Error, "psycopg went psycotic and raised a null error");
        return;
    }
    
    /* if the connection has somehow beed broken, we mark the connection
       object as closed but requiring cleanup */
    if (conn->pgconn != NULL && PQstatus(conn->pgconn) == CONNECTION_BAD)
        conn->closed = 2;

    if (pgres == NULL && curs != NULL)
        pgres = curs->pgres;

    if (pgres) {
        err = PQresultErrorMessage(pgres);
#ifdef HAVE_PQPROTOCOL3
        if (err != NULL && conn->protocol == 3) {
            code = PQresultErrorField(pgres, PG_DIAG_SQLSTATE);
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

    /* Analyze the message and try to deduce the right exception kind
       (only if we got the SQLSTATE from the pgres, obviously) */
    if (code != NULL) {
        exc = exception_from_sqlstate(code);
    }

    /* if exc is still NULL psycopg was not built with HAVE_PQPROTOCOL3 or the
       connection is using protocol 2: in both cases we default to comparing
       error messages */
    if (exc == NULL) {
        if (!strncmp(err, "ERROR:  Cannot insert a duplicate key", 37)
            || !strncmp(err, "ERROR:  ExecAppend: Fail to add null", 36)
            || strstr(err, "referential integrity violation"))
            exc = IntegrityError;
        else if (strstr(err, "could not serialize") ||
                 strstr(err, "deadlock detected"))
#ifdef PSYCOPG_EXTENSIONS
            exc = TransactionRollbackError;
#else
            exc = OperationalError;
#endif
        else
            exc = ProgrammingError;
    }

    /* try to remove the initial "ERROR: " part from the postgresql error */
    err2 = strip_severity(err);

    psyco_set_error(exc, pgc, err2, err, code);
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
    Dprintf("pq_set_critical: setting %s", msg);
    if (msg && msg[0] != '\0') conn->critical = strdup(msg);
    else conn->critical = NULL;
}

static void
pq_clear_critical(connectionObject *conn)
{
    /* sometimes we know that the notice analizer set a critical that
       was not really as such (like when raising an error for a delayed
       contraint violation. it would be better to analyze the notice
       or avoid the set-error-on-notice stuff at all but given that we
       can't, some functions at least clear the critical status after
       operations they know would result in a wrong critical to be set */
    Dprintf("pq_clear_critical: clearing %s", conn->critical);
    if (conn->critical) {
        free(conn->critical);
        conn->critical = NULL;
    }
}

static PyObject *
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
    
        /* remember to clear the critical! */
        pq_clear_critical(conn);    
    }
    return NULL;
}

/* pq_clear_async - clear the effects of a previous async query

   note that this function does block because it needs to wait for the full
   result sets of the previous query to clear them.


   this function does not call any Py_*_ALLOW_THREADS macros */

static void
pq_clear_async(connectionObject *conn)
{
    PGresult *pgres;

    do {
        pgres = PQgetResult(conn->pgconn);
        Dprintf("pq_clear_async: clearing PGresult at %p", pgres);
        IFCLEARPGRES(pgres);
    } while (pgres != NULL);
}

/* pg_execute_command_locked - execute a no-result query on a locked connection.

   This function should only be called on a locked connection without
   holding the global interpreter lock.

   On error, -1 is returned, and the pgres argument will hold the
   relevant result structure.
 */
int
pq_execute_command_locked(connectionObject *conn, const char *query,
                          PGresult **pgres, char **error)
{
    int pgstatus, retvalue = -1;

    Dprintf("pq_execute_command_locked: pgconn = %p, query = %s",
            conn->pgconn, query);
    *error = NULL;
    *pgres = PQexec(conn->pgconn, query);
    if (*pgres == NULL) {
        const char *msg;

        Dprintf("pq_execute_command_locked: PQexec returned NULL");
        msg = PQerrorMessage(conn->pgconn);
        if (msg)
            *error = strdup(msg);
        goto cleanup;
    }

    pgstatus = PQresultStatus(*pgres);
    if (pgstatus != PGRES_COMMAND_OK ) {
        Dprintf("pq_execute_command_locked: result was not COMMAND_OK (%d)",
                pgstatus);
        goto cleanup;
    }

    retvalue = 0;
    IFCLEARPGRES(*pgres);

 cleanup:
    return retvalue;
}

/* pq_complete_error: handle an error from pq_execute_command_locked()

   If pq_execute_command_locked() returns -1, this function should be
   called to convert the result to a Python exception.

   This function should be called while holding the global interpreter
   lock.
 */
void
pq_complete_error(connectionObject *conn, PGresult **pgres, char **error)
{
    Dprintf("pq_complete_error: pgconn = %p, pgres = %p, error = %s",
            conn->pgconn, *pgres, *error ? *error : "(null)");
    if (*pgres != NULL)
        pq_raise(conn, NULL, *pgres);
    else if (*error != NULL) {
        PyErr_SetString(OperationalError, *error);
    } else {
        PyErr_SetString(OperationalError, "unknown error");
    }
    IFCLEARPGRES(*pgres);
    if (*error) {
        free(*error);
        *error = NULL;
    }
}


/* pq_begin_locked - begin a transaction, if necessary

   This function should only be called on a locked connection without
   holding the global interpreter lock.

   On error, -1 is returned, and the pgres argument will hold the
   relevant result structure.
 */
int
pq_begin_locked(connectionObject *conn, PGresult **pgres, char **error)
{
    const char *query[] = {
        NULL,
        "BEGIN; SET TRANSACTION ISOLATION LEVEL READ COMMITTED",
        "BEGIN; SET TRANSACTION ISOLATION LEVEL SERIALIZABLE"};
    int result;

    Dprintf("pq_begin_locked: pgconn = %p, isolevel = %ld, status = %d",
            conn->pgconn, conn->isolation_level, conn->status);

    if (conn->isolation_level == 0 || conn->status != CONN_STATUS_READY) {
        Dprintf("pq_begin_locked: transaction in progress");
        return 0;
    }

    pq_clear_async(conn);
    result = pq_execute_command_locked(conn, query[conn->isolation_level],
                                       pgres, error);
    if (result == 0)
        conn->status = CONN_STATUS_BEGIN;

    return result;
}

/* pq_commit - send an END, if necessary

   This function should be called while holding the global interpreter
   lock. */

int
pq_commit(connectionObject *conn)
{
    int retvalue = -1;
    PGresult *pgres = NULL;
    char *error = NULL;

    Dprintf("pq_commit: pgconn = %p, isolevel = %ld, status = %d",
            conn->pgconn, conn->isolation_level, conn->status);

    if (conn->isolation_level == 0 || conn->status != CONN_STATUS_BEGIN) {
        Dprintf("pq_commit: no transaction to commit");
        return 0;
    }

    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&conn->lock);
    conn->mark += 1;

    pq_clear_async(conn);
    retvalue = pq_execute_command_locked(conn, "COMMIT", &pgres, &error);

    pthread_mutex_unlock(&conn->lock);
    Py_END_ALLOW_THREADS;

    if (retvalue < 0)
        pq_complete_error(conn, &pgres, &error);

    /* Even if an error occurred, the connection will be rolled back,
       so we unconditionally set the connection status here. */
    conn->status = CONN_STATUS_READY;

    return retvalue;
}

int
pq_abort_locked(connectionObject *conn, PGresult **pgres, char **error)
{
    int retvalue = -1;

    Dprintf("pq_abort_locked: pgconn = %p, isolevel = %ld, status = %d",
            conn->pgconn, conn->isolation_level, conn->status);

    if (conn->isolation_level == 0 || conn->status != CONN_STATUS_BEGIN) {
        Dprintf("pq_abort_locked: no transaction to abort");
        return 0;
    }

    conn->mark += 1;
    pq_clear_async(conn);
    retvalue = pq_execute_command_locked(conn, "ROLLBACK", pgres, error);
    if (retvalue == 0)
        conn->status = CONN_STATUS_READY;

    return retvalue;
}

/* pq_abort - send an ABORT, if necessary

   This function should be called while holding the global interpreter
   lock. */

int
pq_abort(connectionObject *conn)
{
    int retvalue = -1;
    PGresult *pgres = NULL;
    char *error = NULL;

    Dprintf("pq_abort: pgconn = %p, isolevel = %ld, status = %d",
            conn->pgconn, conn->isolation_level, conn->status);

    if (conn->isolation_level == 0 || conn->status != CONN_STATUS_BEGIN) {
        Dprintf("pq_abort: no transaction to abort");
        return 0;
    }

    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&conn->lock);

    retvalue = pq_abort_locked(conn, &pgres, &error);

    pthread_mutex_unlock(&conn->lock);
    Py_END_ALLOW_THREADS;

    if (retvalue < 0)
        pq_complete_error(conn, &pgres, &error);

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
    int res;
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


    /* now check for notifies */
    while ((pgn = PQnotifies(conn->pgconn)) != NULL) {
        PyObject *notify;

        Dprintf("curs_is_busy: got NOTIFY from pid %d, msg = %s",
                (int) pgn->be_pid, pgn->relname);

        Py_BLOCK_THREADS;
        notify = PyTuple_New(2);
        PyTuple_SET_ITEM(notify, 0, PyInt_FromLong((long)pgn->be_pid));
        PyTuple_SET_ITEM(notify, 1, PyString_FromString(pgn->relname));
        PyList_Append(conn->notifies, notify);
        Py_UNBLOCK_THREADS;
        free(pgn);
    }

    res = PQisBusy(conn->pgconn);
    
    pthread_mutex_unlock(&(conn->lock));
    Py_END_ALLOW_THREADS;

    return res;
}

/* pq_execute - execute a query, possibly asyncronously

   this fucntion locks the connection object
   this function call Py_*_ALLOW_THREADS macros */

int
pq_execute(cursorObject *curs, const char *query, int async)
{
    PGresult *pgres = NULL;
    char *error = NULL;

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

    if (pq_begin_locked(curs->conn, &pgres, &error) < 0) {
        pthread_mutex_unlock(&(curs->conn->lock));
        Py_BLOCK_THREADS;
        pq_complete_error(curs->conn, &pgres, &error);
        return -1;
    }

    if (async == 0) {
        IFCLEARPGRES(curs->pgres);
        Dprintf("pq_execute: executing SYNC query:");
        Dprintf("    %-.200s", query);
        curs->pgres = PQexec(curs->conn->pgconn, query);

        /* dont let pgres = NULL go to pq_fetch() */
        if (curs->pgres == NULL) {
            pthread_mutex_unlock(&(curs->conn->lock));
            Py_BLOCK_THREADS;
            PyErr_SetString(OperationalError,
                            PQerrorMessage(curs->conn->pgconn));
            return -1;
        }
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
    int pgnfields;
    int pgbintuples;

    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&(curs->conn->lock));
    
    pgnfields = PQnfields(curs->pgres);
    pgbintuples = PQbinaryTuples(curs->pgres);

    curs->notuples = 0;

    /* create the tuple for description and typecasting */
    Py_BLOCK_THREADS;
    Py_XDECREF(curs->description);
    Py_XDECREF(curs->casts);    
    curs->description = PyTuple_New(pgnfields);
    curs->casts = PyTuple_New(pgnfields);
    curs->columns = pgnfields;
    Py_UNBLOCK_THREADS;

    /* calculate the display size for each column (cpu intensive, can be
       switched off at configuration time) */
#ifdef PSYCOPG_DISPLAY_SIZE
    Py_BLOCK_THREADS;
    dsize = (int *)PyMem_Malloc(pgnfields * sizeof(int));
    Py_UNBLOCK_THREADS;
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

        PyObject *dtitem;
        PyObject *type;
        PyObject *cast = NULL;

        Py_BLOCK_THREADS;

        dtitem = PyTuple_New(7);
        PyTuple_SET_ITEM(curs->description, i, dtitem);

        /* fill the right cast function by accessing three different dictionaries:
           - the per-cursor dictionary, if available (can be NULL or None)
           - the per-connection dictionary (always exists but can be null)
           - the global dictionary (at module level)
           if we get no defined cast use the default one */

        type = PyInt_FromLong(ftype);
        Dprintf("_pq_fetch_tuples: looking for cast %d:", ftype);
        if (curs->string_types != NULL && curs->string_types != Py_None) {
            cast = PyDict_GetItem(curs->string_types, type);
            Dprintf("_pq_fetch_tuples:     per-cursor dict: %p", cast);
        }
        if (cast == NULL) {
            cast = PyDict_GetItem(curs->conn->string_types, type);
            Dprintf("_pq_fetch_tuples:     per-connection dict: %p", cast);
        }
        if (cast == NULL) {
            cast = PyDict_GetItem(psyco_types, type);
            Dprintf("_pq_fetch_tuples:     global dict: %p", cast);
        }
        if (cast == NULL) cast = psyco_default_cast;

        /* else if we got binary tuples and if we got a field that
           is binary use the default cast
           FIXME: what the hell am I trying to do here? This just can't work..
        */
        if (pgbintuples && cast == psyco_default_binary_cast) {
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
            PyTuple_SET_ITEM(dtitem, 5, PyInt_FromLong(fmod & 0xFFFF));
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
    
        Py_UNBLOCK_THREADS;    
    }

    if (dsize) {
        Py_BLOCK_THREADS;
        PyMem_Free(dsize);
        Py_UNBLOCK_THREADS;
   }
   
    pthread_mutex_unlock(&(curs->conn->lock));
    Py_END_ALLOW_THREADS;
}

#ifdef HAVE_PQPROTOCOL3
static int
_pq_copy_in_v3(cursorObject *curs)
{
    /* COPY FROM implementation when protocol 3 is available: this function
       uses the new PQputCopyData() and can detect errors and set the correct
       exception */
    PyObject *o;
    Py_ssize_t length = 0;
    int res, error = 0;

    while (1) {
        o = PyObject_CallMethod(curs->copyfile, "read",
            CONV_CODE_PY_SSIZE_T, curs->copysize);
        if (!o || !PyString_Check(o) || (length = PyString_Size(o)) == -1) {
            error = 1;
        }
        if (length == 0 || length > INT_MAX || error == 1) break;

        Py_BEGIN_ALLOW_THREADS;
        res = PQputCopyData(curs->conn->pgconn, PyString_AS_STRING(o),
            /* Py_ssize_t->int cast was validated above */
            (int) length);
        Dprintf("_pq_copy_in_v3: sent %d bytes of data; res = %d",
            (int) length, res);
            
        if (res == 0) {
            /* FIXME: in theory this should not happen but adding a check
               here would be a nice idea */
        }
        else if (res == -1) {
            Dprintf("_pq_copy_in_v3: PQerrorMessage = %s",
                PQerrorMessage(curs->conn->pgconn));
            error = 2;
        }
        Py_END_ALLOW_THREADS;

        if (error == 2) break;

        Py_DECREF(o);
    }

    Py_XDECREF(o);

    Dprintf("_pq_copy_in_v3: error = %d", error);

    /* 0 means that the copy went well, 2 that there was an error on the
       backend: in both cases we'll get the error message from the PQresult */
    if (error == 0)
        res = PQputCopyEnd(curs->conn->pgconn, NULL);
    else if (error == 2)
        res = PQputCopyEnd(curs->conn->pgconn, "error in PQputCopyData() call");
    else
        res = PQputCopyEnd(curs->conn->pgconn, "error in .read() call");

    IFCLEARPGRES(curs->pgres);
    
    Dprintf("_pq_copy_in_v3: copy ended; res = %d", res);
    
    /* if the result is -1 we should not even try to get a result from the
       bacause that will lock the current thread forever */
    if (res == -1) {
        pq_raise(curs->conn, curs, NULL);
        /* FIXME: pq_raise check the connection but for some reason even
           if the error message says "server closed the connection unexpectedly"
           the status returned by PQstatus is CONNECTION_OK! */
        curs->conn->closed = 2;
    }
    else {
        /* and finally we grab the operation result from the backend */
        while ((curs->pgres = PQgetResult(curs->conn->pgconn)) != NULL) {
            if (PQresultStatus(curs->pgres) == PGRES_FATAL_ERROR)
                pq_raise(curs->conn, curs, NULL);
            IFCLEARPGRES(curs->pgres);
        }
    }
   
    return error == 0 ? 1 : -1;
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
        if (o == NULL) return -1;
        if (o == Py_None || PyString_GET_SIZE(o) == 0) break;
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
            pq_raise(curs->conn, curs, NULL);
        IFCLEARPGRES(curs->pgres);
    }

    return 1;
}

#ifdef HAVE_PQPROTOCOL3
static int
_pq_copy_out_v3(cursorObject *curs)
{
    PyObject *tmp = NULL;

    char *buffer;
    Py_ssize_t len;

    while (1) {
        Py_BEGIN_ALLOW_THREADS;
        len = PQgetCopyData(curs->conn->pgconn, &buffer, 0);
        Py_END_ALLOW_THREADS;

        if (len > 0 && buffer) {
            tmp = PyObject_CallMethod(curs->copyfile,
                            "write", "s#", buffer, len);
            PQfreemem(buffer);
            if (tmp == NULL)
                return -1;
            else
                Py_DECREF(tmp);
        }
        /* we break on len == 0 but note that that should *not* happen,
           because we are not doing an async call (if it happens blame
           postgresql authors :/) */
        else if (len <= 0) break;
    }

    if (len == -2) {
        pq_raise(curs->conn, curs, NULL);
        return -1;
    }

    /* and finally we grab the operation result from the backend */
    IFCLEARPGRES(curs->pgres);
    while ((curs->pgres = PQgetResult(curs->conn->pgconn)) != NULL) {
        if (PQresultStatus(curs->pgres) == PGRES_FATAL_ERROR)
            pq_raise(curs->conn, curs, NULL);
        IFCLEARPGRES(curs->pgres);
    }
    return 1;
}
#endif

static int
_pq_copy_out(cursorObject *curs)
{
    PyObject *tmp = NULL;

    char buffer[4096];
    int status, ll=0;
    Py_ssize_t len;

    while (1) {
        Py_BEGIN_ALLOW_THREADS;
        status = PQgetline(curs->conn->pgconn, buffer, 4096);
        Py_END_ALLOW_THREADS;
        if (status == 0) {
            if (!ll && buffer[0] == '\\' && buffer[1] == '.') break;

            len = (Py_ssize_t) strlen(buffer);
            buffer[len++] = '\n';
            ll = 0;
        }
        else if (status == 1) {
            len = 4096-1;
            ll = 1;
        }
        else {
            return -1;
        }

        tmp = PyObject_CallMethod(curs->copyfile, "write", "s#", buffer, len);
        if (tmp == NULL)
            return -1;
        else
            Py_DECREF(tmp);
    }

    status = 1;
    if (PQendcopy(curs->conn->pgconn) != 0)
        status = -1;

    /* if for some reason we're using a protocol 3 libpq to connect to a
       protocol 2 backend we still need to cycle on the result set */
    IFCLEARPGRES(curs->pgres);
    while ((curs->pgres = PQgetResult(curs->conn->pgconn)) != NULL) {
        if (PQresultStatus(curs->pgres) == PGRES_FATAL_ERROR)
            pq_raise(curs->conn, curs, NULL);
        IFCLEARPGRES(curs->pgres);
    }

    return status;
}

int
pq_fetch(cursorObject *curs)
{
    int pgstatus, ex = -1;
    const char *rowcount;

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

        Py_BEGIN_ALLOW_THREADS;
        pthread_mutex_lock(&(curs->conn->lock));

        Dprintf("pq_fetch: data is probably ready");
        IFCLEARPGRES(curs->pgres);
        curs->pgres = PQgetResult(curs->conn->pgconn);

        pthread_mutex_unlock(&(curs->conn->lock));
        Py_END_ALLOW_THREADS;
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
        rowcount = PQcmdTuples(curs->pgres);
        if (!rowcount || !rowcount[0])
          curs->rowcount = -1;
        else
          curs->rowcount = atoi(rowcount);
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
        pq_raise(curs->conn, curs, NULL);
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
