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

#include "psycopg/cursor.h"
#include "psycopg/connection.h"

/* macros to clean the pg result */
#define IFCLEARPGRES(pgres)  if (pgres) {PQclear(pgres); pgres = NULL;}
#define CLEARPGRES(pgres)    PQclear(pgres); pgres = NULL

/* exported functions */
extern int pq_fetch(cursorObject *curs);
extern int pq_execute(cursorObject *curs, const char *query, int async);
extern int pq_begin(connectionObject *conn);
extern int pq_commit(connectionObject *conn);
extern int pq_abort(connectionObject *conn);
extern int pq_is_busy(connectionObject *conn);
extern void pq_set_critical(connectionObject *conn, const char *msg);

#endif /* !defined(PSYCOPG_PQPATH_H) */
