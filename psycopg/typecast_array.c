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


/** typecast_array_scan - scan a string looking for array items **/

#define ASCAN_EOF    0
#define ASCAN_BEGIN  1
#define ASCAN_END    2
#define ASCAN_TOKEN  3
#define ASCAN_QUOTED 4

static int
typecast_array_tokenize(unsigned char *str, int strlength,
                        int *pos, unsigned char** token, int *length)
{
    int i;
    int quoted = 0;

    /* first we check for quotes, used when the content of the item contains
       special or quoted characters */
    if (str[*pos] == '"') {
        quoted = 1;
        *pos += 1;
    }

    for (i = *pos ; i < strlength ; i++) {
        switch (str[i]) {
        case '{':
            *pos = i+1;
            return ASCAN_BEGIN;

        case '}':
            *pos = i+1;
            return ASCAN_END;

        case ',':
            *token = &str[*pos];
            *length = i - *pos;
            if (quoted == 1)
                *length -= 1;
            *pos = i+1;
            return ASCAN_TOKEN;

        default:
            /* nothing to do right now */
            break;
        }
    }

    *token = &str[*pos];
    *length = i - *pos;
    if (quoted == 1)
        *length -= 1;
    
    return ASCAN_EOF;
}

static int
typecast_array_scan(unsigned char *str, int strlength,
                    PyObject *curs, PyObject *base, PyObject *array)
{
    int state, length, pos = 0;
    unsigned char *token;
    
    while (1) {
        state = typecast_array_tokenize(str, strlength, &pos, &token, &length);
        
        if (state == ASCAN_TOKEN || state ==  ASCAN_EOF) {
            PyObject *obj = typecast_cast(base, token, length, curs);
            if (obj == NULL) return 0;
            PyList_Append(array, obj);
            Py_DECREF(obj);
        }
        else {
            Dprintf("** RECURSION (not supported right now)!!");
        }

        if (state ==  ASCAN_EOF) break;
    }

    return 1;
}


/** GENERIC - a generic typecaster that can be used when no special actions
    have to be taken on the single items **/
   
static PyObject *
typecast_GENERIC_ARRAY_cast(unsigned char *str, int len, PyObject *curs)
{
    PyObject *obj = NULL;
    PyObject *base = ((typecastObject*)((cursorObject*)curs)->caster)->bcast;
    
    if (str == NULL) {Py_INCREF(Py_None); return Py_None;}
    if (str[0] != '{') {
        PyErr_SetString(Error, "array does not start with '{'");
        return NULL;
    }
    
    obj = PyList_New(0);

    /* scan the array skipping the first level of {} */
    if (typecast_array_scan(&str[1], len-2, curs, base, obj) == 0) {
        Py_DECREF(obj);
        obj = NULL;
    }
    
    return obj;
}

/** LONGINTEGERARRAY and INTEGERARRAY - cast integers arrays **/

#define typecast_LONGINTEGERARRAY_cast typecast_GENERIC_ARRAY_cast
#define typecast_INTEGERARRAY_cast typecast_GENERIC_ARRAY_cast

/** STRINGARRAY - cast integers arrays **/

#define typecast_STRINGARRAY_cast typecast_GENERIC_ARRAY_cast

