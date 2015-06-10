/* libpq_support.c - functions not provided by libpq, but which are
 * required for advanced communication with the server, such as
 * streaming replication
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

#define PSYCOPG_MODULE
#include "psycopg/psycopg.h"

#include "psycopg/libpq_support.h"

/* htonl(), ntohl() */
#ifdef _WIN32
#include <winsock2.h>
/* gettimeofday() */
#include "psycopg/win32_support.h"
#else
#include <arpa/inet.h>
#endif

/* support routines taken from pg_basebackup/streamutil.c */

/*
 * Frontend version of GetCurrentTimestamp(), since we are not linked with
 * backend code. The protocol always uses integer timestamps, regardless of
 * server setting.
 */
pg_int64
feGetCurrentTimestamp(void)
{
    pg_int64 result;
    struct timeval tp;

    gettimeofday(&tp, NULL);

    result = (pg_int64) tp.tv_sec -
        ((POSTGRES_EPOCH_JDATE - UNIX_EPOCH_JDATE) * SECS_PER_DAY);

    result = (result * USECS_PER_SEC) + tp.tv_usec;

    return result;
}

/*
 * Converts an int64 to network byte order.
 */
void
fe_sendint64(pg_int64 i, char *buf)
{
    uint32 n32;

    /* High order half first, since we're doing MSB-first */
    n32 = (uint32) (i >> 32);
    n32 = htonl(n32);
    memcpy(&buf[0], &n32, 4);

    /* Now the low order half */
    n32 = (uint32) i;
    n32 = htonl(n32);
    memcpy(&buf[4], &n32, 4);
}

/*
 * Converts an int64 from network byte order to native format.
 */
pg_int64
fe_recvint64(char *buf)
{
    pg_int64 result;
    uint32 h32;
    uint32 l32;

    memcpy(&h32, buf, 4);
    memcpy(&l32, buf + 4, 4);
    h32 = ntohl(h32);
    l32 = ntohl(l32);

    result = h32;
    result <<= 32;
    result |= l32;

    return result;
}
