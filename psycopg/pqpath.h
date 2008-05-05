/* pqpath.h - definitions for pqpath.c
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

#ifndef PSYCOPG_PQPATH_H
#define PSYCOPG_PQPATH_H 1

#include "psycopg/config.h"
#include "psycopg/cursor.h"
#include "psycopg/connection.h"

/* macros to clean the pg result */
#define IFCLEARPGRES(pgres)  if (pgres) {PQclear(pgres); pgres = NULL;}
#define CLEARPGRES(pgres)    PQclear(pgres); pgres = NULL

/* exported functions */
HIDDEN int pq_fetch(cursorObject *curs);
HIDDEN int pq_execute(cursorObject *curs, const char *query, int async);
HIDDEN int pq_begin_locked(connectionObject *conn, PGresult **pgres,
                           char **error);
HIDDEN int pq_commit(connectionObject *conn);
HIDDEN int pq_abort_locked(connectionObject *conn, PGresult **pgres,
                           char **error);
HIDDEN int pq_abort(connectionObject *conn);
HIDDEN int pq_is_busy(connectionObject *conn);

HIDDEN void pq_set_critical(connectionObject *conn, const char *msg);

HIDDEN int pq_execute_command_locked(connectionObject *conn,
                                     const char *query,
                                     PGresult **pgres, char **error);
HIDDEN void pq_complete_error(connectionObject *conn, PGresult **pgres,
                              char **error);

#endif /* !defined(PSYCOPG_PQPATH_H) */
