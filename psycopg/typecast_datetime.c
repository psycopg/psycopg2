/* typecast_datetime.c - date and time typecasting functions to python types
 *
 * Copyright (C) 2001-2010 Federico Di Gregorio <fog@debian.org>
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

#include <math.h>
#include "datetime.h"

RAISES_NEG static int
psyco_typecast_datetime_init(void)
{
    Dprintf("psyco_typecast_datetime_init: datetime init");

    PyDateTime_IMPORT;

    if (!PyDateTimeAPI) {
        PyErr_SetString(PyExc_ImportError, "datetime initialization failed");
        return -1;
    }
    return 0;
}

/** DATE - cast a date into a date python object **/

static PyObject *
typecast_PYDATE_cast(const char *str, Py_ssize_t len, PyObject *curs)
{
    PyObject* obj = NULL;
    int n, y=0, m=0, d=0;

    if (str == NULL) {Py_INCREF(Py_None); return Py_None;}

    if (!strcmp(str, "infinity") || !strcmp(str, "-infinity")) {
        if (str[0] == '-') {
            obj = PyObject_GetAttrString(
                (PyObject*)PyDateTimeAPI->DateType, "min");
        }
        else {
            obj = PyObject_GetAttrString(
                (PyObject*)PyDateTimeAPI->DateType, "max");
        }
    }

    else {
        n = typecast_parse_date(str, NULL, &len, &y, &m, &d);
        Dprintf("typecast_PYDATE_cast: "
                "n = %d, len = " FORMAT_CODE_PY_SSIZE_T ", "
                "y = %d, m = %d, d = %d",
                 n, len, y, m, d);
        if (n != 3) {
            PyErr_SetString(DataError, "unable to parse date");
            return NULL;
        }
        else {
            if (y > 9999) y = 9999;
            obj = PyObject_CallFunction(
                (PyObject*)PyDateTimeAPI->DateType, "iii", y, m, d);
        }
    }
    return obj;
}

/** DATETIME - cast a timestamp into a datetime python object **/

static PyObject *
typecast_PYDATETIME_cast(const char *str, Py_ssize_t len, PyObject *curs)
{
    PyObject* obj = NULL;
    PyObject *tzinfo = NULL;
    PyObject *tzinfo_factory;
    int n, y=0, m=0, d=0;
    int hh=0, mm=0, ss=0, us=0, tz=0;
    const char *tp = NULL;

    if (str == NULL) {Py_INCREF(Py_None); return Py_None;}

    /* check for infinity */
    if (!strcmp(str, "infinity") || !strcmp(str, "-infinity")) {
        if (str[0] == '-') {
            obj = PyObject_GetAttrString(
                (PyObject*)PyDateTimeAPI->DateTimeType, "min");
        }
        else {
            obj = PyObject_GetAttrString(
                (PyObject*)PyDateTimeAPI->DateTimeType, "max");
        }
    }

    else {
        Dprintf("typecast_PYDATETIME_cast: s = %s", str);
        n = typecast_parse_date(str, &tp, &len, &y, &m, &d);
        Dprintf("typecast_PYDATE_cast: tp = %p "
                "n = %d, len = " FORMAT_CODE_PY_SSIZE_T ","
                " y = %d, m = %d, d = %d",
                 tp, n, len, y, m, d);
        if (n != 3) {
            PyErr_SetString(DataError, "unable to parse date");
            return NULL;
        }

        if (len > 0) {
            n = typecast_parse_time(tp, NULL, &len, &hh, &mm, &ss, &us, &tz);
            Dprintf("typecast_PYDATETIME_cast: n = %d,"
                " len = " FORMAT_CODE_PY_SSIZE_T ","
                " hh = %d, mm = %d, ss = %d, us = %d, tz = %d",
                n, len, hh, mm, ss, us, tz);
            if (n < 3 || n > 6) {
                PyErr_SetString(DataError, "unable to parse time");
                return NULL;
            }
        }

        if (ss > 59) {
            mm += 1;
            ss -= 60;
        }
        if (y > 9999)
            y = 9999;

        tzinfo_factory = ((cursorObject *)curs)->tzinfo_factory;
        if (n >= 5 && tzinfo_factory != Py_None) {
            /* we have a time zone, calculate minutes and create
               appropriate tzinfo object calling the factory */
            Dprintf("typecast_PYDATETIME_cast: UTC offset = %ds", tz);

            /* The datetime module requires that time zone offsets be
               a whole number of minutes, so truncate the seconds to the
               closest minute. */
            // printf("%d %d %d\n", tz, tzmin, round(tz / 60.0));
            tzinfo = PyObject_CallFunction(tzinfo_factory, "i",
                (int)round(tz / 60.0));
        } else {
            Py_INCREF(Py_None);
            tzinfo = Py_None;
        }
        if (tzinfo != NULL) {
            obj = PyObject_CallFunction(
                (PyObject*)PyDateTimeAPI->DateTimeType, "iiiiiiiO",
                y, m, d, hh, mm, ss, us, tzinfo);
            Dprintf("typecast_PYDATETIME_cast: tzinfo: %p, refcnt = "
                FORMAT_CODE_PY_SSIZE_T,
                tzinfo, Py_REFCNT(tzinfo)
              );
            Py_DECREF(tzinfo);
        }
    }
    return obj;
}

/** TIME - parse time into a time object **/

static PyObject *
typecast_PYTIME_cast(const char *str, Py_ssize_t len, PyObject *curs)
{
    PyObject* obj = NULL;
    PyObject *tzinfo = NULL;
    PyObject *tzinfo_factory;
    int n, hh=0, mm=0, ss=0, us=0, tz=0;

    if (str == NULL) {Py_INCREF(Py_None); return Py_None;}

    n = typecast_parse_time(str, NULL, &len, &hh, &mm, &ss, &us, &tz);
    Dprintf("typecast_PYTIME_cast: n = %d, len = " FORMAT_CODE_PY_SSIZE_T ", "
            "hh = %d, mm = %d, ss = %d, us = %d, tz = %d",
            n, len, hh, mm, ss, us, tz);

    if (n < 3 || n > 6) {
        PyErr_SetString(DataError, "unable to parse time");
        return NULL;
    }
    if (ss > 59) {
        mm += 1;
        ss -= 60;
    }
    tzinfo_factory = ((cursorObject *)curs)->tzinfo_factory;
    if (n >= 5 && tzinfo_factory != Py_None) {
        /* we have a time zone, calculate minutes and create
           appropriate tzinfo object calling the factory */
        Dprintf("typecast_PYTIME_cast: UTC offset = %ds", tz);

        /* The datetime module requires that time zone offsets be
           a whole number of minutes, so truncate the seconds to the
           closest minute. */
        tzinfo = PyObject_CallFunction(tzinfo_factory, "i",
            (int)round(tz / 60.0));
    } else {
        Py_INCREF(Py_None);
        tzinfo = Py_None;
    }
    if (tzinfo != NULL) {
        obj = PyObject_CallFunction((PyObject*)PyDateTimeAPI->TimeType, "iiiiO",
                                    hh, mm, ss, us, tzinfo);
        Py_DECREF(tzinfo);
    }
    return obj;
}

/** INTERVAL - parse an interval into a timedelta object **/

static PyObject *
typecast_PYINTERVAL_cast(const char *str, Py_ssize_t len, PyObject *curs)
{
    long years = 0, months = 0, days = 0;
    double hours = 0.0, minutes = 0.0, seconds = 0.0, hundredths = 0.0;
    double v = 0.0, sign = 1.0, denominator = 1.0;
    int part = 0, sec;
    double micro;

    if (str == NULL) {Py_INCREF(Py_None); return Py_None;}

    Dprintf("typecast_PYINTERVAL_cast: s = %s", str);

    while (len-- > 0 && *str) {
        switch (*str) {

        case '-':
            sign = -1.0;
            break;

        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            v = v * 10.0 + (double)(*str - '0');
            if (part == 6){
                denominator *= 10;
            }
            break;

        case 'y':
            if (part == 0) {
                years = (long)(v*sign);
                str = skip_until_space2(str, &len);
                v = 0.0; sign = 1.0; part = 1;
            }
            break;

        case 'm':
            if (part <= 1) {
                months = (long)(v*sign);
                str = skip_until_space2(str, &len);
                v = 0.0; sign = 1.0; part = 2;
            }
            break;

        case 'd':
            if (part <= 2) {
                days = (long)(v*sign);
                str = skip_until_space2(str, &len);
                v = 0.0; sign = 1.0; part = 3;
            }
            break;

        case ':':
            if (part <= 3) {
                hours = v;
                v = 0.0; part = 4;
            }
            else if (part == 4) {
                minutes = v;
                v = 0.0; part = 5;
            }
            break;

        case '.':
            if (part == 5) {
                seconds = v;
                v = 0.0; part = 6;
            }
            break;

        default:
            break;
        }

        str++;
    }

    /* manage last value, be it minutes or seconds or hundredths of a second */
    if (part == 4) {
        minutes = v;
    }
    else if (part == 5) {
        seconds = v;
    }
    else if (part == 6) {
        hundredths = v;
        hundredths = hundredths/denominator;
    }

    /* calculates seconds */
    if (sign < 0.0) {
        seconds = - (hundredths + seconds + minutes*60 + hours*3600);
    }
    else {
        seconds += hundredths + minutes*60 + hours*3600;
    }

    /* calculates days */
    days += years*365 + months*30;

    micro = (seconds - floor(seconds)) * 1000000.0;
    sec = (int)floor(seconds);
    return PyObject_CallFunction((PyObject*)PyDateTimeAPI->DeltaType, "iii",
                                 days, sec, (int)round(micro));
}

/* psycopg defaults to using python datetime types */

#ifdef PSYCOPG_DEFAULT_PYDATETIME
#define typecast_DATE_cast typecast_PYDATE_cast
#define typecast_TIME_cast typecast_PYTIME_cast
#define typecast_INTERVAL_cast typecast_PYINTERVAL_cast
#define typecast_DATETIME_cast typecast_PYDATETIME_cast
#endif
