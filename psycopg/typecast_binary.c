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
    Dprintf("chunk_dealloc: deallocating memory at %p, size "
        FORMAT_CODE_PY_SSIZE_T,
        self->base, self->len
      );
    free(self->base);
    self->ob_type->tp_free((PyObject *) self);
}

static PyObject *
chunk_repr(chunkObject *self)
{
    return PyString_FromFormat(
        "<memory chunk at %p size " FORMAT_CODE_PY_SSIZE_T ">",
        self->base, self->len
      );
}

static Py_ssize_t
chunk_getreadbuffer(chunkObject *self, Py_ssize_t segment, void **ptr)
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

static Py_ssize_t
chunk_getsegcount(chunkObject *self, Py_ssize_t *lenp)
{
    if (lenp != NULL)
        *lenp = self->len;
    return 1;
}

static PyBufferProcs chunk_as_buffer =
{
    (readbufferproc) chunk_getreadbuffer,
    (writebufferproc) NULL,
    (segcountproc) chunk_getsegcount,
    (charbufferproc) NULL
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
typecast_BINARY_cast(const char *s, Py_ssize_t l, PyObject *curs)
{
    chunkObject *chunk = NULL;
    PyObject *res = NULL;
    char *str = NULL, *buffer = NULL;
    size_t len;

    if (s == NULL) {Py_INCREF(Py_None); return Py_None;}

    /* PQunescapeBytea absolutely wants a 0-terminated string and we don't
       want to copy the whole buffer, right? Wrong, but there isn't any other
       way <g> */
    if (s[l] != '\0') {
        if ((buffer = PyMem_Malloc(l+1)) == NULL) {
            PyErr_NoMemory();
            goto fail;
        }
        /* Py_ssize_t->size_t cast is safe, as long as the Py_ssize_t is
         * >= 0: */
        assert (l >= 0);
        strncpy(buffer, s, (size_t) l);

        buffer[l] = '\0';
        s = buffer;
    }
    str = (char*)PQunescapeBytea((unsigned char*)s, &len);
    Dprintf("typecast_BINARY_cast: unescaped " FORMAT_CODE_SIZE_T " bytes",
      len);

    /* The type of the second parameter to PQunescapeBytea is size_t *, so it's
     * possible (especially with Python < 2.5) to get a return value too large
     * to fit into a Python container. */
    if (len > (size_t) PY_SSIZE_T_MAX) {
      PyErr_SetString(PyExc_IndexError, "PG buffer too large to fit in Python"
                                        " buffer.");
      goto fail;
    }

    chunk = (chunkObject *) PyObject_New(chunkObject, &chunkType);
    if (chunk == NULL) goto fail;

    /* **Transfer** ownership of str's memory to the chunkObject: */
    chunk->base = str;
    str = NULL;

    /* size_t->Py_ssize_t cast was validated above: */
    chunk->len = (Py_ssize_t) len;
    if ((res = PyBuffer_FromObject((PyObject *)chunk, 0, chunk->len)) == NULL)
        goto fail;
    /* PyBuffer_FromObject() created a new reference.  We'll release our
     * reference held in 'chunk' in the 'cleanup' clause. */

    goto cleanup;
    fail:
      assert (PyErr_Occurred());
      if (res != NULL) {
          Py_DECREF(res);
          res = NULL;
      }
      /* Fall through to cleanup: */
    cleanup:
      if (chunk != NULL) {
          Py_DECREF((PyObject *) chunk);
      }
      if (str != NULL) {
          /* str's mem was allocated by PQunescapeBytea; must use free: */
          free(str);
      }
      if (buffer != NULL) {
          /* We allocated buffer with PyMem_Malloc; must use PyMem_Free: */
          PyMem_Free(buffer);
      }

      return res;
}
