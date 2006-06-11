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

#define MAX_DIMENSIONS 16

/** typecast_array_cleanup - remove the horrible [...]= stuff **/

static int
typecast_array_cleanup(char **str, int *len)
{
    int i, depth = 1;
    
    if ((*str)[0] != '[') return -1;
    
    for (i=1 ; depth > 0 && i < *len ; i++) {
        if ((*str)[i] == '[')
            depth += 1;
        else if ((*str)[i] == ']')
            depth -= 1;
    }
    if ((*str)[i] != '=') return -1;
    
    *str = &((*str)[i+1]);
    *len = *len - i - 1;
    return 0;
}

/** typecast_array_scan - scan a string looking for array items **/

#define ASCAN_ERROR -1
#define ASCAN_EOF    0
#define ASCAN_BEGIN  1
#define ASCAN_END    2
#define ASCAN_TOKEN  3
#define ASCAN_QUOTED 4

static int
typecast_array_tokenize(char *str, int strlength,
                        int *pos, char** token, int *length)
{
    /* FORTRAN glory */
    int i, j, q, b, l, res;

    Dprintf("typecast_array_tokenize: '%s', %d/%d",
            &str[*pos], *pos, strlength);

    /* we always get called with pos pointing at the start of a token, so a
       fast check is enough for ASCAN_EOF, ASCAN_BEGIN and ASCAN_END */
    if (*pos == strlength) {
        return ASCAN_EOF;
    }
    else if (str[*pos] == '{') {
        *pos += 1;
        return ASCAN_BEGIN;
    }
    else if (str[*pos] == '}') {
        *pos += 1;
        if (str[*pos] == ',')
            *pos += 1;
        return ASCAN_END;
    }

    /* now we start looking for the first unquoted ',' or '}', the only two
       tokens that can limit an array element */
    q = 0; /* if q is odd we're inside quotes */
    b = 0; /* if b is 1 we just encountered a backslash */
    res = ASCAN_TOKEN;
    
    for (i = *pos ; i < strlength ; i++) {
        switch (str[i]) {
        case '"':
            if (b == 0)
                q += 1;
            else
                b = 0;
            break;

        case '\\':
            res = ASCAN_QUOTED;
            if (b == 0)
                b = 1;
            else
                /* we're backslashing a backslash */
                b = 0;
            break;

        case '}':
        case ',':
            if (b == 0 && ((q&1) == 0))
                goto tokenize;
            break;

        default:
            /* reset the backslash counter */
            b = 0;
            break;
        }
    }

 tokenize:
    /* remove initial quoting character and calculate raw length */
    l = i - *pos;
    if (str[*pos] == '"') {
        *pos += 1;
        l -= 2;
    }

    if (res == ASCAN_QUOTED) { 
        char *buffer = PyMem_Malloc(l+1);
        if (buffer == NULL) return ASCAN_ERROR;

        *token = buffer;
        
        for (j = *pos; j < *pos+l; j++) {
            if (str[j] != '\\'
                || (j > *pos && str[j-1] == '\\'))
                *(buffer++) = str[j];
        }
        
        *buffer = '\0';
        *length = buffer - *token;
    }
    else {
        *token = &str[*pos];
        *length = l;
    }

    *pos = i;
    
    /* skip the comma and set position to the start of next token */
    if (str[i] == ',') *pos += 1;

    return res;
}

static int
typecast_array_scan(char *str, int strlength,
                    PyObject *curs, PyObject *base, PyObject *array)
{
    int state, length = 0, pos = 0;
    char *token;

    PyObject *stack[MAX_DIMENSIONS];
    int stack_index = 0;
    
    while (1) {
        token = NULL;
        state = typecast_array_tokenize(str, strlength, &pos, &token, &length);
        Dprintf("typecast_array_scan: state = %d, length = %d, token = '%s'",
                state, length, token);
        if (state == ASCAN_TOKEN || state == ASCAN_QUOTED) {
            PyObject *obj = typecast_cast(base, token, length, curs);

            /* before anything else we free the memory */
            if (state == ASCAN_QUOTED) PyMem_Free(token);
            if (obj == NULL) return 0;

            PyList_Append(array, obj);
            Py_DECREF(obj);
        }

        else if (state == ASCAN_BEGIN) {
            PyObject *sub = PyList_New(0);            
            if (sub == NULL) return 0;

            PyList_Append(array, sub);
            Py_DECREF(sub);

            if (stack_index == MAX_DIMENSIONS)
                return 0;

            stack[stack_index++] = array;
            array = sub;
        }
        
        else if (state == ASCAN_ERROR) {
            return 0;
        }

        else if (state == ASCAN_END) {
            if (--stack_index < 0)
                return 0;
            array = stack[stack_index];
        }

        else if (state ==  ASCAN_EOF)
            break;
    }

    return 1;
}


/** GENERIC - a generic typecaster that can be used when no special actions
    have to be taken on the single items **/
   
static PyObject *
typecast_GENERIC_ARRAY_cast(char *str, int len, PyObject *curs)
{
    PyObject *obj = NULL;
    PyObject *base = ((typecastObject*)((cursorObject*)curs)->caster)->bcast;
   
    Dprintf("typecast_GENERIC_ARRAY_cast: str = '%s', len = %d", str, len);

    if (str == NULL) {Py_INCREF(Py_None); return Py_None;}
    if (str[0] == '[')
        typecast_array_cleanup(&str, &len);
    if (str[0] != '{') {
        PyErr_SetString(Error, "array does not start with '{'");
        return NULL;
    }

    Dprintf("typecast_GENERIC_ARRAY_cast: str = '%s', len = %d", str, len);
    
    obj = PyList_New(0);

    /* scan the array skipping the first level of {} */
    if (typecast_array_scan(&str[1], len-2, curs, base, obj) == 0) {
        Py_DECREF(obj);
        obj = NULL;
    }
    
    return obj;
}

/** almost all the basic array typecasters are derived from GENERIC **/

#define typecast_LONGINTEGERARRAY_cast typecast_GENERIC_ARRAY_cast
#define typecast_INTEGERARRAY_cast typecast_GENERIC_ARRAY_cast
#define typecast_FLOATARRAY_cast typecast_GENERIC_ARRAY_cast
#define typecast_DECIMALARRAY_cast typecast_GENERIC_ARRAY_cast
#define typecast_STRINGARRAY_cast typecast_GENERIC_ARRAY_cast
#define typecast_UNICODEARRAY_cast typecast_GENERIC_ARRAY_cast
#define typecast_BOOLEANARRAY_cast typecast_GENERIC_ARRAY_cast
#define typecast_DATETIMEARRAY_cast typecast_GENERIC_ARRAY_cast
#define typecast_DATEARRAY_cast typecast_GENERIC_ARRAY_cast
#define typecast_TIMEARRAY_cast typecast_GENERIC_ARRAY_cast
#define typecast_INTERVALARRAY_cast typecast_GENERIC_ARRAY_cast
#define typecast_BINARYARRAY_cast typecast_GENERIC_ARRAY_cast
#define typecast_ROWIDARRAY_cast typecast_GENERIC_ARRAY_cast
