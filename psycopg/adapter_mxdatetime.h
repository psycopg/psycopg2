/* adapter_mxdatetime.h - definition for the mx date/time types
 *
 * Copyright (C) 2003-2004 Federico Di Gregorio <fog@debian.org>
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

#ifndef PSYCOPG_MXDATETIME_H
#define PSYCOPG_MXDATETIME_H 1

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "psycopg/config.h"

#ifdef __cplusplus
extern "C" {
#endif

extern HIDDEN PyTypeObject mxdatetimeType;

typedef struct {
    PyObject_HEAD

    PyObject *wrapped;
    int       type;
#define       PSYCO_MXDATETIME_TIME       0
#define       PSYCO_MXDATETIME_DATE       1
#define       PSYCO_MXDATETIME_TIMESTAMP  2
#define       PSYCO_MXDATETIME_INTERVAL   3

} mxdatetimeObject;

/* functions exported to psycopgmodule.c */
#ifdef PSYCOPG_DEFAULT_MXDATETIME

HIDDEN PyObject *psyco_Date(PyObject *module, PyObject *args);
#define psyco_Date_doc \
    "Date(year, month, day) -> new date"

HIDDEN PyObject *psyco_Time(PyObject *module, PyObject *args);
#define psyco_Time_doc \
    "Time(hour, minutes, seconds) -> new time"

HIDDEN PyObject *psyco_Timestamp(PyObject *module, PyObject *args);
#define psyco_Timestamp_doc \
    "Time(year, month, day, hour, minutes, seconds) -> new timestamp"

HIDDEN PyObject *psyco_DateFromTicks(PyObject *module, PyObject *args);
#define psyco_DateFromTicks_doc \
    "DateFromTicks(ticks) -> new date"

HIDDEN PyObject *psyco_TimeFromTicks(PyObject *module, PyObject *args);
#define psyco_TimeFromTicks_doc \
    "TimeFromTicks(ticks) -> new time"

HIDDEN PyObject *psyco_TimestampFromTicks(PyObject *module, PyObject *args);
#define psyco_TimestampFromTicks_doc \
    "TimestampFromTicks(ticks) -> new timestamp"

#endif /* PSYCOPG_DEFAULT_MXDATETIME */

HIDDEN PyObject *psyco_DateFromMx(PyObject *module, PyObject *args);
#define psyco_DateFromMx_doc \
    "DateFromMx(mx) -> new date"

HIDDEN PyObject *psyco_TimeFromMx(PyObject *module, PyObject *args);
#define psyco_TimeFromMx_doc \
    "TimeFromMx(mx) -> new time"

HIDDEN PyObject *psyco_TimestampFromMx(PyObject *module, PyObject *args);
#define psyco_TimestampFromMx_doc \
    "TimestampFromMx(mx) -> new timestamp"

HIDDEN PyObject *psyco_IntervalFromMx(PyObject *module, PyObject *args);
#define psyco_IntervalFromMx_doc \
    "IntervalFromMx(mx) -> new interval"

#ifdef __cplusplus
}
#endif

#endif /* !defined(PSYCOPG_MXDATETIME_H) */
