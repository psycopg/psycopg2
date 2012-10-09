/* python.h - python version compatibility stuff
 *
 * Copyright (C) 2003-2010 Federico Di Gregorio <fog@debian.org>
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

#ifndef PSYCOPG_PYTHON_H
#define PSYCOPG_PYTHON_H 1

#include <structmember.h>
#if PY_MAJOR_VERSION < 3
#include <stringobject.h>
#endif

#if PY_VERSION_HEX < 0x02040000
#  error "psycopg requires Python >= 2.4"
#endif

#if PY_VERSION_HEX < 0x02050000
/* Function missing in Py 2.4 */
#define PyErr_WarnEx(cat,msg,lvl) PyErr_Warn(cat,msg)
#endif

#if PY_VERSION_HEX < 0x02050000 && !defined(PY_SSIZE_T_MIN)
  typedef int Py_ssize_t;
  #define PY_SSIZE_T_MIN INT_MIN
  #define PY_SSIZE_T_MAX INT_MAX
  #define PY_FORMAT_SIZE_T ""
  #define PyInt_FromSsize_t(x) PyInt_FromLong((x))

  #define lenfunc inquiry
  #define ssizeargfunc intargfunc
  #define readbufferproc getreadbufferproc
  #define writebufferproc getwritebufferproc
  #define segcountproc getsegcountproc
  #define charbufferproc getcharbufferproc

  #define CONV_CODE_PY_SSIZE_T "i"
#else
  #define CONV_CODE_PY_SSIZE_T "n"
#endif

/* hash() return size changed around version 3.2a4 on 64bit platforms.  Before
 *   this, the return size was always a long, regardless of arch.  ~3.2 
 *   introduced the Py_hash_t & Py_uhash_t typedefs with the resulting sizes
 *   based upon arch. */
#if PY_VERSION_HEX < 0x030200A4
typedef long Py_hash_t;
typedef unsigned long Py_uhash_t;
#endif

/* Macros defined in Python 2.6 */
#ifndef Py_REFCNT
#define Py_REFCNT(ob)           (((PyObject*)(ob))->ob_refcnt)
#define Py_TYPE(ob)             (((PyObject*)(ob))->ob_type)
#define Py_SIZE(ob)             (((PyVarObject*)(ob))->ob_size)
#define PyVarObject_HEAD_INIT(x,n) PyObject_HEAD_INIT(x) n,
#endif

/* Missing at least in Python 2.4 */
#ifndef Py_MEMCPY
#define Py_MEMCPY memcpy
#endif

/* FORMAT_CODE_PY_SSIZE_T is for Py_ssize_t: */
#define FORMAT_CODE_PY_SSIZE_T "%" PY_FORMAT_SIZE_T "d"

/* FORMAT_CODE_SIZE_T is for plain size_t, not for Py_ssize_t: */
#ifdef _MSC_VER
  /* For MSVC: */
  #define FORMAT_CODE_SIZE_T "%Iu"
#else
  /* C99 standard format code: */
  #define FORMAT_CODE_SIZE_T "%zu"
#endif

/* Abstract from text type. Only supported for ASCII and UTF-8 */
#if PY_MAJOR_VERSION < 3
#define Text_Type PyString_Type
#define Text_Check(s) PyString_Check(s)
#define Text_Format(f,a) PyString_Format(f,a)
#define Text_FromUTF8(s) PyString_FromString(s)
#define Text_FromUTF8AndSize(s,n) PyString_FromStringAndSize(s,n)
#else
#define Text_Type PyUnicode_Type
#define Text_Check(s) PyUnicode_Check(s)
#define Text_Format(f,a) PyUnicode_Format(f,a)
#define Text_FromUTF8(s) PyUnicode_FromString(s)
#define Text_FromUTF8AndSize(s,n) PyUnicode_FromStringAndSize(s,n)
#endif

#if PY_MAJOR_VERSION > 2
#define PyInt_Type             PyLong_Type
#define PyInt_Check            PyLong_Check
#define PyInt_AsLong           PyLong_AsLong
#define PyInt_FromLong         PyLong_FromLong
#define PyInt_FromSsize_t      PyLong_FromSsize_t
#define PyString_FromFormat    PyUnicode_FromFormat
#define Py_TPFLAGS_HAVE_ITER   0L
#define Py_TPFLAGS_HAVE_RICHCOMPARE 0L
#define Py_TPFLAGS_HAVE_WEAKREFS 0L
#ifndef PyNumber_Int
#define PyNumber_Int           PyNumber_Long
#endif
#endif  /* PY_MAJOR_VERSION > 2 */

#if PY_MAJOR_VERSION < 3
#define Bytes_Type PyString_Type
#define Bytes_Check PyString_Check
#define Bytes_CheckExact PyString_CheckExact
#define Bytes_AS_STRING PyString_AS_STRING
#define Bytes_GET_SIZE PyString_GET_SIZE
#define Bytes_Size PyString_Size
#define Bytes_AsString PyString_AsString
#define Bytes_AsStringAndSize PyString_AsStringAndSize
#define Bytes_FromString PyString_FromString
#define Bytes_FromStringAndSize PyString_FromStringAndSize
#define Bytes_FromFormat PyString_FromFormat
#define Bytes_ConcatAndDel PyString_ConcatAndDel
#define _Bytes_Resize _PyString_Resize

#else

#define Bytes_Type PyBytes_Type
#define Bytes_Check PyBytes_Check
#define Bytes_CheckExact PyBytes_CheckExact
#define Bytes_AS_STRING PyBytes_AS_STRING
#define Bytes_GET_SIZE PyBytes_GET_SIZE
#define Bytes_Size PyBytes_Size
#define Bytes_AsString PyBytes_AsString
#define Bytes_AsStringAndSize PyBytes_AsStringAndSize
#define Bytes_FromString PyBytes_FromString
#define Bytes_FromStringAndSize PyBytes_FromStringAndSize
#define Bytes_FromFormat PyBytes_FromFormat
#define Bytes_ConcatAndDel PyBytes_ConcatAndDel
#define _Bytes_Resize _PyBytes_Resize

#endif

HIDDEN PyObject *Bytes_Format(PyObject *format, PyObject *args);

/* Mangle the module name into the name of the module init function */
#if PY_MAJOR_VERSION > 2
#define INIT_MODULE(m) PyInit_ ## m
#else
#define INIT_MODULE(m) init ## m
#endif

#endif /* !defined(PSYCOPG_PYTHON_H) */
