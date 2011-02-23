/* typecast_binary.c - binary typecasting functions to python types
 *
 * Copyright (C) 2001-2010 Federico Di Gregorio <fog@debian.org>
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

#include "typecast_binary.h"

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
    PQfreemem(self->base);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
chunk_repr(chunkObject *self)
{
    return PyString_FromFormat(
        "<memory chunk at %p size " FORMAT_CODE_PY_SSIZE_T ">",
        self->base, self->len
      );
}

#if PY_MAJOR_VERSION < 3

/* XXX support 3.0 buffer protocol */
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

#else

/* 3.0 buffer interface */
int chunk_getbuffer(PyObject *_self, Py_buffer *view, int flags)
{
    chunkObject *self = (chunkObject*)_self;
    return PyBuffer_FillInfo(view, _self, self->base, self->len, 1, flags);
}
static PyBufferProcs chunk_as_buffer =
{
    chunk_getbuffer,
    NULL,
};

#endif

#define chunk_doc "memory chunk"

PyTypeObject chunkType = {
    PyVarObject_HEAD_INIT(NULL, 0)
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

    /* Check the escaping was successful */
    if (s[0] == '\\' && s[1] == 'x'     /* input encoded in hex format */
        && str[0] == 'x'                /* output resulted in an 'x' */
        && s[2] != '7' && s[3] != '8')  /* input wasn't really an x (0x78) */
    {
        PyErr_SetString(InterfaceError,
            "can't receive bytea data from server >= 9.0 with the current "
            "libpq client library: please update the libpq to at least 9.0 "
            "or set bytea_output to 'escape' in the server config "
            "or with a query");
        goto fail;
    }

    chunk = (chunkObject *) PyObject_New(chunkObject, &chunkType);
    if (chunk == NULL) goto fail;

    /* **Transfer** ownership of str's memory to the chunkObject: */
    chunk->base = str;
    str = NULL;

    /* size_t->Py_ssize_t cast was validated above: */
    chunk->len = (Py_ssize_t) len;
#if PY_MAJOR_VERSION < 3
    if ((res = PyBuffer_FromObject((PyObject *)chunk, 0, chunk->len)) == NULL)
        goto fail;
#else
    if ((res = PyMemoryView_FromObject((PyObject*)chunk)) == NULL)
        goto fail;
#endif
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
          /* str's mem was allocated by PQunescapeBytea; must use PQfreemem: */
          PQfreemem(str);
      }
      /* We allocated buffer with PyMem_Malloc; must use PyMem_Free: */
      PyMem_Free(buffer);

      return res;
}
