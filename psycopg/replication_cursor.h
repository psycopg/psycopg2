/* replication_cursor.h - definition for the psycopg replication cursor type
 *
 * Copyright (C) 2015 Daniele Varrazzo <daniele.varrazzo@gmail.com>
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

#ifndef PSYCOPG_REPLICATION_CURSOR_H
#define PSYCOPG_REPLICATION_CURSOR_H 1

#include "psycopg/cursor.h"
#include "libpq_support.h"

#ifdef __cplusplus
extern "C" {
#endif

extern HIDDEN PyTypeObject replicationCursorType;

typedef struct replicationCursorObject {
    cursorObject cur;

    int         consuming:1;      /* if running the consume loop */
    int         decode:1;         /* if we should use character decoding on the messages */

    struct timeval last_io;       /* timestamp of the last exchange with the server */
    struct timeval keepalive_interval;   /* interval for keepalive messages in replication mode */

    XLogRecPtr  write_lsn;        /* LSNs for replication feedback messages */
    XLogRecPtr  flush_lsn;
    XLogRecPtr  apply_lsn;
} replicationCursorObject;


RAISES_NEG int psyco_repl_curs_datetime_init(void);

#ifdef __cplusplus
}
#endif

#endif /* !defined(PSYCOPG_REPLICATION_CURSOR_H) */
