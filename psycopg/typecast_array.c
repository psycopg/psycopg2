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


/** typecast_array_scan - scan a string looking for array items **/

#define ASCAN_ERROR -1
#define ASCAN_EOF    0
#define ASCAN_BEGIN  1
#define ASCAN_END    2
#define ASCAN_TOKEN  3
#define ASCAN_QUOTED 4

static int
typecast_array_tokenize(unsigned char *str, int strlength,
                        int *pos, unsigned char** token, int *length)
{
    int i, l, res = ASCAN_TOKEN;
    int qs = 0; /* 2 = in quotes, 1 = quotes closed */
    
    /* first we check for quotes, used when the content of the item contains
       special or quoted characters */
    
    if (str[*pos] == '"') {
        qs = 2;
        *pos += 1;
    }

    Dprintf("typecast_array_tokenize: '%s'; %d/%d",
            &str[*pos], *pos, strlength);

    for (i = *pos ; i < strlength ; i++) {
        switch (str[i]) {
        case '{':
            *pos = i+1;
            return ASCAN_BEGIN;

        case '}':
            /* we tokenize the last item in the array and then return it to
               the user togheter with the closing bracket marker */
            res = ASCAN_END;
            goto tokenize;

        case '"':
            /* this will close the quoting only if the previous character was
               NOT a backslash */
            if (qs == 2 && str[i-1] != '\\') qs = 1;
            continue;
               
        case '\\':
            /* something has been quoted, sigh, we'll need a copy buffer */
            res = ASCAN_QUOTED;
            continue;
            
        case ',':
            /* if we're inside quotes we use the comma as a normal char */
            if (qs == 2)
                continue;
            else
                goto tokenize;
        }
    }

    res = ASCAN_EOF;
    
 tokenize:
    l = i - *pos - qs;
    
    /* if res is ASCAN_QUOTED we need to copy the string to a newly allocated
       buffer and return it */
    if (res == ASCAN_QUOTED) {
        unsigned char *buffer = PyMem_Malloc(l+1);
        if (buffer == NULL) return ASCAN_ERROR;

        *token = buffer;
        
        for (i = *pos; i < l+*pos; i++) {
            if (str[i] != '\\')
                *(buffer++) = str[i];
        }
        *buffer = '\0';
        *length = (int)buffer - (int)*token;
        *pos = i+2;
    }
    else {
        *token = &str[*pos];
        *length = l;
        *pos = i+1;
        if (res == ASCAN_END && str[*pos] == ',')
            *pos += 1; /* skip both the bracket and the comma */
    }
    
    return res;
}

static int
typecast_array_scan(unsigned char *str, int strlength,
                    PyObject *curs, PyObject *base, PyObject *array)
{
    int state, length, bracket = 0, pos = 0;
    unsigned char *token;

    PyObject *stack[MAX_DIMENSIONS];
    int stack_index = 0;
    
    while (1) {
        state = typecast_array_tokenize(str, strlength, &pos, &token, &length);
        if (state == ASCAN_TOKEN
            || state == ASCAN_QUOTED 
            || (state ==  ASCAN_EOF && bracket == 0)
            || (state == ASCAN_END && bracket == 0)) {
            
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

        /* reset the closing bracket marker just before cheking for ASCAN_END:
           this is to make sure we don't mistake two closing brackets for an
           empty item */
        bracket = 0;
        
        if (state == ASCAN_END) {
            if (--stack_index < 0)
                return 0;
            array = stack[stack_index];
            bracket = 1;
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

    Dprintf("typecast_GENERIC_ARRAY_cast: scanning %s", str);
    
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

