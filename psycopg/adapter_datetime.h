/* adapter_datetime.h - definition for the python date/time types
 *
 * Copyright (C) 2004 Federico Di Gregorio <fog@debian.org>
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

#ifndef PSYCOPG_DATETIME_H
#define PSYCOPG_DATETIME_H 1

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "psycopg/config.h"

#ifdef __cplusplus
extern "C" {
#endif

extern HIDDEN PyTypeObject pydatetimeType;

typedef struct {
    PyObject_HEAD

    PyObject *wrapped;
    int       type;
#define       PSYCO_DATETIME_TIME       0
#define       PSYCO_DATETIME_DATE       1
#define       PSYCO_DATETIME_TIMESTAMP  2
#define       PSYCO_DATETIME_INTERVAL   3

} pydatetimeObject;


/* functions exported to psycopgmodule.c */
#ifdef PSYCOPG_DEFAULT_PYDATETIME

HIDDEN PyObject *psyco_Date(PyObject *module, PyObject *args);
#define psyco_Date_doc \
    "Date(year, month, day) -> new date\n\n" \
    "Build an object holding a date value."

HIDDEN PyObject *psyco_Time(PyObject *module, PyObject *args);
#define psyco_Time_doc \
    "Time(hour, minutes, seconds, tzinfo=None) -> new time\n\n" \
    "Build an object holding a time value."

HIDDEN PyObject *psyco_Timestamp(PyObject *module, PyObject *args);
#define psyco_Timestamp_doc \
    "Timestamp(year, month, day, hour, minutes, seconds, tzinfo=None) -> new timestamp\n\n" \
    "Build an object holding a timestamp value."

HIDDEN PyObject *psyco_DateFromTicks(PyObject *module, PyObject *args);
#define psyco_DateFromTicks_doc \
    "DateFromTicks(ticks) -> new date\n\n" \
    "Build an object holding a date value from the given ticks value.\n\n" \
    "Ticks are the number of seconds since the epoch; see the documentation " \
    "of the standard Python time module for details)."

HIDDEN PyObject *psyco_TimeFromTicks(PyObject *module, PyObject *args);
#define psyco_TimeFromTicks_doc \
    "TimeFromTicks(ticks) -> new time\n\n" \
    "Build an object holding a time value from the given ticks value.\n\n" \
    "Ticks are the number of seconds since the epoch; see the documentation " \
    "of the standard Python time module for details)."

HIDDEN PyObject *psyco_TimestampFromTicks(PyObject *module, PyObject *args);
#define psyco_TimestampFromTicks_doc \
    "TimestampFromTicks(ticks) -> new timestamp\n\n" \
    "Build an object holding a timestamp value from the given ticks value.\n\n" \
    "Ticks are the number of seconds since the epoch; see the documentation " \
    "of the standard Python time module for details)."

#endif /* PSYCOPG_DEFAULT_PYDATETIME */

HIDDEN PyObject *psyco_DateFromPy(PyObject *module, PyObject *args);
#define psyco_DateFromPy_doc \
    "DateFromPy(datetime.date) -> new wrapper"

HIDDEN PyObject *psyco_TimeFromPy(PyObject *module, PyObject *args);
#define psyco_TimeFromPy_doc \
    "TimeFromPy(datetime.time) -> new wrapper"

HIDDEN PyObject *psyco_TimestampFromPy(PyObject *module, PyObject *args);
#define psyco_TimestampFromPy_doc \
    "TimestampFromPy(datetime.datetime) -> new wrapper"

HIDDEN PyObject *psyco_IntervalFromPy(PyObject *module, PyObject *args);
#define psyco_IntervalFromPy_doc \
    "IntervalFromPy(datetime.timedelta) -> new wrapper"

#ifdef __cplusplus
}
#endif

#endif /* !defined(PSYCOPG_DATETIME_H) */
