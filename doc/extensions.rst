:mod:`psycopg2.extensions` -- Extensions to the DB API
======================================================

.. sectionauthor:: Daniele Varrazzo <daniele.varrazzo@gmail.com>

.. module:: psycopg2.extensions

The module contains a few objects and function extending the minimum set of
functionalities defined by the |DBAPI|.


.. class:: connection

    Is the class usually returned by the :func:`psycopg2.connect()` function.
    It is exposed by the :mod:`extensions` module in order to allow
    subclassing to extend its behaviour: the subclass should be passed to the
    :func:`connect()` function using the :obj:`connection_factory` parameter.
    See also :ref:`subclassing-connection`.

    For a complete description of the class, see :class:`connection`.

.. class:: cursor

    It is the class usually returnded by the :meth:`connection.cursor()`
    method. It is exposed by the :mod:`extensions` module in order to allow
    subclassing to extend its behaviour: the subclass should be passed to the
    ``cursor()`` method using the :obj:`cursor_factory` parameter. See
    also :ref:`subclassing-cursor`.

    For a complete description of the class, see :class:`cursor`.

    .. todo:: row factories

.. class:: lobject

    .. todo:: class lobject


.. todo:: finish module extensions



.. index::
    pair: Isolation level; Constants

.. _isolation-level-constants:

Isolation level constants
-------------------------

Psycopg2 connection objects hold informations about the PostgreSQL
`transaction isolation level`_.  The current transaction level can be read
from the :attr:`connection.isolation_level` attribute.  The default isolation
level is ``READ COMMITTED``.  A different isolation level con be set through
the :meth:`connection.set_isolation_level()` method.  The level can be set to
one of the following constants:

.. data:: ISOLATION_LEVEL_AUTOCOMMIT

    No transaction is started when command are issued and no ``commit()`` or
    ``rollback()`` is required.  Some PostgreSQL command such as ``CREATE
    DATABASE`` can't run into a transaction: to run such command use::
    
        >>> conn.set_isolation_level(ISOLATION_LEVEL_AUTOCOMMIT)
    
.. data:: ISOLATION_LEVEL_READ_UNCOMMITTED

    This isolation level is defined in the SQL standard but not available in
    the MVCC model of PostgreSQL: it is replaced by the stricter ``READ
    COMMITTED``.

.. data:: ISOLATION_LEVEL_READ_COMMITTED

    This is the default value.  A new transaction is started at the first
    :meth:`cursor.execute()` command on a cursor and at each new ``execute()``
    after a :meth:`connection.commit()` or a :meth:`connection.rollback()`.
    The transaction runs in the PostgreSQL ``READ COMMITTED`` isolation level.
    
.. data:: ISOLATION_LEVEL_REPEATABLE_READ

    This isolation level is defined in the SQL standard but not available in
    the MVCC model of PostgreSQL: it is replaced by the stricter
    ``SERIALIZABLE``.
    
.. data:: ISOLATION_LEVEL_SERIALIZABLE

    Transactions are run at a ``SERIALIZABLE`` isolation level. This is the
    strictest transactions isolation level, equivalent to having the
    transactions executed serially rather than concurrently. However
    applications using this level must be prepared to retry reansactions due
    to serialization failures. See `serializable isolation level`_ in
    PostgreSQL documentation.



.. index::
    pair: Transaction status; Constants

.. _transaction-status-constants:

Transaction status constants
----------------------------

These values represent the possible status of a transaction: the current value
can be read using the :meth:`connection.get_transaction_status()` method.

.. data:: TRANSACTION_STATUS_IDLE

    The session is idle and there is no current transaction.

.. data:: TRANSACTION_STATUS_ACTIVE

    A command is currently in progress.

.. data:: TRANSACTION_STATUS_INTRANS

    The session is idle in a valid transaction block.

.. data:: TRANSACTION_STATUS_INERROR

    The session is idle in a failed transaction block.

.. data:: TRANSACTION_STATUS_UNKNOWN

    Reported if the connection with the server is bad.



.. index::
    pair: Connection status; Constants

.. _connection-status-constants:

Connection status constants
---------------------------

These values represent the possible status of a connection: the current value
can be read from the :data:`connection.status` attribute.

.. todo:: check if these values are really useful or not.

.. data:: STATUS_SETUP

    Defined but not used.

.. data:: STATUS_READY

    Connection established.

.. data:: STATUS_BEGIN

    Connection established. A transaction is in progress.

.. data:: STATUS_IN_TRANSACTION

    An alias for :data:`STATUS_BEGIN`

.. data:: STATUS_SYNC

    Defined but not used.

.. data:: STATUS_ASYNC

    Defined but not used.


