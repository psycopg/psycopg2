`psycopg2.extensions` -- Extensions to the DB API
======================================================

.. sectionauthor:: Daniele Varrazzo <daniele.varrazzo@gmail.com>

.. module:: psycopg2.extensions

.. testsetup:: *

    from psycopg2.extensions import AsIs, Binary, QuotedString, ISOLATION_LEVEL_AUTOCOMMIT

The module contains a few objects and function extending the minimum set of
functionalities defined by the |DBAPI|_.


.. class:: connection(dsn, async=False)

    Is the class usually returned by the `~psycopg2.connect()` function.
    It is exposed by the `extensions` module in order to allow
    subclassing to extend its behaviour: the subclass should be passed to the
    `!connect()` function using the `connection_factory` parameter.
    See also :ref:`subclassing-connection`.

    For a complete description of the class, see `connection`.

.. class:: cursor(conn, name=None)

    It is the class usually returned by the `connection.cursor()`
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

        The mode the database was open. See `connection.lobject()` for a
        description of the available modes.

    .. method:: read(bytes=-1)

        Read a chunk of data from the current file position. If -1 (default)
        read all the remaining data.

        The result is an Unicode string (decoded according to
        `connection.encoding`) if the file was open in ``t`` mode, a bytes
        string for ``b`` mode.

        .. versionchanged:: 2.4
            added Unicode support.

    .. method:: write(str)

        Write a string to the large object. Return the number of bytes
        written. Unicode strings are encoded in the `connection.encoding`
        before writing.

        .. versionchanged:: 2.4
            added Unicode support.

    .. method:: export(file_name)

        Export the large object content to the file system.
        
        The method uses the efficient |lo_export|_ libpq function.
        
        .. |lo_export| replace:: `!lo_export()`
        .. _lo_export: http://www.postgresql.org/docs/current/static/lo-interfaces.html#LO-EXPORT

    .. method:: seek(offset, whence=0)

        Set the lobject current position.

    .. method:: tell()

        Return the lobject current position.

    .. method:: truncate(len=0)

        .. versionadded:: 2.2.0

        Truncate the lobject to the given size.

        The method will only be available if Psycopg has been built against libpq
        from PostgreSQL 8.3 or later and can only be used with PostgreSQL servers
        running these versions. It uses the |lo_truncate|_ libpq function.

        .. |lo_truncate| replace:: `!lo_truncate()`
        .. _lo_truncate: http://www.postgresql.org/docs/current/static/lo-interfaces.html#LO-TRUNCATE

        .. warning::

            If Psycopg is built with |lo_truncate| support (i.e. if the
            :program:`pg_config` used during setup is version >= 8.3), but at
            runtime an older libpq is found, Psycopg will fail to import.  See
            :ref:`the lo_truncate FAQ <faq-lo_truncate>` about the problem.

    .. method:: close()

        Close the object.

    .. attribute:: closed

        Boolean attribute specifying if the object is closed.

    .. method:: unlink()

        Close the object and remove it from the database.


.. autoclass:: Notify(pid, channel, payload='')
    :members: pid, channel, payload

    .. versionadded:: 2.3


.. autoclass:: Xid(format_id, gtrid, bqual)
    :members: format_id, gtrid, bqual, prepared, owner, database

    .. versionadded:: 2.3

    .. automethod:: from_string(s)


.. autoclass:: Diagnostics(exception)

    .. versionadded:: 2.5

    The attributes currently available are:

    .. attribute::
        column_name
        constraint_name
        context
        datatype_name
        internal_position
        internal_query
        message_detail
        message_hint
        message_primary
        schema_name
        severity
        source_file
        source_function
        source_line
        sqlstate
        statement_position
        table_name

        A string with the error field if available; `!None` if not available.
        The attribute value is available only if the error sent by the server:
        not all the fields are available for all the errors and for all the
        server versions.


.. autofunction:: set_wait_callback(f)

    .. versionadded:: 2.2.0

.. autofunction:: get_wait_callback()

    .. versionadded:: 2.2.0


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
    should implement a `getquoted()` and optionally a `prepare()` method.

    Adapters may subclass `!ISQLQuote`, but is not necessary: it is
    enough to expose a `!getquoted()` method to be conforming.

    .. attribute:: _wrapped

        The wrapped object passes to the constructor

    .. method:: getquoted()

        Subclasses or other conforming objects should return a valid SQL
        string representing the wrapped object. In Python 3 the SQL must be
        returned in a `!bytes` object. The `!ISQLQuote` implementation does
        nothing.

    .. method:: prepare(conn)

        Prepare the adapter for a connection.  The method is optional: if
        implemented, it will be invoked before `!getquoted()` with the
        connection to adapt for as argument.

        A conform object can implement this method if the SQL
        representation depends on any server parameter, such as the server
        version or the :envvar:`standard_conforming_string` setting.  Container
        objects may store the connection and use it to recursively prepare
        contained objects: see the implementation for
        `psycopg2.extensions.SQL_IN` for a simple example.


.. class:: AsIs(object)

    Adapter conform to the `ISQLQuote` protocol useful for objects
    whose string representation is already valid as SQL representation.

    .. method:: getquoted()

        Return the `str()` conversion of the wrapped object.

            >>> AsIs(42).getquoted()
            '42'

.. class:: QuotedString(str)

    Adapter conform to the `ISQLQuote` protocol for string-like
    objects.

    .. method:: getquoted()

        Return the string enclosed in single quotes.  Any single quote
        appearing in the the string is escaped by doubling it according to SQL
        string constants syntax.  Backslashes are escaped too.

            >>> QuotedString(r"O'Reilly").getquoted()
            "'O''Reilly'"

.. class:: Binary(str)

    Adapter conform to the `ISQLQuote` protocol for binary objects.

    .. method:: getquoted()

        Return the string enclosed in single quotes.  It performs the same
        escaping of the `QuotedString` adapter, plus it knows how to
        escape non-printable chars.

            >>> Binary("\x00\x08\x0F").getquoted()
            "'\\\\000\\\\010\\\\017'"

    .. versionchanged:: 2.0.14
        previously the adapter was not exposed by the `extensions`
        module. In older versions it can be imported from the implementation
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
    object.  The object created must be registered using
    `register_type()` to be used.

    :param oids: tuple of OIDs of the PostgreSQL type to convert.
    :param name: the name of the new type adapter.
    :param adapter: the adaptation function.

    The object OID can be read from the `cursor.description` attribute
    or by querying from the PostgreSQL catalog.

    *adapter* should have signature :samp:`fun({value}, {cur})` where
    *value* is the string representation returned by PostgreSQL and
    *cur* is the cursor from which data are read. In case of
    :sql:`NULL`, *value* will be `!None`. The adapter should return the
    converted object.

    See :ref:`type-casting-from-sql-to-python` for an usage example.


.. function:: new_array_type(oids, name, base_caster)

    Create a new type caster to convert from a PostgreSQL array type to a list
    of Python object.  The object created must be registered using
    `register_type()` to be used.

    :param oids: tuple of OIDs of the PostgreSQL type to convert. It should
        probably contain the oid of the array type (e.g. the ``typarray``
        field in the ``pg_type`` table).
    :param name: the name of the new type adapter.
    :param base_caster: a Psycopg typecaster, e.g. created using the
        `new_type()` function. The caster should be able to parse a single
        item of the desired type.

    .. versionadded:: 2.4.3

    .. _cast-array-unknown:

    .. note::

        The function can be used to create a generic array typecaster,
        returning a list of strings: just use `psycopg2.STRING` as base
        typecaster. For instance, if you want to receive an array of
        :sql:`macaddr` from the database, each address represented by string,
        you can use::

            # select typarray from pg_type where typname = 'macaddr' -> 1040
            psycopg2.extensions.register_type(
                psycopg2.extensions.new_array_type(
                    (1040,), 'MACADDR[]', psycopg2.STRING))


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

    .. __: http://www.postgresql.org/docs/current/static/multibyte.html
    .. __: http://docs.python.org/library/codecs.html#standard-encodings



.. index::
    single: Exceptions; Additional

Additional exceptions
---------------------

The module exports a few exceptions in addition to the :ref:`standard ones
<dbapi-exceptions>` defined by the |DBAPI|_.

.. exception:: QueryCanceledError

    (subclasses `~psycopg2.OperationalError`)

    Error related to SQL query cancellation.  It can be trapped specifically to
    detect a timeout.

    .. versionadded:: 2.0.7


.. exception:: TransactionRollbackError

    (subclasses `~psycopg2.OperationalError`)

    Error causing transaction rollback (deadlocks, serialization failures,
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

    This is usually the the default PostgreSQL value, but a different default
    may be set in the database configuration.

    A new transaction is started at the first `~cursor.execute()` command on a
    cursor and at each new `!execute()` after a `~connection.commit()` or a
    `~connection.rollback()`.  The transaction runs in the PostgreSQL
    :sql:`READ COMMITTED` isolation level: a :sql:`SELECT` query sees only
    data committed before the query began; it never sees either uncommitted
    data or changes committed during query execution by concurrent
    transactions.

    .. seealso:: `Read Committed Isolation Level`__ in PostgreSQL
        documentation.

        .. __: http://www.postgresql.org/docs/current/static/transaction-iso.html#XACT-READ-COMMITTED

.. data:: ISOLATION_LEVEL_REPEATABLE_READ

    As in `!ISOLATION_LEVEL_READ_COMMITTED`, a new transaction is started at
    the first `~cursor.execute()` command.  Transactions run at a
    :sql:`REPEATABLE READ` isolation level: all the queries in a transaction
    see a snapshot as of the start of the transaction, not as of the start of
    the current query within the transaction.  However applications using this
    level must be prepared to retry transactions due to serialization
    failures.

    While this level provides a guarantee that each transaction sees a
    completely stable view of the database, this view will not necessarily
    always be consistent with some serial (one at a time) execution of
    concurrent transactions of the same level.

    .. versionchanged:: 2.4.2
        The value was an alias for `!ISOLATION_LEVEL_SERIALIZABLE` before. The
        two levels are distinct since PostgreSQL 9.1

    .. seealso:: `Repeatable Read Isolation Level`__ in PostgreSQL
        documentation.

        .. __: http://www.postgresql.org/docs/current/static/transaction-iso.html#XACT-REPEATABLE-READ

.. data:: ISOLATION_LEVEL_SERIALIZABLE

    As in `!ISOLATION_LEVEL_READ_COMMITTED`, a new transaction is started at
    the first `~cursor.execute()` command.  Transactions run at a
    :sql:`SERIALIZABLE` isolation level. This is the strictest transactions
    isolation level, equivalent to having the transactions executed serially
    rather than concurrently. However applications using this level must be
    prepared to retry transactions due to serialization failures.

    Starting from PostgreSQL 9.1, this mode monitors for conditions which
    could make execution of a concurrent set of serializable transactions
    behave in a manner inconsistent with all possible serial (one at a time)
    executions of those transaction. In previous version the behaviour was the
    same of the :sql:`REPEATABLE READ` isolation level.

    .. seealso:: `Serializable Isolation Level`__ in PostgreSQL documentation.

        .. __: http://www.postgresql.org/docs/current/static/transaction-iso.html#XACT-SERIALIZABLE



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

It is possible to find the connection in other status than the one shown below.
Those are the only states in which a working connection is expected to be found
during the execution of regular Python client code: other states are for
internal usage and Python code should not rely on them.

.. data:: STATUS_READY

    Connection established. No transaction in progress.

.. data:: STATUS_BEGIN

    Connection established. A transaction is currently in progress.

.. data:: STATUS_IN_TRANSACTION

    An alias for `STATUS_BEGIN`

.. data:: STATUS_PREPARED

    The connection has been prepared for the second phase in a :ref:`two-phase
    commit <tpc>` transaction. The connection can't be used to send commands
    to the database until the transaction is finished with
    `~connection.tpc_commit()` or `~connection.tpc_rollback()`.

    .. versionadded:: 2.3



.. index::
    pair: Poll status; Constants

.. _poll-constants:

Poll constants
--------------

.. versionadded:: 2.2.0

These values can be returned by `connection.poll()` during asynchronous
connection and communication.  They match the values in the libpq enum
`!PostgresPollingStatusType`.  See :ref:`async-support` and
:ref:`green-support`.

.. data:: POLL_OK

    The data being read is available, or the file descriptor is ready for
    writing: reading or writing will not block.

.. data:: POLL_READ

    Some data is being read from the backend, but it is not available yet on
    the client and reading would block.  Upon receiving this value, the client
    should wait for the connection file descriptor to be ready *for reading*.
    For example::

        select.select([conn.fileno()], [], [])

.. data:: POLL_WRITE

    Some data is being sent to the backend but the connection file descriptor
    can't currently accept new data.  Upon receiving this value, the client
    should wait for the connection file descriptor to be ready *for writing*.
    For example::

        select.select([], [conn.fileno()], [])

.. data:: POLL_ERROR

    There was a problem during connection polling. This value should actually
    never be returned: in case of poll error usually an exception containing
    the relevant details is raised.



Additional database types
-------------------------

The `!extensions` module includes typecasters for many standard
PostgreSQL types.  These objects allow the conversion of returned data into
Python objects.  All the typecasters are automatically registered, except
`UNICODE` and `UNICODEARRAY`: you can register them using
`register_type()` in order to receive Unicode objects instead of strings
from the database.  See :ref:`unicode-handling` for details.

.. data:: BOOLEAN
          DATE
          DECIMAL
          FLOAT
          INTEGER
          INTERVAL
          LONGINTEGER
          TIME
          UNICODE

    Typecasters for basic types. Note that a few other ones (`~psycopg2.BINARY`,
    `~psycopg2.DATETIME`, `~psycopg2.NUMBER`, `~psycopg2.ROWID`,
    `~psycopg2.STRING`) are exposed by the `psycopg2` module for |DBAPI|_
    compliance.

.. data:: BINARYARRAY
          BOOLEANARRAY
          DATEARRAY
          DATETIMEARRAY
          DECIMALARRAY
          FLOATARRAY
          INTEGERARRAY
          INTERVALARRAY
          LONGINTEGERARRAY
          ROWIDARRAY
          STRINGARRAY
          TIMEARRAY
          UNICODEARRAY

    Typecasters to convert arrays of sql types into Python lists.

.. data:: PYDATE
          PYDATETIME
          PYINTERVAL
          PYTIME
          PYDATEARRAY
          PYDATETIMEARRAY
          PYINTERVALARRAY
          PYTIMEARRAY

    Typecasters to convert time-related data types to Python `!datetime`
    objects.

.. data:: MXDATE
          MXDATETIME
          MXINTERVAL
          MXTIME
          MXDATEARRAY
          MXDATETIMEARRAY
          MXINTERVALARRAY
          MXTIMEARRAY

    Typecasters to convert time-related data types to `mx.DateTime`_ objects.
    Only available if Psycopg was compiled with `!mx` support.

.. versionchanged:: 2.2.0
        previously the `DECIMAL` typecaster and the specific time-related
        typecasters (`!PY*` and `!MX*`) were not exposed by the `extensions`
        module. In older versions they can be imported from the implementation
        module `!psycopg2._psycopg`.

