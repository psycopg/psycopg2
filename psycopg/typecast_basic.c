/* pgcasts_basic.c - basic typecasting functions to python types
 *
 * Copyright (C) 2001-2003 Federico Di Gregorio <fog@debian.org>
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

#include <libpq-fe.h>

/** INTEGER - cast normal integers (4 bytes) to python int **/

static PyObject *
typecast_INTEGER_cast(PyObject *s, PyObject *curs)
{
    if (s == Py_None) {Py_INCREF(s); return s;}
    return PyNumber_Int(s);
}

/** LONGINTEGER - cast long integers (8 bytes) to python long **/

static PyObject *
typecast_LONGINTEGER_cast(PyObject *s, PyObject *curs)
{
    if (s == Py_None) {Py_INCREF(s); return s;}
    return PyNumber_Long(s);
}

/** FLOAT - cast floating point numbers to python float **/

static PyObject *
typecast_FLOAT_cast(PyObject *s, PyObject *curs)
{
    if (s == Py_None) {Py_INCREF(s); return s;}
    return PyNumber_Float(s);
}

/** STRING - cast strings of any type to python string **/

static PyObject *
typecast_STRING_cast(PyObject *s, PyObject *curs)
{
    Py_INCREF(s);
    return s;
}

/** UNICODE - cast strings of any type to a python unicode object **/

static PyObject *
typecast_UNICODE_cast(PyObject *s, PyObject *curs)
{
    PyObject *enc;
    
    if (s == Py_None) {Py_INCREF(s); return s;}

    enc = PyDict_GetItemString(psycoEncodings,
                               ((cursorObject*)curs)->conn->encoding);
    if (enc) {
        return PyUnicode_Decode(PyString_AsString(s),
                                PyString_Size(s),
                                PyString_AsString(enc),
                                NULL);
    }
    else {
       PyErr_Format(InterfaceError,
                    "can't decode into unicode string from %s",
                    ((cursorObject*)curs)->conn->encoding);
       return NULL; 
    }
}

/** BINARY - cast a binary string into a python string **/

/* the function typecast_BINARY_cast_unescape is used when libpq does not
   provide PQunescapeBytea: it convert all the \xxx octal sequences to the
   proper byte value */

#ifdef PSYCOPG_OWN_QUOTING
static unsigned char *
typecast_BINARY_cast_unescape(unsigned char *str, size_t *to_length)
{
    char *dstptr, *dststr;
    int len, i;

    len = strlen(str);
    dststr = (char*)calloc(len, sizeof(char));
    dstptr = dststr;

    if (dststr == NULL) return NULL;

    Py_BEGIN_ALLOW_THREADS;
   
    for (i = 0; i < len; i++) {
        if (str[i] == '\\') {
            if ( ++i < len) {
                if (str[i] == '\\') {
                    *dstptr = '\\';
                }
                else {
                    *dstptr = 0;
                    *dstptr |= (str[i++] & 7) << 6;
                    *dstptr |= (str[i++] & 7) << 3;
                    *dstptr |= (str[i] & 7);
                }
            }
        }
        else {
            *dstptr = str[i];
        }
        dstptr++;
    }
    
    Py_END_ALLOW_THREADS;

    *to_length = (size_t)(dstptr-dststr);
    
    return dststr;
}

#define PQunescapeBytea typecast_BINARY_cast_unescape
#endif
    
static PyObject *
typecast_BINARY_cast(PyObject *s, PyObject *curs)
{
    PyObject *res;
    unsigned char *str;
    size_t len;

    if (s == Py_None) {Py_INCREF(s); return s;}
   
    str = PQunescapeBytea(PyString_AS_STRING(s), &len);
    Dprintf("typecast_BINARY_cast: unescaped %d bytes", len);
    
    /* TODO: using a PyBuffer would make this a zero-copy operation but we'll
       need to define our own buffer-derived object to keep a reference to the
       memory area: does it buy it?

       res = PyBuffer_FromMemory((void*)str, len); */
    res = PyString_FromStringAndSize(str, len);
    free(str);
    
    return res;
}

/** BOOLEAN - cast boolean value into right python object **/

static PyObject *
typecast_BOOLEAN_cast(PyObject *s, PyObject *curs)
{
    PyObject *res;
    
    if (s == Py_None) {Py_INCREF(s); return s;}

    if (PyString_AS_STRING(s)[0] == 't')
        res = Py_True;
    else
        res = Py_False;

    Py_INCREF(res);
    return res;
}

/** DECIMAL - cast any kind of number into a Python Decimal object **/

#ifdef HAVE_DECIMAL
static PyObject *
typecast_DECIMAL_cast(PyObject *s, PyObject *curs)
{
    if (s == Py_None) {Py_INCREF(s); return s;}
    return PyObject_CallFunction(decimalType, "O", s);
}
#else
#define typecast_DECIMAL_cast  typecast_FLOAT_cast
#endif
     
/* some needed aliases */
#define typecast_NUMBER_cast   typecast_FLOAT_cast
#define typecast_ROWID_cast    typecast_INTEGER_cast
