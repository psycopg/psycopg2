/* libpq_support.h - definitions for libpq_support.c
 *
 * Copyright (C) 2003-2015 Federico Di Gregorio <fog@debian.org>
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
#ifndef PSYCOPG_LIBPQ_SUPPORT_H
#define PSYCOPG_LIBPQ_SUPPORT_H 1

#include "psycopg/config.h"

/* type and constant definitions from internal postgres includes */
typedef unsigned int uint32;
typedef unsigned PG_INT64_TYPE XLogRecPtr;

#define InvalidXLogRecPtr ((XLogRecPtr) 0)

HIDDEN pg_int64 feGetCurrentTimestamp(void);
HIDDEN void fe_sendint64(pg_int64 i, char *buf);
HIDDEN pg_int64 fe_recvint64(char *buf);

#endif /* !defined(PSYCOPG_LIBPQ_SUPPORT_H) */
