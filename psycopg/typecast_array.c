/* typecast_array.c - array typecasters
 *
 * Copyright (C) 2005 Federico Di Gregorio <fog@debian.org>
 *
 * This file is part of the psycopg module.
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


/* the pointer to the datetime module API is initialized by the module init
   code, we just need to grab it */
extern PyObject* pyDateTimeModuleP;
extern PyObject *pyDateTypeP;
extern PyObject *pyTimeTypeP;
extern PyObject *pyDateTimeTypeP;
extern PyObject *pyDeltaTypeP;

/** LONGINTEGERARRAY and INTEGERARRAY - cast integers arrays **/

static PyObject *
typecast_INTEGERARRAY_cast(unsigned char *str, int len, PyObject *curs)
{
    PyObject* obj = NULL;
     
    if (str == NULL) {Py_INCREF(Py_None); return Py_None;}

    return obj;
}

#define typecast_LONGINTEGERARRAY_cast typecast_INTEGERARRAY_cast

/** STRINGARRAY - cast integers arrays **/

static PyObject *
typecast_STRINGARRAY_cast(unsigned char *str, int len, PyObject *curs)
{
    PyObject* obj = NULL;
     
    if (str == NULL) {Py_INCREF(Py_None); return Py_None;}

    return obj;
}
