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



.. _sql-adaptation-objects:

SQL adaptation protocol objects
-------------------------------

Psycopg provides a flexible system to adapt Python objects to the SQL syntax
(inspired to the :pep:`246`), allowing serialization in PostgreSQL. See
:ref:`adapting-new-types` for a detailed description.  The following objects
deal with Python objects adaptation:

.. function:: adapt(obj)

    Return the SQL representation of :obj:`obj` as a string.  Raise a
    :exc:`ProgrammingError` if how to adapt the object is unknown.  In order
    to allow new objects to be adapted, register a new adapter for it using
    the :func:`register_adapter` function.

    The function is the entry point of the adaptation mechanism: it can be
    used to write adapters for complex objects by recursively calling
    :func:`adapt` on its components.

.. function:: register_adapter(class, adapter)

    Register a new adapter for the objects of class :data:`class`.

    :data:`adapter` should be a function taking a single argument (the object
    to adapt) and returning an object conforming the :class:`ISQLQuote`
    protocol (e.g. exposing a :meth:`getquoted` method).  The :class:`AsIs` is
    often useful for this task.

    Once an object is registered, it can be safely used in SQL queries and by
    the :func:`adapt` function.

.. class:: ISQLQuote

    Represents the SQL adaptation protocol. Objects conforming this protocol
    should implement a :meth:`getquoted` method.

    .. todo:: has Psycopg user ever to explicitely use this object?

    .. todo::
        what the ISQLQuote methods are for? In my understanding the
        class is only used as symbol to dispatch adaptation and not to be
        instantiated.

.. class:: AsIs

    Adapter conform to the :class:`ISQLQuote` protocol useful for objects
    whose string representation is already valid as SQL representation.

    .. method:: getquoted()

        Return the ``str()`` conversion of the wrapped object. ::

            >>> AsIs(42).getquoted()
            '42'

.. class:: QuotedString

    Adapter conform to the :class:`ISQLQuote` protocol for string-like
    objects.

    .. method:: getquoted()

        Return the string enclosed in single quotes.  Any single quote
        appearing in the the string is escaped by doubling it according to SQL
        string constants syntax.  Backslashes are escaped too.

            >>> QuotedString(r"O'Reilly").getquoted()
            "'O''Reilly'"

.. class:: Binary

    Adapter conform to the :class:`ISQLQuote` protocol for binary objects.

    .. method:: getquoted()

        Return the string enclosed in single quotes.  It performs the same
        escaping of the :class:`QuotedString` adapter, plus it knows how to
        escape non-printable chars.

            >>> Binary("\x00\x08\x0F").getquoted()
            "'\\\\000\\\\010\\\\017'"

    .. todo::

        this class is actually not importd in module extensions: I'd say this
        is a bug.


.. data:: Boolean
.. data:: Float

Specialized adapters for builtin objects.

.. data:: DateFromPy
.. data:: TimeFromPy
.. data:: TimestampFromPy
.. data:: IntervalFromPy

Specialized adapters for Python datetime objects.

.. data:: DateFromMx
.. data:: TimeFromMx
.. data:: TimestampFromMx
.. data:: IntervalFromMx

Specialized adapters for `mx.DateTime`_ objects.

.. data:: adapters

    Dictionary of the currently registered object adapters.  Use
    :func:`register_adapter` to add an adapter for a new type.



Database types casting functions
--------------------------------

These functions are used to manipulate type casters to convert from PostgreSQL
types to Python objects.  See :ref:`type-casting-from-sql-to-python` for
details.

.. function:: new_type(oids, name, adapter)

    Create a new type caster to convert from a PostgreSQL type to a Python
    object.  The created object must be registered using
    :func:`register_type` to be used.

    :param oids: tuple of OIDs of the PostgreSQL type to convert.
    :param name: the name of the new type adapter.
    :param adapter: the adaptation function.

    The object OID can be read from the :data:`cursor.description` or directly
    from the PostgreSQL catalog.

    :data:`adapter` should have signature ``fun(value, cur)`` where
    ``value`` is the string representation returned by PostgreSQL and ``cur``
    is the cursor from which data are read. In case of ``NULL``, ``value`` is
    ``None``. The adapter should return the converted object.

    See :ref:`type-casting-from-sql-to-python` for an usage example.

.. function:: register_type(obj [, scope])

    Register a type caster created using :func:`new_type`.

    If :obj:`scope` is specified, it should be a :class:`connection` or a
    :class:`cursor`: the type caster will be effective only limited to the
    specified object.  Otherwise it will be globally registered.

    .. todo:: Please confirm the above behaviour.

.. data:: string_types

    The global register of type casters.

.. data:: binary_types

    .. todo:: is this used?



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


