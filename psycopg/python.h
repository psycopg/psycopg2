/* python.h - python version compatibility stuff
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

#ifndef PSYCOPG_PYTHON_H
#define PSYCOPG_PYTHON_H 1

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <structmember.h>

#if PY_VERSION_HEX < 0x02040000
#  error "psycopg requires Python >= 2.4"
#endif

#if PY_VERSION_HEX < 0x02050000 && !defined(PY_SSIZE_T_MIN)
  typedef int Py_ssize_t;
  #define PY_SSIZE_T_MIN INT_MIN
  #define PY_SSIZE_T_MAX INT_MAX
  #define PY_FORMAT_SIZE_T ""

  #define readbufferproc getreadbufferproc
  #define writebufferproc getwritebufferproc
  #define segcountproc getsegcountproc
  #define charbufferproc getcharbufferproc

  #define CONV_CODE_PY_SSIZE_T "i"
#else
  #define CONV_CODE_PY_SSIZE_T "n"
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

#endif /* !defined(PSYCOPG_PYTHON_H) */
