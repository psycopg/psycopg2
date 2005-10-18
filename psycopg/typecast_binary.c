/* typecast_binary.c - binary typecasting functions to python types
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

#include "typecast_binary.h"

#include <libpq-fe.h>
#include <stdlib.h>


/* Python object holding a memory chunk. The memory is deallocated when
   the object is destroyed. This type is used to let users directly access
   memory chunks holding unescaped binary data through the buffer interface.
 */

static void
chunk_dealloc(chunkObject *self)
{
    Dprintf("chunk_dealloc: deallocating memory at %p, size %d",
            self->base, self->len);
    free(self->base);
    self->ob_type->tp_free((PyObject *) self);
}

static PyObject *
chunk_repr(chunkObject *self)
{
    return PyString_FromFormat("<memory chunk at %p size %d>",
                               self->base, self->len);
}

static int
chunk_getreadbuffer(chunkObject *self, int segment, void **ptr)
{
    if (segment != 0)
    {
        PyErr_SetString(PyExc_SystemError,
                        "acessing non-existant buffer segment");
        return -1;
    }
    *ptr = self->base;
    return self->len;
}

static int
chunk_getsegcount(chunkObject *self, int *lenp)
{
    if (lenp != NULL)
        *lenp = self->len;
    return 1;
}

static PyBufferProcs chunk_as_buffer =
{
    (getreadbufferproc) chunk_getreadbuffer,
    (getwritebufferproc) NULL,
    (getsegcountproc) chunk_getsegcount,
    (getcharbufferproc) NULL
};

#define chunk_doc "memory chunk"

PyTypeObject chunkType = {
    PyObject_HEAD_INIT(NULL)
    0,                          /* ob_size */
    "psycopg2._psycopg.chunk",   /* tp_name */
    sizeof(chunkObject),        /* tp_basicsize */
    0,                          /* tp_itemsize */
    (destructor) chunk_dealloc, /* tp_dealloc*/
    0,                          /* tp_print */
    0,                          /* tp_getattr */
    0,                          /* tp_setattr */
    0,                          /* tp_compare */
    (reprfunc) chunk_repr,      /* tp_repr */
    0,                          /* tp_as_number */
    0,                          /* tp_as_sequence */
    0,                          /* tp_as_mapping */
    0,                          /* tp_hash */
    0,                          /* tp_call */
    0,                          /* tp_str */
    0,                          /* tp_getattro */
    0,                          /* tp_setattro */
    &chunk_as_buffer,           /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE, /* tp_flags */
    chunk_doc                   /* tp_doc */
};

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
typecast_BINARY_cast(char *s, int l, PyObject *curs)
{
    chunkObject *chunk;
    PyObject *res;
    char *str, *buffer = NULL;
    size_t len;

    if (s == NULL) {Py_INCREF(Py_None); return Py_None;}

    /* PQunescapeBytea absolutely wants a 0-terminated string and we don't
       want to copy the whole buffer, right? Wrong, but there isn't any other
       way <g> */
    if (s[l] != '\0') {
        if ((buffer = PyMem_Malloc(l+1)) == NULL)
            PyErr_NoMemory();
        strncpy(buffer, s, l);
        buffer[l] = '\0';
        s = buffer;
    }
    str = (char*)PQunescapeBytea((unsigned char*)s, &len);
    Dprintf("typecast_BINARY_cast: unescaped %d bytes", len);
    if (buffer) PyMem_Free(buffer);

    chunk = (chunkObject *) PyObject_New(chunkObject, &chunkType);
    if (chunk == NULL) return NULL;
    
    chunk->base = str;
    chunk->len = len;
    if ((res = PyBuffer_FromObject((PyObject *)chunk, 0, len)) == NULL)
        return NULL;

    /* PyBuffer_FromObject() created a new reference. Release our reference so
       that the memory can be freed once the buffer is garbage collected. */
    Py_DECREF(chunk);

    return res;
}
