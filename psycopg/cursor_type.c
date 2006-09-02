/* cursor_type.c - python interface to cursor objects
 *
 * Copyright (C) 2003-2004 Federico Di Gregorio <fog@debian.org>
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
 * You should have received a copy of the GNU General Public Likcense
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <Python.h>
#include <structmember.h>
#include <string.h>

#define PSYCOPG_MODULE
#include "psycopg/config.h"
#include "psycopg/python.h"
#include "psycopg/psycopg.h"
#include "psycopg/cursor.h"
#include "psycopg/connection.h"
#include "psycopg/pqpath.h"
#include "psycopg/typecast.h"
#include "psycopg/microprotocols.h"
#include "psycopg/microprotocols_proto.h"
#include "pgversion.h"

extern PyObject *pyPsycopgTzFixedOffsetTimezone;


/** DBAPI methods **/

/* close method - close the cursor */

#define psyco_curs_close_doc \
"close() -- Close the cursor."

static PyObject *
psyco_curs_close(cursorObject *self, PyObject *args)
{
    if (!PyArg_ParseTuple(args, "")) return NULL;
    
    EXC_IF_CURS_CLOSED(self);
    
    if (self->name != NULL) {
        char buffer[128];
        
        EXC_IF_NO_MARK(self);
        PyOS_snprintf(buffer, 127, "CLOSE %s", self->name);
        if (pq_execute(self, buffer, 0) == -1) return NULL;
    }

    self->closed = 1;
    Dprintf("psyco_curs_close: cursor at %p closed", self);

    Py_INCREF(Py_None);
    return Py_None;
}


/* execute method - executes a query */

/* mogrify a query string and build argument array or dict */

static int
_mogrify(PyObject *var, PyObject *fmt, connectionObject *conn, PyObject **new)
{
    PyObject *key, *value, *n, *item;
    char *d, *c;
    int index = 0, force = 0;

    /* from now on we'll use n and replace its value in *new only at the end,
       just before returning. we also init *new to NULL to exit with an error
       if we can't complete the mogrification */
    n = *new = NULL;
    c = PyString_AsString(fmt);

    while(*c) {
        /* handle plain percent symbol in format string */
        if (c[0] == '%' && c[1] == '%') {
            c+=2; force = 1;
        }
        
        /* if we find '%(' then this is a dictionary, we:
           1/ find the matching ')' and extract the key name
           2/ locate the value in the dictionary (or return an error)
           3/ mogrify the value into something usefull (quoting)...
           4/ ...and add it to the new dictionary to be used as argument
        */
        else if (c[0] == '%' && c[1] == '(') {

            /* let's have d point the end of the argument */
            for (d = c + 2; *d && *d != ')'; d++);

            if (*d == ')') {
                key = PyString_FromStringAndSize(c+2, d-c-2);
                value = PyObject_GetItem(var, key);
                /* key has refcnt 1, value the original value + 1 */
                
                /*  if value is NULL we did not find the key (or this is not a
                    dictionary): let python raise a KeyError */
                if (value == NULL) {
                    Py_DECREF(key); /* destroy key */
                    Py_XDECREF(n);  /* destroy n */
                    return -1;
                }

                Dprintf("_mogrify: value refcnt: %d (+1)", value->ob_refcnt);
                
                if (n == NULL) {
                    n = PyDict_New();
                }
                
                if ((item = PyObject_GetItem(n, key)) == NULL) {
                    PyObject *t = NULL;

                    PyErr_Clear();

                    /* None is always converted to NULL; this is an
                       optimization over the adapting code and can go away in
                       the future if somebody finds a None adapter usefull. */
                    if (value == Py_None) {
                        t = PyString_FromString("NULL");
                        PyDict_SetItem(n, key, t);
                        /* t is a new object, refcnt = 1, key is at 2 */
                        
                        /* if the value is None we need to substitute the
                           formatting char with 's' (FIXME: this should not be
                           necessary if we drop support for formats other than
                           %s!) */
                        while (*d && !isalpha(*d)) d++;
                        if (*d) *d = 's';
                    }
                    else {
                        t = microprotocol_getquoted(value, conn);

                        if (t != NULL) {
                            PyDict_SetItem(n, key, t);
                            /* both key and t refcnt +1, key is at 2 now */
                        }
                        else {
                            /* no adapter found, raise a BIG exception */
                            Py_XDECREF(value); 
                            Py_DECREF(n);
                            return -1;
                        }
                    }

                    Py_XDECREF(t); /* t dies here */
                    /* after the DECREF value has the original refcnt plus 1
                       if it was added to the dictionary directly; good */
                    Py_XDECREF(value); 
                }
                else {
                    /* we have an item with one extra refcnt here, zap! */
                    Py_DECREF(item);
                }
                Py_DECREF(key); /* key has the original refcnt now */
                Dprintf("_mogrify: after value refcnt: %d",value->ob_refcnt);
            }
            c = d;
        }
        
        else if (c[0] == '%' && c[1] != '(') {
            /* this is a format that expects a tuple; it is much easier,
               because we don't need to check the old/new dictionary for
               keys */
            
            value = PySequence_GetItem(var, index);
            /* value has refcnt inc'ed by 1 here */
            
            /*  if value is NULL this is not a sequence or the index is wrong;
                anyway we let python set its own exception */
            if (value == NULL) {
                Py_XDECREF(n);
                return -1;
            }

            if (n == NULL) {
                n = PyTuple_New(PyObject_Length(var));
            }
            
            /* let's have d point just after the '%' */
            d = c+1;
            
            if (value == Py_None) {
                PyTuple_SET_ITEM(n, index, PyString_FromString("NULL"));
                while (*d && !isalpha(*d)) d++;
                if (*d) *d = 's';
                Py_DECREF(value);
            }
            else {
                PyObject *t = microprotocol_getquoted(value, conn);

                if (t != NULL) {
                    PyTuple_SET_ITEM(n, index, t);
                    Py_DECREF(value);
                }
                else {
                    Py_DECREF(n);
                    Py_DECREF(value);
                    return -1;
                }
            }
            c = d;
            index += 1;
        }
        else {
            c++;
        }
    }

    if (force && n == NULL)
        n = PyTuple_New(0);
    *new = n;
    
    return 0;
}
    
#define psyco_curs_execute_doc \
"execute(query, vars=None, async=0) -- Execute query with bound vars."

static int
_psyco_curs_execute(cursorObject *self,
                    PyObject *operation, PyObject *vars, long int async)
{
    int res;
    PyObject *fquery, *cvt = NULL, *uoperation = NULL;
    
    pthread_mutex_lock(&(self->conn->lock));
    if (self->conn->async_cursor != NULL
        && self->conn->async_cursor != (PyObject*)self) {
        pthread_mutex_unlock(&(self->conn->lock));
        psyco_set_error(ProgrammingError, (PyObject*)self,
                         "asynchronous query already in execution", NULL, NULL);
        return 0;
    }
    pthread_mutex_unlock(&(self->conn->lock));
    
    if (!PyObject_IsTrue(operation)) {
        psyco_set_error(ProgrammingError, (PyObject*)self,
                         "can't execute an empty query", NULL, NULL);
    	return 0;
    }
    
    if (PyUnicode_Check(operation)) {
        PyObject *enc = PyDict_GetItemString(psycoEncodings,
                                             self->conn->encoding);
        /* enc is a borrowed reference, we won't decref it */
        
        if (enc) {
            operation = PyUnicode_AsEncodedString(
                operation, PyString_AsString(enc), NULL);

            /* if there was an error during the encoding from unicode to the
               target encoding we just let the exception propagate */
            if (operation == NULL) return 0;

            /* we clone operation in uoperation to be sure to free it later */ 
            uoperation = operation;
        }
        else {
            PyErr_Format(InterfaceError, "can't encode unicode query to %s",
                         self->conn->encoding);
            return 0;
        }
    }
    else if (!PyString_Check(operation)) {
        /* the operation is not unicode or string, raise an error */
        PyErr_SetString(PyExc_TypeError,
                        "argument 1 must be a string or unicode object");
        return 0;
    }
    
    IFCLEARPGRES(self->pgres);

    if (self->query) {
        Py_DECREF(self->query);
        self->query = NULL;
    }
    
    Dprintf("psyco_curs_execute: starting execution of new query");

    /* here we are, and we have a sequence or a dictionary filled with
       objects to be substituted (bound variables). we try to be smart and do
       the right thing (i.e., what the user expects) */
    
    if (vars && vars != Py_None)
    {
        if(_mogrify(vars, operation, self->conn, &cvt) == -1) {
            Py_XDECREF(uoperation);
            return 0;
        }
    }

    if (vars && cvt) {
        /* if PyString_Format() return NULL an error occured: if the error is
           a TypeError we need to check the exception.args[0] string for the
           values:

               "not enough arguments for format string"
               "not all arguments converted"

           and return the appropriate ProgrammingError. we do that by grabbing
           the curren exception (we will later restore it if the type or the
           strings do not match.) */
        
        if (!(fquery = PyString_Format(operation, cvt))) {
            PyObject *err, *arg, *trace;
            int pe = 0;

            PyErr_Fetch(&err, &arg, &trace);
            
            if (err && PyErr_GivenExceptionMatches(err, PyExc_TypeError)) {
                Dprintf("psyco_curs_execute: TypeError exception catched");
                PyErr_NormalizeException(&err, &arg, &trace);

                if (PyObject_HasAttrString(arg, "args")) {
                    PyObject *args = PyObject_GetAttrString(arg, "args");
                    PyObject *str = PySequence_GetItem(args, 0);
                    char *s = PyString_AS_STRING(str);

                    Dprintf("psyco_curs_execute:     -> %s", s);

                    if (!strcmp(s, "not enough arguments for format string")
                      || !strcmp(s, "not all arguments converted")) {
                        Dprintf("psyco_curs_execute:     -> got a match");
                        psyco_set_error(ProgrammingError, (PyObject*)self,
                                         s, NULL, NULL);
                        pe = 1;
                    }

                    Py_DECREF(args);
                    Py_DECREF(str);
                }
            }

            /* if we did not manage our own exception, restore old one */
            if (pe == 1) {
                Py_XDECREF(err); Py_XDECREF(arg); Py_XDECREF(trace);
            }
            else {
                PyErr_Restore(err, arg, trace);
            }
            Py_XDECREF(uoperation);
            return 0;
        }
        
        if (self->name != NULL) {
            self->query = PyString_FromFormat(
                "DECLARE %s CURSOR WITHOUT HOLD FOR %s",
                self->name, PyString_AS_STRING(fquery));
            Py_DECREF(fquery);
        }
        else {
            self->query = fquery;
        }
        
        Dprintf("psyco_curs_execute: cvt->refcnt = %d", cvt->ob_refcnt);
        Py_DECREF(cvt);
    }
    else {
        if (self->name != NULL) {
            self->query = PyString_FromFormat(
                "DECLARE %s CURSOR WITHOUT HOLD FOR %s",
                self->name, PyString_AS_STRING(operation));
        }
        else {
            Py_INCREF(operation);
            self->query = operation;
        }
    }
    
    res = pq_execute(self, PyString_AS_STRING(self->query), async);
    
    Dprintf("psyco_curs_execute: res = %d, pgres = %p", res, self->pgres);

    Py_XDECREF(uoperation);
    
    return res == -1 ? 0 : 1;
}

static PyObject *
psyco_curs_execute(cursorObject *self, PyObject *args, PyObject *kwargs)
{   
    long int async = 0;
    PyObject *vars = NULL, *operation = NULL;
    
    static char *kwlist[] = {"query", "vars", "async", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|Oi", kwlist,
                                     &operation, &vars, &async)) {
        return NULL;
    }

    if (self->name != NULL) {
        if (self->query != Py_None) {
            psyco_set_error(ProgrammingError, (PyObject*)self,
                "can't call .execute() on named cursors more than once",
                NULL, NULL);
            return NULL;
        }
        if (self->conn->isolation_level == 0) {
            psyco_set_error(ProgrammingError, (PyObject*)self,
                "can't use a named cursor outside of transactions", NULL, NULL);
            return NULL;          
        }
        if (self->conn->mark != self->mark) {
            psyco_set_error(ProgrammingError, (PyObject*)self,
                "named cursor isn't valid anymore", NULL, NULL);
            return NULL;
        }
    }

    EXC_IF_CURS_CLOSED(self);
    
    if (_psyco_curs_execute(self, operation, vars, async)) {
        Py_INCREF(Py_None);
        return Py_None;
    }
    else {
        return NULL;
    }
}

#define psyco_curs_executemany_doc \
"executemany(query, vars_list) -- Execute many queries with bound vars."

static PyObject *
psyco_curs_executemany(cursorObject *self, PyObject *args, PyObject *kwargs)
{
    PyObject *operation = NULL, *vars = NULL;
    PyObject *v, *iter = NULL;
    
    static char *kwlist[] = {"query", "vars_list", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OO", kwlist,
                                     &operation, &vars)) {
        return NULL;
    }

    EXC_IF_CURS_CLOSED(self);
    
    if (self->name != NULL) {
        psyco_set_error(ProgrammingError, (PyObject*)self,
                "can't call .executemany() on named cursors", NULL, NULL);
        return NULL;
    } 

    if (!PyIter_Check(vars)) {
        vars = iter = PyObject_GetIter(vars);
        if (iter == NULL) return NULL;
    }
    
    while ((v = PyIter_Next(vars)) != NULL) {
        if (_psyco_curs_execute(self, operation, v, 0) == 0) {
            Py_DECREF(v);
            return NULL;
        }
        else {
            Py_DECREF(v);
        }
    }
    Py_XDECREF(iter);
    
    Py_INCREF(Py_None);
    return Py_None;
}


#ifdef PSYCOPG_EXTENSIONS
#define psyco_curs_mogrify_doc \
"mogrify(query, vars=None) -> str -- Return query after vars binding."

static PyObject *
psyco_curs_mogrify(cursorObject *self, PyObject *args, PyObject *kwargs)
{   
    PyObject *vars = NULL, *cvt = NULL, *operation = NULL;
    PyObject *fquery;
    
    static char *kwlist[] = {"query", "vars", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|O", kwlist,
                                     &operation, &vars)) {
        return NULL;
    }

    if (PyUnicode_Check(operation)) {
        PyErr_SetString(NotSupportedError,
                        "unicode queries not yet supported");
        return NULL;
    }
    
    EXC_IF_CURS_CLOSED(self);
    IFCLEARPGRES(self->pgres);

    /* note that we don't overwrite the last query executed on the cursor, we
       just *return* the new query with bound variables

       TODO: refactor the common mogrification code (see psycopg_curs_execute
       for comments, the code is amost identical) */

    if (vars)
    {
        if(_mogrify(vars, operation, self->conn, &cvt) == -1) return NULL;
    }

    if (vars && cvt) {        
        if (!(fquery = PyString_Format(operation, cvt))) {
            PyObject *err, *arg, *trace;
            int pe = 0;

            PyErr_Fetch(&err, &arg, &trace);
            
            if (err && PyErr_GivenExceptionMatches(err, PyExc_TypeError)) {
                Dprintf("psyco_curs_execute: TypeError exception catched");
                PyErr_NormalizeException(&err, &arg, &trace);

                if (PyObject_HasAttrString(arg, "args")) {
                    PyObject *args = PyObject_GetAttrString(arg, "args");
                    PyObject *str = PySequence_GetItem(args, 0);
                    char *s = PyString_AS_STRING(str);

                    Dprintf("psyco_curs_execute:     -> %s", s);

                    if (!strcmp(s, "not enough arguments for format string")
                      || !strcmp(s, "not all arguments converted")) {
                        Dprintf("psyco_curs_execute:     -> got a match");
                        psyco_set_error(ProgrammingError, (PyObject*)self,
                                         s, NULL, NULL);
                        pe = 1;
                    }

                    Py_DECREF(args);
                    Py_DECREF(str);
                }
            }

            /* if we did not manage our own exception, restore old one */
            if (pe == 1) {
                Py_XDECREF(err); Py_XDECREF(arg); Py_XDECREF(trace);
            }
            else {
                PyErr_Restore(err, arg, trace);
            }
            return NULL;
        }

        Dprintf("psyco_curs_execute: cvt->refcnt = %d, fquery->refcnt = %d",
                cvt->ob_refcnt, fquery->ob_refcnt);
        Py_DECREF(cvt);
    }
    else {
        fquery = operation;
        Py_INCREF(operation);
    }

    return fquery;
}
#endif


/* fetchone method - fetch one row of results */

#define psyco_curs_fetchone_doc \
"fetchone() -> tuple or None\n\n" \
"Return the next row of a query result set in the form of a tuple (by\n" \
"default) or using the sequence factory previously set in the\n" \
"`row_factory` attribute. Return `None` when no more data is available.\n"

static int
_psyco_curs_prefetch(cursorObject *self)
{
    int i = 0;
    
    /* check if the fetching cursor is the one that did the asynchronous query
       and raise an exception if not */
    pthread_mutex_lock(&(self->conn->lock));
    if (self->conn->async_cursor != NULL
        && self->conn->async_cursor != (PyObject*)self) {
        pthread_mutex_unlock(&(self->conn->lock));
        psyco_set_error(ProgrammingError, (PyObject*)self,
                         "asynchronous fetch by wrong cursor", NULL, NULL);
        return -2;
    }
    pthread_mutex_unlock(&(self->conn->lock));
    
    if (self->pgres == NULL || self->needsfetch) {
        self->needsfetch = 0;
        Dprintf("_psyco_curs_prefetch: trying to fetch data");
        do {
            i = pq_fetch(self);
            Dprintf("_psycopg_curs_prefetch: result = %d", i);
        } while(i == 1);
    }

    Dprintf("_psyco_curs_prefetch: result = %d", i);
    return i;
}

static PyObject *
_psyco_curs_buildrow_fill(cursorObject *self, PyObject *res,
                          int row, int n, int istuple)
{
    int i, len;
    unsigned char *str;
    PyObject *val;
    
    for (i=0; i < n; i++) {
        if (PQgetisnull(self->pgres, row, i)) {
            str = NULL;
            len = 0;
        }
        else {
            str = (unsigned char*)PQgetvalue(self->pgres, row, i);
            len = PQgetlength(self->pgres, row, i);
        }

        Dprintf("_psyco_curs_buildrow: row %ld, element %d, len %i",
                self->row, i, len);

        val = typecast_cast(PyTuple_GET_ITEM(self->casts, i), (char*)str, len,
                            (PyObject*)self);

        if (val) {
            Dprintf("_psyco_curs_buildrow: val->refcnt = %d", val->ob_refcnt);
            if (istuple) {
                PyTuple_SET_ITEM(res, i, val);
            }
            else {
                PySequence_SetItem(res, i, val);
                Py_DECREF(val);
            }
        }
        else {
            /* an error occurred in the type system, we return NULL to raise
               an exception. the typecast code should already have set the
               exception type and text */
            Py_DECREF(res);
            res = NULL;
            break;
        }
    }
    return res;
}

static PyObject *
_psyco_curs_buildrow(cursorObject *self, int row)
{
    int n;
    
    n = PQnfields(self->pgres);
    return _psyco_curs_buildrow_fill(self, PyTuple_New(n), row, n, 1);
}

static PyObject *
_psyco_curs_buildrow_with_factory(cursorObject *self, int row)
{
    int n;
    PyObject *res;

    n = PQnfields(self->pgres);
    if ((res = PyObject_CallFunction(self->tuple_factory, "O", self))== NULL)
        return NULL;

    return _psyco_curs_buildrow_fill(self, res, row, n, 0);
}

PyObject *
psyco_curs_fetchone(cursorObject *self, PyObject *args)
{
    PyObject *res;
    
    if (args && !PyArg_ParseTuple(args, "")) return NULL;
    
    EXC_IF_CURS_CLOSED(self)
    if (_psyco_curs_prefetch(self) < 0) return NULL;
    EXC_IF_NO_TUPLES(self);
    
    if (self->name != NULL) {
        char buffer[128];
        
        EXC_IF_NO_MARK(self);
        PyOS_snprintf(buffer, 127, "FETCH FORWARD 1 FROM %s", self->name);
        if (pq_execute(self, buffer, 0) == -1) return NULL;
        if (_psyco_curs_prefetch(self) < 0) return NULL;
    }
    
    Dprintf("psyco_curs_fetchone: fetching row %ld", self->row);
    Dprintf("psyco_curs_fetchone: rowcount = %ld", self->rowcount); 

    if (self->row >= self->rowcount) {
        /* we exausted available data: return None */
        Py_INCREF(Py_None);
        return Py_None;
    }

    if (self->tuple_factory == Py_None)
        res = _psyco_curs_buildrow(self, self->row);
    else
        res = _psyco_curs_buildrow_with_factory(self, self->row);
    
    self->row++; /* move the counter to next line */

    /* if the query was async aggresively free pgres, to allow
       successive requests to reallocate it */
    if (self->row >= self->rowcount
        && self->conn->async_cursor == (PyObject*)self)
        IFCLEARPGRES(self->pgres);

    return res;
}


/* fetch many - fetch some results */

#define psyco_curs_fetchmany_doc \
"fetchmany(size=self.arraysize) -> list of tuple\n\n" \
"Return the next `size` rows of a query result set in the form of a list\n" \
"of tuples (by default) or using the sequence factory previously set in\n" \
"the `row_factory` attribute. Return `None` when no more data is available.\n"

PyObject *
psyco_curs_fetchmany(cursorObject *self, PyObject *args, PyObject *kwords)
{
    int i;
    PyObject *list, *res;
    
    long int size = self->arraysize;
    static char *kwlist[] = {"size", NULL};
    
    if (!PyArg_ParseTupleAndKeywords(args, kwords, "|l", kwlist, &size)) {
        return NULL;
    }

    EXC_IF_CURS_CLOSED(self);
    if (_psyco_curs_prefetch(self) < 0) return NULL;
    EXC_IF_NO_TUPLES(self);

    if (self->name != NULL) {
        char buffer[128];
        
        EXC_IF_NO_MARK(self);
        PyOS_snprintf(buffer, 127, "FETCH FORWARD %d FROM %s",
            (int)size, self->name);
        if (pq_execute(self, buffer, 0) == -1) return NULL;
        if (_psyco_curs_prefetch(self) < 0) return NULL;
    }

    /* make sure size is not > than the available number of rows */
    if (size > self->rowcount - self->row || size < 0) {
        size = self->rowcount - self->row;
    }

    Dprintf("psyco_curs_fetchmany: size = %ld", size);
    
    if (size <= 0) {
        return PyList_New(0);
    }
    
    list = PyList_New(size);
    
    for (i = 0; i < size; i++) {
        if (self->tuple_factory == Py_None)
            res = _psyco_curs_buildrow(self, self->row);
        else
            res = _psyco_curs_buildrow_with_factory(self, self->row);
        
        self->row++;

        if (res == NULL) {
            Py_DECREF(list);
            return NULL;
        }

        PyList_SET_ITEM(list, i, res);
    }

    /* if the query was async aggresively free pgres, to allow
       successive requests to reallocate it */
    if (self->row >= self->rowcount
        && self->conn->async_cursor == (PyObject*)self)
        IFCLEARPGRES(self->pgres);
    
    return list;
}


/* fetch all - fetch all results */

#define psyco_curs_fetchall_doc \
"fetchall() -> list of tuple\n\n" \
"Return all the remaining rows of a query result set.\n\n" \
"Rows are returned in the form of a list of tuples (by default) or using\n" \
"the sequence factory previously set in the `row_factory` attribute.\n" \
"Return `None` when no more data is available.\n"

PyObject *
psyco_curs_fetchall(cursorObject *self, PyObject *args)
{
    int i, size;
    PyObject *list, *res;

    if (!PyArg_ParseTuple(args, "")) {
        return NULL;
    }

    EXC_IF_CURS_CLOSED(self);
    if (_psyco_curs_prefetch(self) < 0) return NULL;
    EXC_IF_NO_TUPLES(self);
    
    if (self->name != NULL) {
        char buffer[128];
        
        EXC_IF_NO_MARK(self);
        PyOS_snprintf(buffer, 127, "FETCH FORWARD ALL FROM %s", self->name);
        if (pq_execute(self, buffer, 0) == -1) return NULL;
        if (_psyco_curs_prefetch(self) < 0) return NULL;
    }

    size = self->rowcount - self->row;
    
    if (size <= 0) {
        return PyList_New(0);
    }
    
    list = PyList_New(size);
    
    for (i = 0; i < size; i++) {
        if (self->tuple_factory == Py_None)
            res = _psyco_curs_buildrow(self, self->row);
        else
            res = _psyco_curs_buildrow_with_factory(self, self->row);
        
        self->row++;

        if (res == NULL) {
            Py_DECREF(list);
            return NULL;
        }

        PyList_SET_ITEM(list, i, res);
    }

    /* if the query was async aggresively free pgres, to allow
       successive requests to reallocate it */
    if (self->row >= self->rowcount
        && self->conn->async_cursor == (PyObject*)self)
        IFCLEARPGRES(self->pgres);
    
    return list;
}


/* callproc method - execute a stored procedure */

#define psyco_curs_callproc_doc \
"callproc(procname, parameters=None, async=0) -- Execute stored procedure."

static PyObject *
psyco_curs_callproc(cursorObject *self, PyObject *args, PyObject *kwargs)
{
    char *procname = NULL, *sql = NULL;
    long int async = 0;
    int i, nparameters = 0, sl = 0;
    PyObject *parameters = NULL;
    PyObject *operation = NULL;
    PyObject *res = NULL;

    if (!PyArg_ParseTuple(args, "s|Oi", &procname, &parameters, &async)) {
        return NULL;
    }

    EXC_IF_CURS_CLOSED(self);

    if (self->name != NULL) {
        psyco_set_error(ProgrammingError, (PyObject*)self,
                         "can't call .callproc() on named cursors", NULL, NULL);
        return NULL;
    } 

    if(parameters && parameters != Py_None) {
        nparameters = PyObject_Length(parameters);
	if (nparameters < 0) nparameters = 0;
    }

    /* allocate some memory, build the SQL and create a PyString from it */
    sl = strlen(procname) + 10 + nparameters*3 - (nparameters ? 1 : 0);
    sql = (char*)PyMem_Malloc(sl);
    if (sql == NULL) return NULL;
    
    sprintf(sql, "SELECT %s(", procname);
    for(i=0; i<nparameters; i++) {
        strcat(sql, "%s,");
    }
    sql[sl-2] = ')';
    sql[sl-1] = '\0';
    
    operation = PyString_FromString(sql);
    PyMem_Free((void*)sql);
    
    if (_psyco_curs_execute(self, operation, parameters, async)) {
        Py_INCREF(Py_None);
        res = Py_None;
    }

    Py_DECREF(operation);
    return res;
}


/* nextset method - return the next set of data (not supported) */

#define psyco_curs_nextset_doc \
"nextset() -- Skip to next set of data.\n\n" \
"This method is not supported (PostgreSQL does not have multiple data \n" \
"sets) and will raise a NotSupportedError exception."

static PyObject *
psyco_curs_nextset(cursorObject *self, PyObject *args)
{
    if (!PyArg_ParseTuple(args, "")) return NULL;
    
    EXC_IF_CURS_CLOSED(self);

    PyErr_SetString(NotSupportedError, "not supported by PostgreSQL");
    return NULL;
}


/* setinputsizes - predefine memory areas for execute (does nothing) */

#define psyco_curs_setinputsizes_doc \
"setinputsizes(sizes) -- Set memory areas before execute.\n\n" \
"This method currently does nothing but it is safe to call it."

static PyObject *
psyco_curs_setinputsizes(cursorObject *self, PyObject *args)
{
    PyObject *sizes;
    
    if (!PyArg_ParseTuple(args, "O", &sizes))
        return NULL;
    
    EXC_IF_CURS_CLOSED(self);

    Py_INCREF(Py_None);
    return Py_None;
}


/* setoutputsize - predefine memory areas for execute (does nothing) */

#define psyco_curs_setoutputsize_doc \
"setoutputsize(size, column=None) -- Set column buffer size.\n\n" \
"This method currently does nothing but it is safe to call it."

static PyObject *
psyco_curs_setoutputsize(cursorObject *self, PyObject *args)
{
    long int size, column;
    
    if (!PyArg_ParseTuple(args, "l|l", &size, &column))
        return NULL;
    
    EXC_IF_CURS_CLOSED(self);

    Py_INCREF(Py_None);
    return Py_None;
}


/* scroll - scroll position in result list */

#define psyco_curs_scroll_doc \
"scroll(value, mode='relative') -- Scroll to new position according to mode." 

static PyObject *
psyco_curs_scroll(cursorObject *self, PyObject *args, PyObject *kwargs)
{
    int value, newpos;
    char *mode = "relative";

    static char *kwlist[] = {"value", "mode", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "i|s",
                                     kwlist, &value, &mode))
        return NULL;
    
    EXC_IF_CURS_CLOSED(self);

    /* if the cursor is not named we have the full result set and we can do
       our own calculations to scroll; else we just delegate the scrolling
       to the MOVE SQL statement */
    if (self->name == NULL) {
        if (strcmp(mode, "relative") == 0) {
            newpos = self->row + value;
        } else if (strcmp( mode, "absolute") == 0) {
            newpos = value;
        } else {
            psyco_set_error(ProgrammingError, (PyObject*)self,
                "scroll mode must be 'relative' or 'absolute'", NULL, NULL);
            return NULL;
        }
    
        if (newpos < 0 || newpos >= self->rowcount ) {
            psyco_set_error(ProgrammingError, (PyObject*)self,
                             "scroll destination out of bounds", NULL, NULL);
            return NULL;
        }
        
        self->row = newpos;
    }
    
    else {
        char buffer[128];
        
        EXC_IF_NO_MARK(self);
        
        if (strcmp(mode, "absolute") == 0) {
            PyOS_snprintf(buffer, 127, "MOVE ABSOLUTE %d FROM %s",
                value, self->name);
        }
        else {
            PyOS_snprintf(buffer, 127, "MOVE %d FROM %s", value, self->name);
        }
        if (pq_execute(self, buffer, 0) == -1) return NULL;
        if (_psyco_curs_prefetch(self) < 0) return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


#ifdef PSYCOPG_EXTENSIONS

/* extension: copy_from - implements COPY FROM */

#define psyco_curs_copy_from_doc \
"copy_from(file, table, sep='\\t', null='\\N', columns=None) -- Copy table from file."

static int
_psyco_curs_has_read_check(PyObject* o, void* var)
{
    if (PyObject_HasAttrString(o, "readline")
        && PyObject_HasAttrString(o, "read")) {
        Py_INCREF(o);
        *((PyObject**)var) = o;
        return 1;
    }
    else {
        PyErr_SetString(PyExc_TypeError,
            "argument 1 must have both .read() and .readline() methods");
        return 0;
    }   
}

static PyObject *
psyco_curs_copy_from(cursorObject *self, PyObject *args, PyObject *kwargs)
{
    char query[1024];
    char *table_name;
    char *sep = "\t", *null = NULL;
    long int bufsize = DEFAULT_COPYSIZE;
    PyObject *file, *columns = NULL, *res = NULL;
    char columnlist[1024] = "";

    static char *kwlist[] = {"file", "table", "sep", "null", "size", 
                             "columns", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O&s|ssiO", kwlist,
                                     _psyco_curs_has_read_check, &file,
                                     &table_name, &sep, &null, &bufsize,
                                     &columns)) {
        return NULL;
    }
    
    if (columns != NULL && columns != Py_None) {
        PyObject* collistiter = PyObject_GetIter(columns);
        PyObject* col;
        int collistlen = 2;
        int colitemlen;
        char* colname;
        if (collistiter == NULL) {
            return NULL;
        }
        strcpy(columnlist, " (");
        while ((col = PyIter_Next(collistiter)) != NULL) {
            if (!PyString_Check(col)) {
                Py_DECREF(col);
                Py_DECREF(collistiter);
                PyErr_SetString(PyExc_ValueError,
		    "Elements in column list must be strings");
                return NULL;
            }
            PyString_AsStringAndSize(col, &colname, &colitemlen);
            if (collistlen + colitemlen > 1022) {
                Py_DECREF(col);
                Py_DECREF(collistiter);
                PyErr_SetString(PyExc_ValueError, "Column list too long");
                return NULL;
            }
            strncpy(&columnlist[collistlen], colname, colitemlen);
            collistlen += colitemlen;
            columnlist[collistlen++] = ',';
            Py_DECREF(col);
        }
        Py_DECREF(collistiter);

        if (collistlen == 2) {  /* empty list; we printed no comma */
            collistlen++;
        }

        columnlist[collistlen - 1] = ')';
        columnlist[collistlen] = '\0';
    }

    if (PyErr_Occurred()) {
        return NULL;
    }

    EXC_IF_CURS_CLOSED(self);

    if (null) {
        PyOS_snprintf(query, 1023, "COPY %s%s FROM stdin USING DELIMITERS '%s'"
                      " WITH NULL AS '%s'", table_name, columnlist, sep, null);
    }
    else {
        PyOS_snprintf(query, 1023, "COPY %s%s FROM stdin USING DELIMITERS '%s'",
                      table_name, columnlist, sep);
    }
    Dprintf("psyco_curs_copy_from: query = %s", query);

    self->copysize = bufsize;
    self->copyfile = file;

    if (pq_execute(self, query, 0) == 1) {
        res = Py_None;
        Py_INCREF(Py_None);
    }

    self->copyfile =NULL;
    
    return res;
}

#define psyco_curs_copy_to_doc \
"copy_to(file, table, sep='\\t', null='\\N') -- Copy table to file."

static int
_psyco_curs_has_write_check(PyObject* o, void* var)
{
    if (PyObject_HasAttrString(o, "write")) {
        Py_INCREF(o);
        *((PyObject**)var) = o;
        return 1;
    }
    else {
        PyErr_SetString(PyExc_TypeError,
                        "argument 1 must have a .write() method");
        return 0;
    }   
}

static PyObject *
psyco_curs_copy_to(cursorObject *self, PyObject *args, PyObject *kwargs)
{
    char query[256];
    char *table_name;
    char *sep = "\t", *null = NULL;
    PyObject *file, *res = NULL;

    static char *kwlist[] = {"file", "table", "sep", "null", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O&s|ss", kwlist,
                                     _psyco_curs_has_write_check, &file,
                                     &table_name, &sep, &null)) {
        return NULL;
    }
    
    EXC_IF_CURS_CLOSED(self);

    if (null) {
        PyOS_snprintf(query, 255, "COPY %s TO stdout USING DELIMITERS '%s'"
                      " WITH NULL AS '%s'", table_name, sep, null);
    }
    else {
        PyOS_snprintf(query, 255, "COPY %s TO stdout USING DELIMITERS '%s'",
                      table_name, sep);
    }

    self->copysize = 0;
    self->copyfile = file;

    if (pq_execute(self, query, 0) == 1) {
        res = Py_None;
        Py_INCREF(Py_None);   
    }
    
    self->copyfile = NULL;
    
    return res;
}   
/* extension: fileno - return the file descripor of the connection */

#define psyco_curs_fileno_doc \
"fileno() -> int -- Return file descriptor associated to database connection."

static PyObject *
psyco_curs_fileno(cursorObject *self, PyObject *args)
{
    long int socket;
    
    if (!PyArg_ParseTuple(args, "")) return NULL;
    EXC_IF_CURS_CLOSED(self);

    /* note how we call PQflush() to make sure the user will use
       select() in the safe way! */
    pthread_mutex_lock(&(self->conn->lock));
    Py_BEGIN_ALLOW_THREADS;
    PQflush(self->conn->pgconn);
    socket = (long int)PQsocket(self->conn->pgconn);
    Py_END_ALLOW_THREADS;
    pthread_mutex_unlock(&(self->conn->lock));

    return PyInt_FromLong(socket);
}

/* extension: isready - return true if data from async execute is ready */

#define psyco_curs_isready_doc \
"isready() -> bool -- Return True if data is ready after an async query."

static PyObject *
psyco_curs_isready(cursorObject *self, PyObject *args)
{
    if (!PyArg_ParseTuple(args, "")) return NULL;
    EXC_IF_CURS_CLOSED(self);

    /* pq_is_busy does its own locking, we don't need anything special but if
       the cursor is ready we need to fetch the result and free the connection
       for the next query. */
    
    if (pq_is_busy(self->conn)) {
        Py_INCREF(Py_False);
        return Py_False;
    }
    else {
        IFCLEARPGRES(self->pgres);
        pthread_mutex_lock(&(self->conn->lock));
        self->pgres = PQgetResult(self->conn->pgconn);
        self->conn->async_cursor = NULL;
        pthread_mutex_unlock(&(self->conn->lock));
        self->needsfetch = 1;
        Py_INCREF(Py_True);
        return Py_True;
    }
}

#endif


/** the cursor object **/

/* iterator protocol */

static PyObject *
cursor_iter(PyObject *self)
{
    EXC_IF_CURS_CLOSED((cursorObject*)self);
    Py_INCREF(self);
    return self;
}

static PyObject *
cursor_next(PyObject *self)
{
    PyObject *res;

    /* we don't parse arguments: psyco_curs_fetchone will do that for us */
    res = psyco_curs_fetchone((cursorObject*)self, NULL);

    /* convert a None to NULL to signal the end of iteration */
    if (res && res == Py_None) {
        Py_DECREF(res);
        res = NULL;
    }
    return res;
}

/* object method list */

static struct PyMethodDef cursorObject_methods[] = {
    /* DBAPI-2.0 core */
    {"close", (PyCFunction)psyco_curs_close,
     METH_VARARGS, psyco_curs_close_doc},
    {"execute", (PyCFunction)psyco_curs_execute,
     METH_VARARGS|METH_KEYWORDS, psyco_curs_execute_doc},
    {"executemany", (PyCFunction)psyco_curs_executemany,
     METH_VARARGS|METH_KEYWORDS, psyco_curs_executemany_doc},
    {"fetchone", (PyCFunction)psyco_curs_fetchone,
     METH_VARARGS, psyco_curs_fetchone_doc},
    {"fetchmany", (PyCFunction)psyco_curs_fetchmany,
     METH_VARARGS|METH_KEYWORDS, psyco_curs_fetchmany_doc},
    {"fetchall", (PyCFunction)psyco_curs_fetchall,
     METH_VARARGS, psyco_curs_fetchall_doc},
    {"callproc", (PyCFunction)psyco_curs_callproc,
     METH_VARARGS, psyco_curs_callproc_doc},
    {"nextset", (PyCFunction)psyco_curs_nextset,
     METH_VARARGS, psyco_curs_nextset_doc},
    {"setinputsizes", (PyCFunction)psyco_curs_setinputsizes,
     METH_VARARGS, psyco_curs_setinputsizes_doc},
    {"setoutputsize", (PyCFunction)psyco_curs_setoutputsize,
     METH_VARARGS, psyco_curs_setoutputsize_doc},
    /* DBAPI-2.0 extensions */
    {"scroll", (PyCFunction)psyco_curs_scroll,
     METH_VARARGS|METH_KEYWORDS, psyco_curs_scroll_doc},
    /* psycopg extensions */
#ifdef PSYCOPG_EXTENSIONS
    {"mogrify", (PyCFunction)psyco_curs_mogrify,
     METH_VARARGS|METH_KEYWORDS, psyco_curs_mogrify_doc},
    {"fileno", (PyCFunction)psyco_curs_fileno,
     METH_VARARGS, psyco_curs_fileno_doc},
    {"isready", (PyCFunction)psyco_curs_isready,
     METH_VARARGS, psyco_curs_isready_doc},
    {"copy_from", (PyCFunction)psyco_curs_copy_from,
     METH_VARARGS|METH_KEYWORDS, psyco_curs_copy_from_doc},
    {"copy_to", (PyCFunction)psyco_curs_copy_to,
     METH_VARARGS|METH_KEYWORDS, psyco_curs_copy_to_doc},
#endif
    {NULL}
};

/* object member list */

#define OFFSETOF(x) offsetof(cursorObject, x)

static struct PyMemberDef cursorObject_members[] = {
    /* DBAPI-2.0 basics */
    {"rowcount", T_LONG, OFFSETOF(rowcount), RO,
        "Number of rows read from the backend in the last command."},
    {"arraysize", T_LONG, OFFSETOF(arraysize), 0, 
        "Number of records `fetchmany()` must fetch if not explicitely " \
        "specified."},
    {"description", T_OBJECT, OFFSETOF(description), RO,
        "Cursor description as defined in DBAPI-2.0."},
    {"lastrowid", T_LONG, OFFSETOF(lastoid), RO,
        "The ``oid`` of the last row inserted by the cursor."},
    /* DBAPI-2.0 extensions */
    {"rownumber", T_LONG, OFFSETOF(row), RO,
        "The current row position."},
    {"connection", T_OBJECT, OFFSETOF(conn), RO, 
        "The connection where the cursor comes from."},
#ifdef PSYCOPG_EXTENSIONS
    {"name", T_STRING, OFFSETOF(name), RO},
    {"statusmessage", T_OBJECT, OFFSETOF(pgstatus), RO,
        "The return message of the last command."},
    {"query", T_OBJECT, OFFSETOF(query), RO,
        "The last query text sent to the backend."},
    {"row_factory", T_OBJECT, OFFSETOF(tuple_factory), 0},
    {"tzinfo_factory", T_OBJECT, OFFSETOF(tzinfo_factory), 0},
    {"typecaster", T_OBJECT, OFFSETOF(caster), RO},
    {"string_types", T_OBJECT, OFFSETOF(string_types), 0},
    {"binary_types", T_OBJECT, OFFSETOF(binary_types), 0},
#endif
    {NULL}
};

/* initialization and finalization methods */

static int
cursor_setup(cursorObject *self, connectionObject *conn, char *name)
{
    Dprintf("cursor_setup: init cursor object at %p", self);
    Dprintf("cursor_setup:     parameters: name = %s, conn = %p", name, conn);
    
    if (name) {
        self->name = PyMem_Malloc(strlen(name)+1);
        if (self->name == NULL) return 1;
        strncpy(self->name, name, strlen(name)+1);
    }

    /* FIXME: why does this raise an excpetion on the _next_ line of code?
    if (PyObject_IsInstance((PyObject*)conn,
                             (PyObject *)&connectionType) == 0) {
        PyErr_SetString(PyExc_TypeError,
            "argument 1 must be subclass of psycopg2._psycopg.connection");
        return 1;
    } */
    self->conn = conn;
    Py_INCREF((PyObject*)self->conn);
    
    self->closed = 0;
    self->mark = conn->mark;
    self->pgres = NULL; 
    self->notuples = 1;
    self->arraysize = 1;
    self->rowcount = -1;
    self->lastoid = InvalidOid;
    
    self->casts = NULL;
    self->notice = NULL;
    
    self->string_types = NULL;
    self->binary_types = NULL;
    
    self->description = Py_None;
    Py_INCREF(Py_None);
    self->pgstatus = Py_None;
    Py_INCREF(Py_None);
    self->tuple_factory = Py_None;
    Py_INCREF(Py_None);
    self->query = Py_None;
    Py_INCREF(Py_None);
    
    /* default tzinfo factory */
    self->tzinfo_factory = pyPsycopgTzFixedOffsetTimezone;
    Py_INCREF(self->tzinfo_factory);
    
    Dprintf("cursor_setup: good cursor object at %p, refcnt = %d",
            self, ((PyObject *)self)->ob_refcnt);
    return 0;
}

static void
cursor_dealloc(PyObject* obj)
{
    cursorObject *self = (cursorObject *)obj;

    if (self->name) PyMem_Free(self->name);
    
    Py_XDECREF((PyObject*)self->conn);
    Py_XDECREF(self->casts);
    Py_XDECREF(self->description);
    Py_XDECREF(self->pgstatus);
    Py_XDECREF(self->tuple_factory);
    Py_XDECREF(self->tzinfo_factory);
    Py_XDECREF(self->query);

    IFCLEARPGRES(self->pgres);
    
    Dprintf("cursor_dealloc: deleted cursor object at %p, refcnt = %d",
            obj, obj->ob_refcnt);

    obj->ob_type->tp_free(obj);
}

static int
cursor_init(PyObject *obj, PyObject *args, PyObject *kwds)
{
    char *name = NULL;
    PyObject *conn;
    
    if (!PyArg_ParseTuple(args, "O|s", &conn, &name))
        return -1;

    return cursor_setup((cursorObject *)obj, (connectionObject *)conn, name);
}

static PyObject *
cursor_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    return type->tp_alloc(type, 0);
}

static void
cursor_del(PyObject* self)
{
    PyObject_Del(self);
}

static PyObject *
cursor_repr(cursorObject *self)
{
    return PyString_FromFormat(
        "<cursor object at %p; closed: %d>", self, self->closed);
}


/* object type */

#define cursorType_doc \
"A database cursor."

PyTypeObject cursorType = {
    PyObject_HEAD_INIT(NULL)
    0,
    "psycopg2._psycopg.cursor",
    sizeof(cursorObject),
    0,
    cursor_dealloc, /*tp_dealloc*/
    0,          /*tp_print*/  
    0,          /*tp_getattr*/
    0,          /*tp_setattr*/
    0,          /*tp_compare*/
    (reprfunc)cursor_repr, /*tp_repr*/
    0,          /*tp_as_number*/
    0,          /*tp_as_sequence*/
    0,          /*tp_as_mapping*/
    0,          /*tp_hash */

    0,          /*tp_call*/
    (reprfunc)cursor_repr, /*tp_str*/
    0,          /*tp_getattro*/
    0,          /*tp_setattro*/
    0,          /*tp_as_buffer*/

    Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE|Py_TPFLAGS_HAVE_ITER, /*tp_flags*/
    cursorType_doc, /*tp_doc*/
    
    0,          /*tp_traverse*/
    0,          /*tp_clear*/

    0,          /*tp_richcompare*/
    0,          /*tp_weaklistoffset*/

    cursor_iter, /*tp_iter*/
    cursor_next, /*tp_iternext*/

    /* Attribute descriptor and subclassing stuff */

    cursorObject_methods, /*tp_methods*/
    cursorObject_members, /*tp_members*/
    0,          /*tp_getset*/
    0,          /*tp_base*/
    0,          /*tp_dict*/
    
    0,          /*tp_descr_get*/
    0,          /*tp_descr_set*/
    0,          /*tp_dictoffset*/
    
    cursor_init, /*tp_init*/
    0, /*tp_alloc  Will be set to PyType_GenericAlloc in module init*/
    cursor_new, /*tp_new*/
    (freefunc)cursor_del, /*tp_free  Low-level free-memory routine */
    0,          /*tp_is_gc For PyObject_IS_GC */
    0,          /*tp_bases*/
    0,          /*tp_mro method resolution order */
    0,          /*tp_cache*/
    0,          /*tp_subclasses*/
    0           /*tp_weaklist*/
};
