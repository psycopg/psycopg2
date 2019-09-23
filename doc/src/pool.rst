`psycopg2.pool` -- Connections pooling
======================================

.. sectionauthor:: Daniele Varrazzo <daniele.varrazzo@gmail.com>

.. index::
    pair: Connection; Pooling

.. module:: psycopg2.pool

Creating new PostgreSQL connections can be an expensive operation.  This
module offers a few pure Python classes implementing simple connection pooling
directly in the client application.

.. class:: AbstractConnectionPool(minconn, maxconn, \*args, idle_timeout=0, \*\*kwargs)

    Base class implementing generic key-based pooling code.

    New *minconn* connections are created automatically. The pool will support
    a maximum of about *maxconn* connections.  *\*args* and *\*\*kwargs* are
    passed to the `~psycopg2.connect()` function.

    Connections are kept in the pool for at most *idle_timeout* seconds. There
    are two special values: zero means that connections are always immediately
    closed upon their return to the pool; `None` means that connections are kept
    indefinitely (leaving the server in charge of closing idle connections). The
    current default value is zero because it replicates the behavior of previous
    versions, however the default value may be changed in a future release.

    .. versionadded:: 2.9 the *idle_timeout* argument.

    The following methods are expected to be implemented by subclasses:

    .. method:: getconn(key=None)

        Get a free connection from the pool.

        The *key* parameter is optional: if used, the connection will be
        associated to the key and calling `!getconn()` with the same key again
        will return the same connection.

    .. method:: putconn(conn, key=None, close=False)

        Put away a connection.

        If *close* is `!True`, discard the connection from the pool.
        *key* should be used consistently with `getconn()`.

    .. method:: closeall

        Close all the connections handled by the pool.

        Note that all the connections are closed, including ones
        eventually in use by the application.

    .. method:: prune

        Drop all expired connections from the pool.

        You can call this method periodically to clean up the pool.

        .. versionadded:: 2.9


The following classes are `AbstractConnectionPool` subclasses ready to
be used.

.. autoclass:: SimpleConnectionPool

    .. note:: This pool class is useful only for single-threaded applications.


.. index:: Multithread; Connection pooling

.. autoclass:: ThreadedConnectionPool

    .. note:: This pool class can be safely used in multi-threaded applications.
