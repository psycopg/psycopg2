`psycopg2.extensions` -- Extensions to the DB API
======================================================

.. sectionauthor:: Daniele Varrazzo <daniele.varrazzo@gmail.com>

.. module:: psycopg2.extensions

.. testsetup:: *

    from psycopg2.extensions import AsIs, Binary, QuotedString, ISOLATION_LEVEL_AUTOCOMMIT

The module contains a few objects and function extending the minimum set of
functionalities defined by the |DBAPI|_.


.. class:: connection

    Is the class usually returned by the `~psycopg2.connect()` function.
    It is exposed by the `extensions` module in order to allow
    subclassing to extend its behaviour: the subclass should be passed to the
    `!connect()` function using the `connection_factory` parameter.
    See also :ref:`subclassing-connection`.

    For a complete description of the class, see `connection`.

.. class:: cursor

    It is the class usually returnded by the `connection.cursor()`
    method. It is exposed by the `extensions` module in order to allow
    subclassing to extend its behaviour: the subclass should be passed to the
    `!cursor()` method using the `cursor_factory` parameter. See
    also :ref:`subclassing-cursor`.

    For a complete description of the class, see `cursor`.

.. class:: lobject(conn [, oid [, mode [, new_oid [, new_file ]]]])

    Wrapper for a PostgreSQL large object. See :ref:`large-objects` for an
    overview.

    The class can be subclassed: see the `connection.lobject()` to know
    how to specify a `!lobject` subclass.
    
    .. versionadded:: 2.0.8

    .. attribute:: oid

        Database OID of the object.

    .. attribute:: mode

        The mode the database was open (``r``, ``w``, ``rw`` or ``n``).

    .. method:: read(bytes=-1)

        Read a chunk of data from the current file position. If -1 (default)
        read all the remaining data.

    .. method:: write(str)

        Write a string to the large object. Return the number of bytes
        written.

    .. method:: export(file_name)

        Export the large object content to the file system.
        
        The method uses the efficient |lo_export|_ libpq function.
        
        .. |lo_export| replace:: `!lo_export()`
        .. _lo_export: http://www.postgresql.org/docs/8.4/static/lo-interfaces.html#AEN36330

    .. method:: seek(offset, whence=0)

        Set the lobject current position.

    .. method:: tell()

        Return the lobject current position.

    .. method:: close()

        Close the object.

    .. attribute:: closed

        Boolean attribute specifying if the object is closed.

    .. method:: unlink()

        Close the object and remove it from the database.



.. _sql-adaptation-objects:

SQL adaptation protocol objects
-------------------------------

Psycopg provides a flexible system to adapt Python objects to the SQL syntax
(inspired to the :pep:`246`), allowing serialization in PostgreSQL. See
:ref:`adapting-new-types` for a detailed description.  The following objects
deal with Python objects adaptation:

.. function:: adapt(obj)

    Return the SQL representation of *obj* as a string.  Raise a
    `~psycopg2.ProgrammingError` if how to adapt the object is unknown.
    In order to allow new objects to be adapted, register a new adapter for it
    using the `register_adapter()` function.

    The function is the entry point of the adaptation mechanism: it can be
    used to write adapters for complex objects by recursively calling
    `!adapt()` on its components.

.. function:: register_adapter(class, adapter)

    Register a new adapter for the objects of class *class*.

    *adapter* should be a function taking a single argument (the object
    to adapt) and returning an object conforming the `ISQLQuote`
    protocol (e.g. exposing a `!getquoted()` method).  The `AsIs` is
    often useful for this task.

    Once an object is registered, it can be safely used in SQL queries and by
    the `adapt()` function.

.. class:: ISQLQuote(wrapped_object)

    Represents the SQL adaptation protocol.  Objects conforming this protocol
    should implement a `!getquoted()` method.

    Adapters may subclass `!ISQLQuote`, but is not necessary: it is
    enough to expose a `!getquoted()` method to be conforming.

    .. attribute:: _wrapped

        The wrapped object passes to the constructor

    .. method:: getquoted()

        Subclasses or other conforming objects should return a valid SQL
        string representing the wrapped object. The `!ISQLQuote`
        implementation does nothing.

.. class:: AsIs

    Adapter conform to the `ISQLQuote` protocol useful for objects
    whose string representation is already valid as SQL representation.

    .. method:: getquoted()

        Return the `str()` conversion of the wrapped object.

            >>> AsIs(42).getquoted()
            '42'

.. class:: QuotedString

    Adapter conform to the `ISQLQuote` protocol for string-like
    objects.

    .. method:: getquoted()

        Return the string enclosed in single quotes.  Any single quote
        appearing in the the string is escaped by doubling it according to SQL
        string constants syntax.  Backslashes are escaped too.

            >>> QuotedString(r"O'Reilly").getquoted()
            "'O''Reilly'"

.. class:: Binary

    Adapter conform to the `ISQLQuote` protocol for binary objects.

    .. method:: getquoted()

        Return the string enclosed in single quotes.  It performs the same
        escaping of the `QuotedString` adapter, plus it knows how to
        escape non-printable chars.

            >>> Binary("\x00\x08\x0F").getquoted()
            "'\\\\000\\\\010\\\\017'"

    .. versionchanged:: 2.0.14
        previously the adapter was not exposed by the `extensions`
        module. In older version it can be imported from the implementation
        module `!psycopg2._psycopg`.



.. class:: Boolean
           Float
           SQL_IN

        Specialized adapters for builtin objects.

.. class:: DateFromPy
           TimeFromPy
           TimestampFromPy
           IntervalFromPy

        Specialized adapters for Python datetime objects.

.. class:: DateFromMx
           TimeFromMx
           TimestampFromMx
           IntervalFromMx

        Specialized adapters for `mx.DateTime`_ objects.

.. data:: adapters

    Dictionary of the currently registered object adapters.  Use
    `register_adapter()` to add an adapter for a new type.



Database types casting functions
--------------------------------

These functions are used to manipulate type casters to convert from PostgreSQL
types to Python objects.  See :ref:`type-casting-from-sql-to-python` for
details.

.. function:: new_type(oids, name, adapter)

    Create a new type caster to convert from a PostgreSQL type to a Python
    object.  The created object must be registered using
    `register_type()` to be used.

    :param oids: tuple of OIDs of the PostgreSQL type to convert.
    :param name: the name of the new type adapter.
    :param adapter: the adaptation function.

    The object OID can be read from the `cursor.description` attribute
    or by querying from the PostgreSQL catalog.

    *adapter* should have signature :samp:`fun({value}, {cur})` where
    *value* is the string representation returned by PostgreSQL and
    *cur* is the cursor from which data are read. In case of
    :sql:`NULL`, *value* will be ``None``. The adapter should return the
    converted object.

    See :ref:`type-casting-from-sql-to-python` for an usage example.


.. function:: register_type(obj [, scope])

    Register a type caster created using `new_type()`.

    If *scope* is specified, it should be a `connection` or a
    `cursor`: the type caster will be effective only limited to the
    specified object.  Otherwise it will be globally registered.


.. data:: string_types

    The global register of type casters.


.. index::
    single: Encoding; Mapping

.. data:: encodings

    Mapping from `PostgreSQL encoding`__ names to `Python codec`__ names.
    Used by Psycopg when adapting or casting unicode strings. See
    :ref:`unicode-handling`.

    .. __: http://www.postgresql.org/docs/8.4/static/multibyte.html
    .. __: http://docs.python.org/library/codecs.html#standard-encodings



.. index::
    single: Exceptions; Additional

Additional exceptions
---------------------

The module exports a few exceptions in addition to the :ref:`standard ones
<dbapi-exceptions>` defined by the |DBAPI|_.

.. exception:: QueryCanceledError

    (subclasses `~psycopg2.OperationalError`)

    Error related to SQL query cancelation.  It can be trapped specifically to
    detect a timeout.

    .. versionadded:: 2.0.7


.. exception:: TransactionRollbackError

    (subclasses `~psycopg2.OperationalError`)

    Error causing transaction rollback (deadlocks, serialisation failures,
    etc).  It can be trapped specifically to detect a deadlock.

    .. versionadded:: 2.0.7



.. index::
    pair: Isolation level; Constants

.. _isolation-level-constants:

Isolation level constants
-------------------------

Psycopg2 `connection` objects hold informations about the PostgreSQL
`transaction isolation level`_.  The current transaction level can be read
from the `~connection.isolation_level` attribute.  The default isolation
level is :sql:`READ COMMITTED`.  A different isolation level con be set
through the `~connection.set_isolation_level()` method.  The level can be
set to one of the following constants:

.. data:: ISOLATION_LEVEL_AUTOCOMMIT

    No transaction is started when command are issued and no
    `~connection.commit()` or `~connection.rollback()` is required.
    Some PostgreSQL command such as :sql:`CREATE DATABASE` or :sql:`VACUUM`
    can't run into a transaction: to run such command use::

        >>> conn.set_isolation_level(ISOLATION_LEVEL_AUTOCOMMIT)

    See also :ref:`transactions-control`.

.. data:: ISOLATION_LEVEL_READ_UNCOMMITTED

    The :sql:`READ UNCOMMITTED` isolation level is defined in the SQL standard
    but not available in the |MVCC| model of PostgreSQL: it is replaced by the
    stricter :sql:`READ COMMITTED`.

.. data:: ISOLATION_LEVEL_READ_COMMITTED

    This is the default value.  A new transaction is started at the first
    `~cursor.execute()` command on a cursor and at each new
    `!execute()` after a `~connection.commit()` or a
    `~connection.rollback()`.  The transaction runs in the PostgreSQL
    :sql:`READ COMMITTED` isolation level.

.. data:: ISOLATION_LEVEL_REPEATABLE_READ

    The :sql:`REPEATABLE READ` isolation level is defined in the SQL standard
    but not available in the |MVCC| model of PostgreSQL: it is replaced by the
    stricter :sql:`SERIALIZABLE`.

.. data:: ISOLATION_LEVEL_SERIALIZABLE

    Transactions are run at a :sql:`SERIALIZABLE` isolation level. This is the
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
can be read using the `connection.get_transaction_status()` method.

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
can be read from the `~connection.status` attribute.

.. data:: STATUS_SETUP

    Used internally.

.. data:: STATUS_READY

    Connection established.

.. data:: STATUS_BEGIN

    Connection established. A transaction is in progress.

.. data:: STATUS_IN_TRANSACTION

    An alias for `STATUS_BEGIN`

.. data:: STATUS_SYNC

    Used internally.

.. data:: STATUS_ASYNC

    Used internally.



Additional database types
-------------------------

The `!extensions` module includes typecasters for many standard
PostgreSQL types.  These objects allow the conversion of returned data into
Python objects.  All the typecasters are automatically registered, except
`UNICODE` and `UNICODEARRAY`: you can register them using
`register_type()` in order to receive Unicode objects instead of strings
from the database.  See :ref:`unicode-handling` for details.

.. data:: BINARYARRAY
          BOOLEAN
          BOOLEANARRAY
          DATE
          DATEARRAY
          DATETIMEARRAY
          DECIMALARRAY
          FLOAT
          FLOATARRAY
          INTEGER
          INTEGERARRAY
          INTERVAL
          INTERVALARRAY
          LONGINTEGER
          LONGINTEGERARRAY
          ROWIDARRAY
          STRINGARRAY
          TIME
          TIMEARRAY
          UNICODE
          UNICODEARRAY

