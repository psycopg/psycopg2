/* bytes_format.c - bytes-oriented version of PyString_Format
 *
 * Copyright (C) 2010-2019 Daniele Varrazzo <daniele.varrazzo@gmail.com>
 * Copyright (C) 2020-2021 The Psycopg Team
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
 * Python 2.7.1. The function is altered to be used with both Python 2 strings
 * and Python 3 bytes and is stripped of the support of formats different than
 * 's'. Original license follows.
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
 * i.e., "Copyright (c) 2001-2019, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010
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
#include "pyport.h"

/* Helpers for formatstring */

BORROWED Py_LOCAL_INLINE(PyObject *)
getnextarg(PyObject *args, Py_ssize_t arglen, Py_ssize_t *p_argidx) {
    Py_ssize_t argidx = *p_argidx;
    if (argidx < arglen) {
        (*p_argidx)++;
        if (arglen < 0) return args;
        else return PyTuple_GetItem(args, argidx);
    }
    return NULL;
}

/*
    for function getnextarg：
    I delete the line including 'raise error', for making this func a iterator
    just used to fill in the arguments array
*/

/* wrapper around _Bytes_Resize offering normal Python call semantics */

STEALS(1)
Py_LOCAL_INLINE(PyObject *)
resize_bytes(PyObject *b, Py_ssize_t newsize) {
    if (0 == _Bytes_Resize(&b, newsize)) return b;
    else return NULL;
}

PyObject *Bytes_Format(PyObject *format, PyObject *args, char place_holder) {
    char *fmt, *res;                    //array pointer of format, and array pointer of result
    Py_ssize_t arglen, argidx;          //length of arguments array, and index of arguments(when processing args_list)
    Py_ssize_t reslen, rescnt, fmtcnt;  //rescnt: blank space in result; reslen: the total length of result; fmtcnt: length of format
    int args_owned = 0;                 //args is valid or invalid(or maybe refcnt), 0 for invalid，1 otherwise
    PyObject *result;                   //function's return value
    PyObject *dict = NULL;              //dictionary
    PyObject *args_value = NULL;        //every argument store in it after parse
    char **args_list = NULL;            //arguments list as char **
    char *args_buffer = NULL;           //Bytes_AS_STRING(args_value)
    Py_ssize_t * args_len = NULL;       //every argument's length in args_list
    int args_id = 0;                    //index of arguments(when generating result)
    int index_type = 0;                 //if exists $number, it will be 1, otherwise 0
    
    if (format == NULL || !Bytes_Check(format) || args == NULL) {   //check if arguments are valid
        PyErr_SetString(PyExc_SystemError, "bad argument to internal function");
        return NULL;
    }
    fmt = Bytes_AS_STRING(format);      //get pointer of format
    fmtcnt = Bytes_GET_SIZE(format);    //get length of format
    reslen = rescnt = 1;
    while (reslen <= fmtcnt) {          //when space is not enough, double it's size
        reslen *= 2;
        rescnt *= 2;
    }
    result = Bytes_FromStringAndSize((char *)NULL, reslen); 
    if (result == NULL) return NULL;
    res = Bytes_AS_STRING(result);
    if (PyTuple_Check(args)) {          //check if arguments are sequences
        arglen = PyTuple_GET_SIZE(args);
        argidx = 0;
    }
    else {                              //if no, then this two are of no importance
        arglen = -1;
        argidx = -2;
    }
    if (Py_TYPE(args)->tp_as_mapping && !PyTuple_Check(args) && !PyObject_TypeCheck(args, &Bytes_Type)) {   //check if args is dict
        dict = args;
        //Py_INCREF(dict);
    }
    while (--fmtcnt >= 0) {             //scan the format
        if (*fmt != '%') {              //if not %, pass it(for the special format '%(name)s')
            if (--rescnt < 0) {
                rescnt = reslen;        //double the space
                reslen *= 2;
                if (!(result = resize_bytes(result, reslen))) {
                    return NULL;
                }
                res = Bytes_AS_STRING(result) + reslen - rescnt;//calculate offset
                --rescnt;
            }
            *res++ = *fmt++;            //copy
        }
        else {
            /* Got a format specifier */
            fmt++;
            if (*fmt == '(') {
                char *keystart;         //begin pos of left bracket
                Py_ssize_t keylen;      //length of content in bracket
                PyObject *key;
                int pcount = 1;         //counter of left bracket
                Py_ssize_t length = 0;
                if (dict == NULL) {
                    PyErr_SetString(PyExc_TypeError, "format requires a mapping");
                    goto error;
                }
                ++fmt;
                --fmtcnt;
                keystart = fmt;
                /* Skip over balanced parentheses */
                while (pcount > 0 && --fmtcnt >= 0) {       //find the matching right bracket
                    if (*fmt == ')') --pcount;
                    else if (*fmt == '(') ++pcount;
                    fmt++;
                }
                keylen = fmt - keystart - 1;
                if (fmtcnt < 0 || pcount > 0 || *(fmt++) != 's') {             //not found, raise an error
                    PyErr_SetString(PyExc_ValueError, "incomplete format key");
                    goto error;
                }
                --fmtcnt;
                key = Text_FromUTF8AndSize(keystart, keylen);//get key
                if (key == NULL) goto error;
                if (args_owned) {                           //if refcnt > 0, then release
                    Py_DECREF(args);
                    args_owned = 0;
                }
                args = PyObject_GetItem(dict, key);         //get value with key
                Py_DECREF(key);
                if (args == NULL) goto error;
                if (!Bytes_CheckExact(args)) {
                    PyErr_Format(PyExc_ValueError, "only bytes values expected, got %s", Py_TYPE(args)->tp_name);   //raise error, but may have bug
                    goto error;
                }
                args_buffer = Bytes_AS_STRING(args);        //temporary buffer
                length = Bytes_GET_SIZE(args);
                if (rescnt < length) {
                    while (rescnt < length) {
                        rescnt += reslen;
                        reslen *= 2;
                    }
                    if ((result = resize_bytes(result, reslen)) == NULL) goto error;
                }
                res = Bytes_AS_STRING(result) + reslen - rescnt;
                Py_MEMCPY(res, args_buffer, length);
                rescnt -= length;
                res += length;
                args_owned = 1;
                arglen = -1;    //exists place holder as "%(name)s", set these arguments to invalid
                argidx = -2;
            }
        } /* '%' */
    } /* until end */
    
    if (dict) {                                             //if args' type is dict, the func ends
        if (args_owned) Py_DECREF(args);
        if (!(result = resize_bytes(result, reslen - rescnt))) return NULL;         //resize and return
        if (place_holder != '%') {
            PyErr_SetString(PyExc_TypeError, "place holder only expect %% when using dict");
            goto error;
        }
        return result;
    }

    args_list = (char **)malloc(sizeof(char *) * arglen);                           //buffer
    args_len = (Py_ssize_t *)malloc(sizeof(Py_ssize_t *) * arglen);                 //length of every argument
    while ((args_value = getnextarg(args, arglen, &argidx)) != NULL) {              //stop when receive NULL
        Py_ssize_t length = 0;
        if (!Bytes_CheckExact(args_value)) {
            PyErr_Format(PyExc_ValueError, "only bytes values expected, got %s", Py_TYPE(args_value)->tp_name);  //may have bug
            goto error;
        }
        Py_INCREF(args_value);                              //increase refcnt
        args_buffer = Bytes_AS_STRING(args_value);
        length = Bytes_GET_SIZE(args_value);
        //printf("type: %s, len: %d, value: %s\n", Py_TYPE(args_value)->tp_name, length, args_buffer);
        args_len[argidx - 1] = length;
        args_list[argidx - 1] = (char *)malloc(sizeof(char *) * (length + 1));
        Py_MEMCPY(args_list[argidx - 1], args_buffer, length);
        args_list[argidx - 1][length] = '\0';
        Py_XDECREF(args_value);
    }

    fmt = Bytes_AS_STRING(format);      //get pointer of format
    fmtcnt = Bytes_GET_SIZE(format);    //get length of format
    reslen = rescnt = 1;
    while (reslen <= fmtcnt) {
        reslen *= 2;
        rescnt *= 2;
    }
    if ((result = resize_bytes(result, reslen)) == NULL) goto error;
    res = Bytes_AS_STRING(result);
    memset(res, 0, sizeof(char) * reslen);
    
    while (*fmt != '\0') {
        if (*fmt != place_holder) {     //not place holder, pass it
            if (!rescnt) {
                rescnt += reslen;
                reslen *= 2;
                if ((result = resize_bytes(result, reslen)) == NULL) goto error;
                res = Bytes_AS_STRING(result) + reslen - rescnt;
            }
            *(res++) = *(fmt++);
            --rescnt;
            continue;
        }
        if (*fmt == '%') {
            char c = *(++fmt);
            if (c == '\0') {            //if there is nothing after '%', raise an error
                PyErr_SetString(PyExc_ValueError, "incomplete format");
                goto error;
            }
            else if (c == '%') {        //'%%' will be transfered to '%'
                if (!rescnt) {
                    rescnt += reslen;
                    reslen *= 2;
                    if ((result = resize_bytes(result, reslen)) == NULL) goto error;
                    res = Bytes_AS_STRING(result) + reslen - rescnt;
                }
                *res = c;
                --rescnt;
                ++res;
                ++fmt;
            }
            else if (c == 's') {                //'%s', replace it with corresponding string
                if (args_id >= arglen) {        //index is out of bound
                    PyErr_SetString(PyExc_TypeError, "arguments not enough during string formatting");
                    goto error;
                }
                if (rescnt < args_len[args_id]) {
                    while (rescnt < args_len[args_id]) {
                        rescnt += reslen;
                        reslen *= 2;
                    }
                    if ((result = resize_bytes(result, reslen)) == NULL) goto error;
                    res = Bytes_AS_STRING(result) + reslen - rescnt;
                }
                Py_MEMCPY(res, args_list[args_id], args_len[args_id]);
                rescnt -= args_len[args_id];
                res += args_len[args_id];
                ++args_id;
                ++fmt;
            }
            else {                          //not support the character currently
                PyErr_Format(PyExc_ValueError, "unsupported format character '%c' (0x%x) "
                "at index " FORMAT_CODE_PY_SSIZE_T,
                c, c,
                (Py_ssize_t)(fmt - 1 - Bytes_AS_STRING(format)));
                goto error;
            }
            continue;
        }
        if (*fmt == '$') {
            char c = *(++fmt);
            if (c == '\0') {            //if there is nothing after '$', raise an error
                PyErr_SetString(PyExc_ValueError, "incomplete format");
                goto error;
            }
            else if (c == '$') {        //'$$' will be transfered to'$'
                if (!rescnt) {          //resize buffer
                    rescnt += reslen;
                    reslen *= 2;
                    if ((result = resize_bytes(result, reslen)) == NULL) goto error;
                    res = Bytes_AS_STRING(result) + reslen - rescnt;
                }
                *res = c;
                --rescnt;
                ++res;
                ++fmt;
            }
            else if (isdigit(c)) {      //represents '$number'
                int index = 0;
                index_type = 1;
                while (isdigit(*fmt)) {
                    index = index * 10 + (*fmt) -'0';
                    ++fmt;
                }
                if ((index > arglen) || (index <= 0)) {       //invalid index
                    PyErr_SetString(PyExc_ValueError, "invalid index");
                    goto error;
                }
                --index;
                if (rescnt < args_len[index]) {
                    while (rescnt < args_len[index]) {
                        rescnt += reslen;
                        reslen *= 2;
                    }
                    if ((result = resize_bytes(result, reslen)) == NULL) goto error;
                    res = Bytes_AS_STRING(result) + reslen - rescnt;
                }
                Py_MEMCPY(res, args_list[index], args_len[index]);
                rescnt -= args_len[index];
                res += args_len[index];
            }
            else {                      //invalid place holder
                PyErr_Format(PyExc_ValueError, "unsupported format character '%c' (0x%x) "
                "at index " FORMAT_CODE_PY_SSIZE_T,
                c, c,
                (Py_ssize_t)(fmt - 1 - Bytes_AS_STRING(format)));
                goto error;
            }
        }
    }
    if ((args_id < arglen) && (!dict) && (!index_type)) {    //not all arguments are used
        PyErr_SetString(PyExc_TypeError, "not all arguments converted during string formatting");
        goto error;
    }
    if (args_list != NULL) {
        while (--argidx >= 0) free(args_list[argidx]);
        free(args_list);
        free(args_len);
    }
    if (args_owned) Py_DECREF(args);
    if (!(result = resize_bytes(result, reslen - rescnt))) return NULL;        //resize
    return result;

error:
    if (args_list != NULL) {    //release all the refcnt
        while (--argidx >= 0) free(args_list[argidx]);
        free(args_list);
        free(args_len);
    }
    Py_DECREF(result);
    if (args_owned) Py_DECREF(args);
    return NULL;
}