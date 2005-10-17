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

#include <Python.h>

#ifdef __cplusplus
extern "C" {
#endif

extern PyTypeObject pydatetimeType;

typedef struct {
    PyObject HEAD;

    PyObject *wrapped;
    int       type;
#define       PSYCO_DATETIME_TIME       0
#define       PSYCO_DATETIME_DATE       1
#define       PSYCO_DATETIME_TIMESTAMP  2
#define       PSYCO_DATETIME_INTERVAL   3    
    
} pydatetimeObject;
    
    
/* functions exported to psycopgmodule.c */
#ifdef PSYCOPG_DEFAULT_PYDATETIME
    
extern PyObject *psyco_Date(PyObject *module, PyObject *args);
#define psyco_Date_doc \
    "psycopg.Date(year, month, day) -> new date"

extern PyObject *psyco_Time(PyObject *module, PyObject *args);
#define psyco_Time_doc \
    "psycopg.Time(hour, minutes, seconds, tzinfo=None) -> new time"

extern PyObject *psyco_Timestamp(PyObject *module, PyObject *args);
#define psyco_Timestamp_doc \
    "psycopg.Time(year, month, day, hour, minutes, seconds, tzinfo=None) -> new timestamp"

extern PyObject *psyco_DateFromTicks(PyObject *module, PyObject *args);
#define psyco_DateFromTicks_doc \
    "psycopg.DateFromTicks(ticks) -> new date"

extern PyObject *psyco_TimeFromTicks(PyObject *module, PyObject *args);
#define psyco_TimeFromTicks_doc \
    "psycopg.TimeFromTicks(ticks) -> new date"

extern PyObject *psyco_TimestampFromTicks(PyObject *module, PyObject *args);
#define psyco_TimestampFromTicks_doc \
    "psycopg.TimestampFromTicks(ticks) -> new date"

#endif /* PSYCOPG_DEFAULT_PYDATETIME */

extern PyObject *psyco_DateFromPy(PyObject *module, PyObject *args);
#define psyco_DateFromPy_doc \
    "psycopg.DateFromPy(datetime.date) -> new wrapper"

extern PyObject *psyco_TimeFromPy(PyObject *module, PyObject *args);
#define psyco_TimeFromPy_doc \
    "psycopg.TimeFromPy(datetime.time) -> new wrapper"

extern PyObject *psyco_TimestampFromPy(PyObject *module, PyObject *args);
#define psyco_TimestampFromPy_doc \
    "psycopg.TimestampFromPy(datetime.datetime) -> new wrapper"

extern PyObject *psyco_IntervalFromPy(PyObject *module, PyObject *args);
#define psyco_IntervalFromPy_doc \
    "psycopg.IntervalFromPy(datetime.timedelta) -> new wrapper"
    
#ifdef __cplusplus
}
#endif

#endif /* !defined(PSYCOPG_DATETIME_H) */
