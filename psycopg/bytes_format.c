/* bytes_format.c - bytes-oriented version of PyString_Format
 *
 * Copyright (C) 2010 Daniele Varrazzo <daniele.varrazzo@gmail.com>
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

/* This implementation is based on the PyString_Format function available in
 * Python 2.7.1. Original license follows.
 *
 * PYTHON SOFTWARE FOUNDATION LICENSE VERSION 2
 * --------------------------------------------
 *
 * 1. This LICENSE AGREEMENT is between the Python Software Foundation
 * ("PSF"), and the Individual or Organization ("Licensee") accessing and
 * otherwise using this software ("Python") in source or binary form and
 * its associated documentation.
 *
 * 2. Subject to the terms and conditions of this License Agreement, PSF hereby
 * grants Licensee a nonexclusive, royalty-free, world-wide license to reproduce,
 * analyze, test, perform and/or display publicly, prepare derivative works,
 * distribute, and otherwise use Python alone or in any derivative version,
 * provided, however, that PSF's License Agreement and PSF's notice of copyright,
 * i.e., "Copyright (c) 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010
 * Python Software Foundation; All Rights Reserved" are retained in Python alone or
 * in any derivative version prepared by Licensee.
 *
 * 3. In the event Licensee prepares a derivative work that is based on
 * or incorporates Python or any part thereof, and wants to make
 * the derivative work available to others as provided herein, then
 * Licensee hereby agrees to include in any such work a brief summary of
 * the changes made to Python.
 *
 * 4. PSF is making Python available to Licensee on an "AS IS"
 * basis.  PSF MAKES NO REPRESENTATIONS OR WARRANTIES, EXPRESS OR
 * IMPLIED.  BY WAY OF EXAMPLE, BUT NOT LIMITATION, PSF MAKES NO AND
 * DISCLAIMS ANY REPRESENTATION OR WARRANTY OF MERCHANTABILITY OR FITNESS
 * FOR ANY PARTICULAR PURPOSE OR THAT THE USE OF PYTHON WILL NOT
 * INFRINGE ANY THIRD PARTY RIGHTS.
 *
 * 5. PSF SHALL NOT BE LIABLE TO LICENSEE OR ANY OTHER USERS OF PYTHON
 * FOR ANY INCIDENTAL, SPECIAL, OR CONSEQUENTIAL DAMAGES OR LOSS AS
 * A RESULT OF MODIFYING, DISTRIBUTING, OR OTHERWISE USING PYTHON,
 * OR ANY DERIVATIVE THEREOF, EVEN IF ADVISED OF THE POSSIBILITY THEREOF.
 *
 * 6. This License Agreement will automatically terminate upon a material
 * breach of its terms and conditions.
 *
 * 7. Nothing in this License Agreement shall be deemed to create any
 * relationship of agency, partnership, or joint venture between PSF and
 * Licensee.  This License Agreement does not grant permission to use PSF
 * trademarks or trade name in a trademark sense to endorse or promote
 * products or services of Licensee, or any third party.
 *
 * 8. By copying, installing or otherwise using Python, Licensee
 * agrees to be bound by the terms and conditions of this License
 * Agreement.
 */

#define PSYCOPG_MODULE
#include "psycopg/psycopg.h"

#ifndef Py_USING_UNICODE
#define Py_USING_UNICODE 1
#endif

/* Helpers for formatstring */

Py_LOCAL_INLINE(PyObject *)
getnextarg(PyObject *args, Py_ssize_t arglen, Py_ssize_t *p_argidx)
{
    Py_ssize_t argidx = *p_argidx;
    if (argidx < arglen) {
        (*p_argidx)++;
        if (arglen < 0)
            return args;
        else
            return PyTuple_GetItem(args, argidx);
    }
    PyErr_SetString(PyExc_TypeError,
                    "not enough arguments for format string");
    return NULL;
}

/* fmt%(v1,v2,...) is roughly equivalent to sprintf(fmt, v1, v2, ...)

   FORMATBUFLEN is the length of the buffer in which the ints &
   chars are formatted. XXX This is a magic number. Each formatting
   routine does bounds checking to ensure no overflow, but a better
   solution may be to malloc a buffer of appropriate size for each
   format. For now, the current solution is sufficient.
*/
#define FORMATBUFLEN (size_t)120

PyObject *
PyBytes_Format(PyObject *format, PyObject *args)
{
    char *fmt, *res;
    Py_ssize_t arglen, argidx;
    Py_ssize_t reslen, rescnt, fmtcnt;
    int args_owned = 0;
    PyObject *result, *orig_args;
#ifdef Py_USING_UNICODE
    PyObject *v, *w;
#endif
    PyObject *dict = NULL;
    if (format == NULL || !PyBytes_Check(format) || args == NULL) {
        PyErr_BadInternalCall();
        return NULL;
    }
    orig_args = args;
    fmt = PyBytes_AS_STRING(format);
    fmtcnt = PyBytes_GET_SIZE(format);
    reslen = rescnt = fmtcnt + 100;
    result = PyBytes_FromStringAndSize((char *)NULL, reslen);
    if (result == NULL)
        return NULL;
    res = PyBytes_AsString(result);
    if (PyTuple_Check(args)) {
        arglen = PyTuple_GET_SIZE(args);
        argidx = 0;
    }
    else {
        arglen = -1;
        argidx = -2;
    }
    if (Py_TYPE(args)->tp_as_mapping && !PyTuple_Check(args) &&
        !PyObject_TypeCheck(args, &PyBytes_Type))
        dict = args;
    while (--fmtcnt >= 0) {
        if (*fmt != '%') {
            if (--rescnt < 0) {
                rescnt = fmtcnt + 100;
                reslen += rescnt;
                if (_PyBytes_Resize(&result, reslen))
                    return NULL;
                res = PyBytes_AS_STRING(result)
                    + reslen - rescnt;
                --rescnt;
            }
            *res++ = *fmt++;
        }
        else {
            /* Got a format specifier */
            int flags = 0;
            Py_ssize_t width = -1;
            int prec = -1;
            int c = '\0';
            int fill;
            int isnumok;
            PyObject *v = NULL;
            PyObject *temp = NULL;
            char *pbuf;
            int sign;
            Py_ssize_t len;
            char formatbuf[FORMATBUFLEN];
                 /* For format{int,char}() */
#ifdef Py_USING_UNICODE
            char *fmt_start = fmt;
            Py_ssize_t argidx_start = argidx;
#endif

            fmt++;
            if (*fmt == '(') {
                char *keystart;
                Py_ssize_t keylen;
                PyObject *key;
                int pcount = 1;

                if (dict == NULL) {
                    PyErr_SetString(PyExc_TypeError,
                             "format requires a mapping");
                    goto error;
                }
                ++fmt;
                --fmtcnt;
                keystart = fmt;
                /* Skip over balanced parentheses */
                while (pcount > 0 && --fmtcnt >= 0) {
                    if (*fmt == ')')
                        --pcount;
                    else if (*fmt == '(')
                        ++pcount;
                    fmt++;
                }
                keylen = fmt - keystart - 1;
                if (fmtcnt < 0 || pcount > 0) {
                    PyErr_SetString(PyExc_ValueError,
                               "incomplete format key");
                    goto error;
                }
                key = PyUnicode_FromStringAndSize(keystart, keylen);
                if (key == NULL)
                    goto error;
                if (args_owned) {
                    Py_DECREF(args);
                    args_owned = 0;
                }
                args = PyObject_GetItem(dict, key);
                Py_DECREF(key);
                if (args == NULL) {
                    goto error;
                }
                args_owned = 1;
                arglen = -1;
                argidx = -2;
            }
            while (--fmtcnt >= 0) {
                switch (c = *fmt++) {
                case '-': flags |= F_LJUST; continue;
                case '+': flags |= F_SIGN; continue;
                case ' ': flags |= F_BLANK; continue;
                case '#': flags |= F_ALT; continue;
                case '0': flags |= F_ZERO; continue;
                }
                break;
            }
            if (c == '*') {
                v = getnextarg(args, arglen, &argidx);
                if (v == NULL)
                    goto error;
                if (!PyLong_Check(v)) {
                    PyErr_SetString(PyExc_TypeError,
                                    "* wants int");
                    goto error;
                }
                width = PyLong_AsLong(v);
                if (width < 0) {
                    flags |= F_LJUST;
                    width = -width;
                }
                if (--fmtcnt >= 0)
                    c = *fmt++;
            }
            else if (c >= 0 && isdigit(c)) {
                width = c - '0';
                while (--fmtcnt >= 0) {
                    c = Py_CHARMASK(*fmt++);
                    if (!isdigit(c))
                        break;
                    if ((width*10) / 10 != width) {
                        PyErr_SetString(
                            PyExc_ValueError,
                            "width too big");
                        goto error;
                    }
                    width = width*10 + (c - '0');
                }
            }
            if (c == '.') {
                prec = 0;
                if (--fmtcnt >= 0)
                    c = *fmt++;
                if (c == '*') {
                    v = getnextarg(args, arglen, &argidx);
                    if (v == NULL)
                        goto error;
                    if (!PyLong_Check(v)) {
                        PyErr_SetString(
                            PyExc_TypeError,
                            "* wants int");
                        goto error;
                    }
                    prec = PyLong_AsLong(v);
                    if (prec < 0)
                        prec = 0;
                    if (--fmtcnt >= 0)
                        c = *fmt++;
                }
                else if (c >= 0 && isdigit(c)) {
                    prec = c - '0';
                    while (--fmtcnt >= 0) {
                        c = Py_CHARMASK(*fmt++);
                        if (!isdigit(c))
                            break;
                        if ((prec*10) / 10 != prec) {
                            PyErr_SetString(
                                PyExc_ValueError,
                                "prec too big");
                            goto error;
                        }
                        prec = prec*10 + (c - '0');
                    }
                }
            } /* prec */
            if (fmtcnt >= 0) {
                if (c == 'h' || c == 'l' || c == 'L') {
                    if (--fmtcnt >= 0)
                        c = *fmt++;
                }
            }
            if (fmtcnt < 0) {
                PyErr_SetString(PyExc_ValueError,
                                "incomplete format");
                goto error;
            }
            if (c != '%') {
                v = getnextarg(args, arglen, &argidx);
                if (v == NULL)
                    goto error;
            }
            sign = 0;
            fill = ' ';
            switch (c) {
            case '%':
                pbuf = "%";
                len = 1;
                break;
            case 's':
                /* only bytes! */
                if (!PyBytes_CheckExact(v)) {
                    PyErr_Format(PyExc_ValueError,
                                    "only bytes values expected, got %s",
                                    Py_TYPE(v)->tp_name);
                    goto error;
                }
                temp = v;
                Py_INCREF(v);
                /* Fall through */
            case 'r':
                if (c == 'r')
                    temp = PyObject_Repr(v);
                if (temp == NULL)
                    goto error;
                if (!PyBytes_Check(temp)) {
                    PyErr_SetString(PyExc_TypeError,
                      "%s argument has non-string str()");
                    Py_DECREF(temp);
                    goto error;
                }
                pbuf = PyBytes_AS_STRING(temp);
                len = PyBytes_GET_SIZE(temp);
                if (prec >= 0 && len > prec)
                    len = prec;
                break;
            default:
                PyErr_Format(PyExc_ValueError,
                  "unsupported format character '%c' (0x%x) "
                  "at index %zd",
                  c, c,
                  (Py_ssize_t)(fmt - 1 -
                               PyBytes_AsString(format)));
                goto error;
            }
            if (sign) {
                if (*pbuf == '-' || *pbuf == '+') {
                    sign = *pbuf++;
                    len--;
                }
                else if (flags & F_SIGN)
                    sign = '+';
                else if (flags & F_BLANK)
                    sign = ' ';
                else
                    sign = 0;
            }
            if (width < len)
                width = len;
            if (rescnt - (sign != 0) < width) {
                reslen -= rescnt;
                rescnt = width + fmtcnt + 100;
                reslen += rescnt;
                if (reslen < 0) {
                    Py_DECREF(result);
                    Py_XDECREF(temp);
                    return PyErr_NoMemory();
                }
                if (_PyBytes_Resize(&result, reslen)) {
                    Py_XDECREF(temp);
                    return NULL;
                }
                res = PyBytes_AS_STRING(result)
                    + reslen - rescnt;
            }
            if (sign) {
                if (fill != ' ')
                    *res++ = sign;
                rescnt--;
                if (width > len)
                    width--;
            }
            if ((flags & F_ALT) && (c == 'x' || c == 'X')) {
                assert(pbuf[0] == '0');
                assert(pbuf[1] == c);
                if (fill != ' ') {
                    *res++ = *pbuf++;
                    *res++ = *pbuf++;
                }
                rescnt -= 2;
                width -= 2;
                if (width < 0)
                    width = 0;
                len -= 2;
            }
            if (width > len && !(flags & F_LJUST)) {
                do {
                    --rescnt;
                    *res++ = fill;
                } while (--width > len);
            }
            if (fill == ' ') {
                if (sign)
                    *res++ = sign;
                if ((flags & F_ALT) &&
                    (c == 'x' || c == 'X')) {
                    assert(pbuf[0] == '0');
                    assert(pbuf[1] == c);
                    *res++ = *pbuf++;
                    *res++ = *pbuf++;
                }
            }
            Py_MEMCPY(res, pbuf, len);
            res += len;
            rescnt -= len;
            while (--width >= len) {
                --rescnt;
                *res++ = ' ';
            }
            if (dict && (argidx < arglen) && c != '%') {
                PyErr_SetString(PyExc_TypeError,
                           "not all arguments converted during string formatting");
                Py_XDECREF(temp);
                goto error;
            }
            Py_XDECREF(temp);
        } /* '%' */
    } /* until end */
    if (argidx < arglen && !dict) {
        PyErr_SetString(PyExc_TypeError,
                        "not all arguments converted during string formatting");
        goto error;
    }
    if (args_owned) {
        Py_DECREF(args);
    }
    if (_PyBytes_Resize(&result, reslen - rescnt))
        return NULL;
    return result;

#ifdef Py_USING_UNICODE
 unicode:
    if (args_owned) {
        Py_DECREF(args);
        args_owned = 0;
    }
    /* Fiddle args right (remove the first argidx arguments) */
    if (PyTuple_Check(orig_args) && argidx > 0) {
        PyObject *v;
        Py_ssize_t n = PyTuple_GET_SIZE(orig_args) - argidx;
        v = PyTuple_New(n);
        if (v == NULL)
            goto error;
        while (--n >= 0) {
            PyObject *w = PyTuple_GET_ITEM(orig_args, n + argidx);
            Py_INCREF(w);
            PyTuple_SET_ITEM(v, n, w);
        }
        args = v;
    } else {
        Py_INCREF(orig_args);
        args = orig_args;
    }
    args_owned = 1;
    /* Take what we have of the result and let the Unicode formatting
       function format the rest of the input. */
    rescnt = res - PyBytes_AS_STRING(result);
    if (_PyBytes_Resize(&result, rescnt))
        goto error;
    fmtcnt = PyBytes_GET_SIZE(format) - \
             (fmt - PyBytes_AS_STRING(format));
    format = PyUnicode_Decode(fmt, fmtcnt, NULL, NULL);
    if (format == NULL)
        goto error;
    v = PyUnicode_Format(format, args);
    Py_DECREF(format);
    if (v == NULL)
        goto error;
    /* Paste what we have (result) to what the Unicode formatting
       function returned (v) and return the result (or error) */
    w = PyUnicode_Concat(result, v);
    Py_DECREF(result);
    Py_DECREF(v);
    Py_DECREF(args);
    return w;
#endif /* Py_USING_UNICODE */

 error:
    Py_DECREF(result);
    if (args_owned) {
        Py_DECREF(args);
    }
    return NULL;
}

