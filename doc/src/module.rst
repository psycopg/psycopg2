The `psycopg2` module content
==================================

.. sectionauthor:: Daniele Varrazzo <daniele.varrazzo@gmail.com>

.. module:: psycopg2

The module interface respects the standard defined in the |DBAPI|_.

.. index::
    single: Connection string
    double: Connection; Parameters
    single: Username; Connection
    single: Password; Connection
    single: Host; Connection
    single: Port; Connection
    single: DSN (Database Source Name)

.. function::
    connect(dsn, connection_factory=None, cursor_factory=None, async=False)
    connect(\*\*kwargs, connection_factory=None, cursor_factory=None, async=False)

    Create a new database session and return a new `connection` object.

    The connection parameters can be specified either as a `libpq connection
    string`__ using the *dsn* parameter::

        conn = psycopg2.connect("dbname=test user=postgres password=secret")

    or using a set of keyword arguments::

        conn = psycopg2.connect(database="test", user="postgres", password="secret")

    The two call styles are mutually exclusive: you cannot specify connection
    parameters as keyword arguments together with a connection string; only
    the parameters not needed for the database connection (*i.e.*
    *connection_factory*, *cursor_factory*, and *async*) are supported
    together with the *dsn* argument.

    The basic connection parameters are:

    - `!dbname` -- the database name (only in the *dsn* string)
    - `!database` -- the database name (only as keyword argument)
    - `!user` -- user name used to authenticate
    - `!password` -- password used to authenticate
    - `!host` -- database host address (defaults to UNIX socket if not provided)
    - `!port` -- connection port number (defaults to 5432 if not provided)

    Any other connection parameter supported by the client library/server can
    be passed either in the connection string or as keywords. The PostgreSQL
    documentation contains the complete list of the `supported parameters`__.
    Also note that the same parameters can be passed to the client library
    using `environment variables`__.

    .. __:
    .. _connstring: http://www.postgresql.org/docs/current/static/libpq-connect.html#LIBPQ-CONNSTRING
    .. __:
    .. _connparams: http://www.postgresql.org/docs/current/static/libpq-connect.html#LIBPQ-PARAMKEYWORDS
    .. __:
    .. _connenvvars: http://www.postgresql.org/docs/current/static/libpq-envars.html

    Using the *connection_factory* parameter a different class or
    connections factory can be specified. It should be a callable object
    taking a *dsn* string argument. See :ref:`subclassing-connection` for
    details.  If a *cursor_factory* is specified, the connection's
    `~connection.cursor_factory` is set to it. If you only need customized
    cursors you can use this parameter instead of subclassing a connection.

    Using *async*\=\ `!True` an asynchronous connection will be created: see
    :ref:`async-support` to know about advantages and limitations.

    .. versionchanged:: 2.4.3
        any keyword argument is passed to the connection. Previously only the
        basic parameters (plus `!sslmode`) were supported as keywords.

    .. versionchanged:: 2.5
        added the *cursor_factory* parameter.

    .. seealso::

        - libpq `connection string syntax`__
        - libpq supported `connection parameters`__
        - libpq supported `environment variables`__

        .. __: connstring_
        .. __: connparams_
        .. __: connenvvars_

    .. extension::

        The parameters *connection_factory* and *async* are Psycopg extensions
        to the |DBAPI|.


.. data:: apilevel

    String constant stating the supported DB API level.  For `psycopg2` is
    ``2.0``.

.. data:: threadsafety

    Integer constant stating the level of thread safety the interface
    supports.  For `psycopg2` is ``2``, i.e. threads can share the module
    and the connection. See :ref:`thread-safety` for details.

.. data:: paramstyle

    String constant stating the type of parameter marker formatting expected
    by the interface.  For `psycopg2` is ``pyformat``.  See also
    :ref:`query-parameters`.



.. index:: 
    single: Exceptions; DB API

.. _dbapi-exceptions:

Exceptions
----------

In compliance with the |DBAPI|_, the module makes informations about errors
available through the following exceptions:

.. exception:: Warning 
            
    Exception raised for important warnings like data truncations while
    inserting, etc. It is a subclass of the Python `~exceptions.StandardError`.
            
.. exception:: Error 

    Exception that is the base class of all other error exceptions. You can
    use this to catch all errors with one single `!except` statement. Warnings
    are not considered errors and thus not use this class as base. It
    is a subclass of the Python `!StandardError`.

    .. attribute:: pgerror

        String representing the error message returned by the backend,
        `!None` if not available.

    .. attribute:: pgcode

        String representing the error code returned by the backend, `!None`
        if not available.  The `~psycopg2.errorcodes` module contains
        symbolic constants representing PostgreSQL error codes.

    .. doctest::
        :options: +NORMALIZE_WHITESPACE

        >>> try:
        ...     cur.execute("SELECT * FROM barf")
        ... except Exception, e:
        ...     pass

        >>> e.pgcode
        '42P01'
        >>> print e.pgerror
        ERROR:  relation "barf" does not exist
        LINE 1: SELECT * FROM barf
                              ^
    .. attribute:: cursor

        The cursor the exception was raised from; `None` if not applicable.

    .. attribute:: diag

        A `~psycopg2.extensions.Diagnostics` object containing further
        information about the error. ::

            >>> try:
            ...     cur.execute("SELECT * FROM barf")
            ... except Exception, e:
            ...     pass

            >>> e.diag.severity
            'ERROR'
            >>> e.diag.message_primary
            'relation "barf" does not exist'

        .. versionadded:: 2.5

    .. extension::

        The `~Error.pgerror`, `~Error.pgcode`, `~Error.cursor`, and
        `~Error.diag` attributes are Psycopg extensions.


.. exception:: InterfaceError

    Exception raised for errors that are related to the database interface
    rather than the database itself.  It is a subclass of `Error`.

.. exception:: DatabaseError

    Exception raised for errors that are related to the database.  It is a
    subclass of `Error`.
    
.. exception:: DataError
  
    Exception raised for errors that are due to problems with the processed
    data like division by zero, numeric value out of range, etc. It is a
    subclass of `DatabaseError`.
    
.. exception:: OperationalError
  
    Exception raised for errors that are related to the database's operation
    and not necessarily under the control of the programmer, e.g. an
    unexpected disconnect occurs, the data source name is not found, a
    transaction could not be processed, a memory allocation error occurred
    during processing, etc.  It is a subclass of `DatabaseError`.
    
.. exception:: IntegrityError             
  
    Exception raised when the relational integrity of the database is
    affected, e.g. a foreign key check fails.  It is a subclass of
    `DatabaseError`.
    
.. exception:: InternalError 
              
    Exception raised when the database encounters an internal error, e.g. the
    cursor is not valid anymore, the transaction is out of sync, etc.  It is a
    subclass of `DatabaseError`.
    
.. exception:: ProgrammingError
  
    Exception raised for programming errors, e.g. table not found or already
    exists, syntax error in the SQL statement, wrong number of parameters
    specified, etc.  It is a subclass of `DatabaseError`.
    
.. exception:: NotSupportedError
  
    Exception raised in case a method or database API was used which is not
    supported by the database, e.g. requesting a `!rollback()` on a
    connection that does not support transaction or has transactions turned
    off.  It is a subclass of `DatabaseError`.


.. extension::

    Psycopg may raise a few other, more specialized, exceptions: currently
    `~psycopg2.extensions.QueryCanceledError` and
    `~psycopg2.extensions.TransactionRollbackError` are defined. These
    exceptions are not exposed by the main `!psycopg2` module but are
    made available by the `~psycopg2.extensions` module.  All the
    additional exceptions are subclasses of standard |DBAPI| exceptions, so
    trapping them specifically is not required.


This is the exception inheritance layout:

.. parsed-literal::

    `!StandardError`
    \|__ `Warning`
    \|__ `Error`
        \|__ `InterfaceError`
        \|__ `DatabaseError`
            \|__ `DataError`
            \|__ `OperationalError`
            \|   \|__ `psycopg2.extensions.QueryCanceledError`
            \|   \|__ `psycopg2.extensions.TransactionRollbackError`
            \|__ `IntegrityError`
            \|__ `InternalError`
            \|__ `ProgrammingError`
            \|__ `NotSupportedError`



.. _type-objects-and-constructors:

Type Objects and Constructors
-----------------------------

.. note::

    This section is mostly copied verbatim from the |DBAPI|_
    specification.  While these objects are exposed in compliance to the
    DB API, Psycopg offers very accurate tools to convert data between Python
    and PostgreSQL formats.  See :ref:`adapting-new-types` and
    :ref:`type-casting-from-sql-to-python`

Many databases need to have the input in a particular format for
binding to an operation's input parameters.  For example, if an
input is destined for a DATE column, then it must be bound to the
database in a particular string format.  Similar problems exist
for "Row ID" columns or large binary items (e.g. blobs or RAW
columns).  This presents problems for Python since the parameters
to the .execute*() method are untyped.  When the database module
sees a Python string object, it doesn't know if it should be bound
as a simple CHAR column, as a raw BINARY item, or as a DATE.

To overcome this problem, a module must provide the constructors
defined below to create objects that can hold special values.
When passed to the cursor methods, the module can then detect the
proper type of the input parameter and bind it accordingly.

A Cursor Object's description attribute returns information about
each of the result columns of a query.  The type_code must compare
equal to one of Type Objects defined below. Type Objects may be
equal to more than one type code (e.g. DATETIME could be equal to
the type codes for date, time and timestamp columns; see the
Implementation Hints below for details).

The module exports the following constructors and singletons:

.. function:: Date(year,month,day)

    This function constructs an object holding a date value.

.. function:: Time(hour,minute,second)

    This function constructs an object holding a time value.

.. function:: Timestamp(year,month,day,hour,minute,second)

    This function constructs an object holding a time stamp value.

.. function:: DateFromTicks(ticks)

    This function constructs an object holding a date value from the given
    ticks value (number of seconds since the epoch; see the documentation of
    the standard Python time module for details).

.. function:: TimeFromTicks(ticks)

    This function constructs an object holding a time value from the given
    ticks value (number of seconds since the epoch; see the documentation of
    the standard Python time module for details).

.. function:: TimestampFromTicks(ticks)

    This function constructs an object holding a time stamp value from the
    given ticks value (number of seconds since the epoch; see the
    documentation of the standard Python time module for details).

.. function:: Binary(string)

    This function constructs an object capable of holding a binary (long)
    string value.

.. note::

    All the adapters returned by the module level factories (`!Binary`,
    `!Date`, `!Time`, `!Timestamp` and the `!*FromTicks` variants) expose the
    wrapped object (a regular Python object such as `!datetime`) in an
    `!adapted` attribute.

.. data:: STRING

    This type object is used to describe columns in a database that are
    string-based (e.g. CHAR).

.. data:: BINARY

    This type object is used to describe (long) binary columns in a database
    (e.g. LONG, RAW, BLOBs).

.. data:: NUMBER

    This type object is used to describe numeric columns in a database.

.. data:: DATETIME

    This type object is used to describe date/time columns in a database.

.. data:: ROWID

    This type object is used to describe the "Row ID" column in a database.


.. testcode::
    :hide:

    conn.rollback()
