`psycopg2.pool` -- Connections pooling
======================================

.. sectionauthor:: Daniele Varrazzo <daniele.varrazzo@gmail.com>

.. index::
    pair: Connection; Pooling

.. module:: psycopg2.pool

Creating new PostgreSQL connections can be an expensive operation.  This
module offers a few pure Python classes implementing simple connection pooling
directly in the client application.

.. class:: AbstractConnectionPool(minconn, maxconn, \*args, \*\*kwargs)

    Base class implementing generic key-based pooling code.

    New *minconn* connections are created automatically. The pool will support
    a maximum of about *maxconn* connections.  *\*args* and *\*\*kwargs* are
    passed to the `~psycopg2.connect()` function.

    The following methods are expected to be implemented by subclasses:

    .. method:: getconn(key=None)

        Get a free connection and assign it to *key* if not `!None`.

    .. method:: putconn(conn, key=None, close=False)

        Put away a connection.

        If *close* is `!True`, discard the connection from the pool.

    .. method:: closeall

        Close all the connections handled by the pool.

        Note that all the connections are closed, including ones
        eventually in use by the application.


The following classes are `AbstractConnectionPool` subclasses ready to
be used.

.. autoclass:: SimpleConnectionPool

    .. note:: This pool class is useful only for single-threaded applications.


.. index:: Multithread; Connection pooling

.. autoclass:: ThreadedConnectionPool

    .. note:: This pool class can be safely used in multi-threaded applications.


.. autoclass:: PersistentConnectionPool

    .. note:: 

        This pool class is mostly designed to interact with Zope and probably
        not useful in generic applications.

