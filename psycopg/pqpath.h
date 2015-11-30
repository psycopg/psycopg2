/* pqpath.h - definitions for pqpath.c
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

#ifndef PSYCOPG_PQPATH_H
#define PSYCOPG_PQPATH_H 1

#include "psycopg/cursor.h"
#include "psycopg/connection.h"

/* macro to clean the pg result */
#define CLEARPGRES(pgres)   do { PQclear(pgres); pgres = NULL; } while (0)

/* exported functions */
HIDDEN PGresult *pq_get_last_result(connectionObject *conn);
RAISES_NEG HIDDEN int pq_fetch(cursorObject *curs, int no_result);
RAISES_NEG HIDDEN int pq_execute(cursorObject *curs, const char *query,
                                 int async, int no_result, int no_begin);
HIDDEN int pq_send_query(connectionObject *conn, const char *query);
HIDDEN int pq_begin_locked(connectionObject *conn, PGresult **pgres,
                           char **error, PyThreadState **tstate);
HIDDEN int pq_commit(connectionObject *conn);
RAISES_NEG HIDDEN int pq_abort_locked(connectionObject *conn, PGresult **pgres,
                           char **error, PyThreadState **tstate);
RAISES_NEG HIDDEN int pq_abort(connectionObject *conn);
HIDDEN int pq_reset_locked(connectionObject *conn, PGresult **pgres,
                            char **error, PyThreadState **tstate);
RAISES_NEG HIDDEN int pq_reset(connectionObject *conn);
HIDDEN char *pq_get_guc_locked(connectionObject *conn, const char *param,
                               PGresult **pgres,
                               char **error, PyThreadState **tstate);
HIDDEN int pq_set_guc_locked(connectionObject *conn, const char *param,
                             const char *value, PGresult **pgres,
                             char **error, PyThreadState **tstate);
HIDDEN int pq_tpc_command_locked(connectionObject *conn,
                                 const char *cmd, const char *tid,
                                 PGresult **pgres, char **error,
                                 PyThreadState **tstate);
HIDDEN int pq_is_busy(connectionObject *conn);
HIDDEN int pq_is_busy_locked(connectionObject *conn);
HIDDEN int pq_flush(connectionObject *conn);
HIDDEN void pq_clear_async(connectionObject *conn);
RAISES_NEG HIDDEN int pq_set_non_blocking(connectionObject *conn, int arg);

HIDDEN void pq_set_critical(connectionObject *conn, const char *msg);

HIDDEN int pq_execute_command_locked(connectionObject *conn,
                                     const char *query,
                                     PGresult **pgres, char **error,
                                     PyThreadState **tstate);
RAISES HIDDEN void pq_complete_error(connectionObject *conn, PGresult **pgres,
                              char **error);

#endif /* !defined(PSYCOPG_PQPATH_H) */
