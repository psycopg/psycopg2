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

/** INTEGER - cast normal integers (4 bytes) to python int **/

static PyObject *
typecast_INTEGER_cast(char *s, int len, PyObject *curs)
{
    char buffer[12];
    
    if (s == NULL) {Py_INCREF(Py_None); return Py_None;}
    if (s[len] != '\0') {
        strncpy(buffer, s, len); buffer[len] = '\0';
        s = buffer;
    }
    return PyInt_FromString(s, NULL, 0);
}

/** LONGINTEGER - cast long integers (8 bytes) to python long **/

static PyObject *
typecast_LONGINTEGER_cast(char *s, int len, PyObject *curs)
{
    char buffer[24];
    
    if (s == NULL) {Py_INCREF(Py_None); return Py_None;}
    if (s[len] != '\0') {
        strncpy(buffer, s, len); buffer[len] = '\0';
        s = buffer;
    }
    return PyLong_FromString(s, NULL, 0);
}

/** FLOAT - cast floating point numbers to python float **/

static PyObject *
typecast_FLOAT_cast(char *s, int len, PyObject *curs)
{
    PyObject *str = NULL, *flo = NULL;
    char *pend;
    
    if (s == NULL) {Py_INCREF(Py_None); return Py_None;}
    str = PyString_FromStringAndSize(s, len);
    flo = PyFloat_FromString(str, &pend);
    Py_DECREF(str);
    return flo;
}

/** STRING - cast strings of any type to python string **/

static PyObject *
typecast_STRING_cast(char *s, int len, PyObject *curs)
{
    if (s == NULL) {Py_INCREF(Py_None); return Py_None;}
    return PyString_FromStringAndSize(s, len);
}

/** UNICODE - cast strings of any type to a python unicode object **/

static PyObject *
typecast_UNICODE_cast(char *s, int len, PyObject *curs)
{
    PyObject *enc;

    if (s == NULL) {Py_INCREF(Py_None); return Py_None;}

    enc = PyDict_GetItemString(psycoEncodings,
                               ((cursorObject*)curs)->conn->encoding);
    if (enc) {
        return PyUnicode_Decode(s, len, PyString_AsString(enc), NULL);
    }
    else {
       PyErr_Format(InterfaceError,
                    "can't decode into unicode string from %s",
                    ((cursorObject*)curs)->conn->encoding);
       return NULL; 
    }
}

/** BOOLEAN - cast boolean value into right python object **/

static PyObject *
typecast_BOOLEAN_cast(char *s, int len, PyObject *curs)
{
    PyObject *res;

    if (s == NULL) {Py_INCREF(Py_None); return Py_None;}

    if (s[0] == 't')
        res = Py_True;
    else
        res = Py_False;

    Py_INCREF(res);
    return res;
}

/** DECIMAL - cast any kind of number into a Python Decimal object **/

#ifdef HAVE_DECIMAL
static PyObject *
typecast_DECIMAL_cast(char *s, int len, PyObject *curs)
{
    PyObject *res = NULL;
    char *buffer;
    
    if (s == NULL) {Py_INCREF(Py_None); return Py_None;}

    if ((buffer = PyMem_Malloc(len+1)) == NULL)
        PyErr_NoMemory();
    strncpy(buffer, s, len); buffer[len] = '\0';
    res = PyObject_CallFunction(decimalType, "s", buffer);
    PyMem_Free(buffer);

    return res;
}
#else
#define typecast_DECIMAL_cast  typecast_FLOAT_cast
#endif
     
/* some needed aliases */
#define typecast_NUMBER_cast   typecast_FLOAT_cast
#define typecast_ROWID_cast    typecast_INTEGER_cast
