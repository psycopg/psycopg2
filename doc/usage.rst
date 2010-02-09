Basic module usage
==================

.. sectionauthor:: Daniele Varrazzo <daniele.varrazzo@gmail.com>

.. index::
    pair: Example; Usage

The basic Psycopg usage is common to all the database adapters implementing
the |DBAPI|_ protocol. Here is an interactive session showing some of the
basic commands::

    >>> import psycopg2

    # Connect to an existing database
    >>> conn = psycopg2.connect("dbname=test user=postgres")

    # Open a curstor to perform database operations
    >>> cur = conn.cursor()

    # Execute a command: this creates a new table
    >>> cur.execute("CREATE TABLE test (id serial PRIMARY KEY, num integer, data varchar);")

    # Pass data to fill a query placeholders and let Psycopg perform
    # the correct conversion (no more SQL injections!)
    >>> cur.execute("INSERT INTO test (num, data) VALUES (%s, %s)", 
    ...      (100, "abc'def"))

    # Query the database and obtain data as Python objects
    >>> cur.execute("SELECT * FROM test;")
    >>> cur.fetchone()
    (1, 100, "abc'def")

    # Make the changes to the database persistent
    >>> conn.commit()

    # Close communication with the database
    >>> cur.close()
    >>> conn.close()


The main entry point of Psycopg are:

- The function :func:`psycopg2.connect()` creates a new database session and
  returns a new :class:`connection` instance.

- The class :class:`connection` encapsulates a database session. It allows to:

  - terminate the session using the methods :meth:`connection.commit()` and 
    :meth:`connection.rollback()`,

  - create new :class:`cursor`\ s to execute database commands and queries
    using the method :meth:`connection.cursor()`.

- The class :class:`cursor` allows interaction with the database:

  - send command using the methods :meth:`cursor.execute()` and
    :meth:`cursor.executemany()`,

  - retrieve data using the methods :meth:`cursor.fetchone()`,
    :meth:`cursor.fetchmany()`, :meth:`cursor.fetchall()`.
  


.. index:: Transaction, Begin, Commit, Rollback, Autocommit

Transactions control
--------------------

In Psycopg transactions are handled by the :class:`connection` class. By
default, every time a command is sent to the database (using one of the
:class:`cursor`\ s created by the connection), a new transaction is created.
The following database commands will be executed in the context of the same
transaction -- not only the commands issued by the first cursor, but the ones
issued by all the cursors created by the same connection.  Should any command
fail, the transaction will be aborted and no further command will be executed
until a call to the :meth:`connection.rollback` method.

The connection is responsible to terminate its transaction, calling either the
:meth:`commit` or :meth:`rollback` method.  Committed changes are immediately
made persistent into the database.  Closing the connection using the
:meth:`close` method or destroying the connection object (calling ``del`` or
letting it fall out of scope) will result in an implicit :meth:`rollback`
call.

It is possible to set the connection in *autocommit* mode: this way all the
commands executed will be immediately committed and no rollback is possible. A
few commands (e.g. ``CREATE DATABASE``) require to be run outside any
transaction: in order to be able to run these commands from Psycopg, the
session must be in autocommit mode.  Read the documentation for
:meth:`connection.set_isolation_level` to know how to change the commit mode.


.. index:: Security, SQL injection

.. _sql-injection:

The problem with the query parameters
-------------------------------------

The SQL representation for many data types is often not the same of the Python
string representation.  The classic example is with single quotes in the
strings: SQL uses them as string constants bounds and requires them to be
escaped, whereas in Python single quotes can be left unescaped in strings
bounded by double quotes. For this reason a naïve approach to the composition
of query strings, e.g. using string concatenation, is a recipe for terrible
problems::

    >>> SQL = "INSERT INTO authors (name) VALUES ('%s');" # NEVER DO THIS
    >>> data = ("O'Reilly", )
    >>> cur.execute(SQL % data) # THIS WILL FAIL MISERABLY
    ProgrammingError: syntax error at or near "Reilly"
    LINE 1: INSERT INTO authors (name) VALUES ('O'Reilly')
                                                  ^

If the variable containing the data to be sent to the database comes from an
untrusted source (e.g. a form published on a web site) an attacker could
easily craft a malformed string either gaining access to unauthorized data or
performing destructive operations on the database. This form of attack is
called `SQL injection`_ and is known to be one of the most widespread forms of
attack to servers. Before continuing, please print `this page`__ as a memo and
hang it onto your desktop.

.. _SQL injection: http://en.wikipedia.org/wiki/SQL_injection
.. __: http://xkcd.com/327/

Psycopg can `convert automatically Python objects into and from SQL
literals`__: using this feature your code will result more robust and
reliable. It is really the case to stress this point:

.. __: python-types-adaptation_

.. warning::

    Never, **never**, **NEVER** use Python string concatenation (``+``) or
    string parameters interpolation (``%``) to pass variables to a SQL query
    string.  Not even at gunpoint.

The correct way to pass variables in a SQL command is using the second
argument of the :meth:`cursor.execute()` method::

    >>> SQL = "INSERT INTO authors (name) VALUES (%s);" # Notice: no quotes
    >>> data = ("O'Reilly", )
    >>> cur.execute(SQL, data) # Notice: no % operator



.. index::
    pair: Query; Parameters

.. _query-parameters:

Passing parameters to SQL queries
---------------------------------

Psycopg casts Python variables to SQL literals by type.  `Standard Python types
are already adapted to the proper SQL literal`__. 

.. __: python-types-adaptation_

Example: the Python function call::

    >>> cur.execute(
    ...     """INSERT INTO some_table (an_int, a_date, a_string)
    ...         VALUES (%s, %s, %s);""",
    ...     (10, datetime.date(2005, 11, 18), "O'Reilly"))

is converted into the SQL command::

    INSERT INTO some_table (an_int, a_date, a_string)
     VALUES (10, '2005-11-18', 'O''Reilly');

Named arguments are supported too using ``%(name)s`` placeholders.  Using named
arguments the values can be passed to the query in any order and many
placeholder can use the same values::

    >>> cur.execute(
    ...     """INSERT INTO some_table (an_int, a_date, another_date, a_string)
    ...         VALUES (%(int)s, %(date)s, %(date)s, %(str)s);""",
    ...     {'int': 10, 'str': "O'Reilly", 'date': datetime.date(2005, 11, 18)})

Notice that:

- The Python string operator ``%`` is not used: the :meth:`cursor.execute()`
  method accepts a tuple or dictionary of values as second parameter.
  |sql-warn|__.
  
  .. |sql-warn| replace:: **Never** use ``%`` or ``+`` to merge values
      into queries

  .. __: sql-injection_

- The variables placeholder must always be a ``%s``, even if a different
  placeholder (such as a ``%d`` for an integer) may look more appropriate::

    >>> cur.execute("INSERT INTO numbers VALUES (%d)", (42,)) # WRONG
    >>> cur.execute("INSERT INTO numbers VALUES (%s)", (42,)) # correct

- For positional variables binding, the second argument must always be a
  tuple, even if it contains a single variable::

    >>> cur.execute("INSERT INTO foo VALUES (%s)", "bar")    # WRONG
    >>> cur.execute("INSERT INTO foo VALUES (%s)", ("bar",)) # correct

- Only variable values should be bound via this method: it shouldn't be used
  to set table or field names. For these elements, ordinary string formatting
  should be used before running :meth:`cursor.execute()`.



.. index::
    pair: Objects; Adaptation
    single: Data types; Adaptation

.. _python-types-adaptation:

Adaptation of Python values to SQL types
----------------------------------------

Many standards Python types are adapted into SQL and returned as Python
objects when a query is executed. 

If you need to convert other Python types to and from PostgreSQL data types,
see :ref:`adapting-new-types` and :ref:`type-casting-from-sql-to-python`.

In the following examples the method :meth:`cursor.mogrify()` is used to show
the SQL string that would be sent to the database.

.. index::
    single: None; Adaptation
    single: NULL; Adaptation
    single: Boolean; Adaptation

- Python ``None`` and boolean values are converted into the proper SQL
  literals::

    >>> cur.mogrify("SELECT %s, %s, %s;", (None, True, False))
    >>> 'SELECT NULL, true, false;'

.. index::
    single: Integer; Adaptation
    single: Float; Adaptation
    single: Decimal; Adaptation

- Numeric objects: ``int``, ``long``, ``float``, ``Decimal`` are converted in
  the PostgreSQL numerical representation::

    >>> cur.mogrify("SELECT %s, %s, %s, %s;", (10, 10L, 10.0, Decimal("10.00")))
    >>> 'SELECT 10, 10, 10.0, 10.00;'

.. index::
    single: Strings; Adaptation
    single: Unicode; Adaptation
    single: Buffer; Adaptation
    single: bytea; Adaptation
    single: Binary string

- String types: ``str``, ``unicode`` are converted in SQL string syntax.
  ``buffer`` is converted in PostgreSQL binary string syntax, suitable for
  ``bytea`` fields.

  .. todo:: unicode not working?

.. index::
    single: Date objects; Adaptation
    single: Time objects; Adaptation
    single: Interval objects; Adaptation
    single: mx.DateTime; Adaptation

- Date and time objects: ``datetime.datetime``, ``datetime.date``,
  ``datetime.time``.  ``datetime.timedelta`` are converted into PostgreSQL's
  ``timestamp``, ``date``, ``time``, ``interval`` data types.  Time zones are
  supported too.  The Egenix `mx.DateTime`_ objects are adapted the same way::

    >>> dt = datetime.datetime.now()
    >>> dt
    datetime.datetime(2010, 2, 8, 1, 40, 27, 425337)

    >>> cur.mogrify("SELECT %s, %s, %s;", (dt, dt.date(), dt.time()))
    "SELECT '2010-02-08T01:40:27.425337', '2010-02-08', '01:40:27.425337';"

    >>> cur.mogrify("SELECT %s;", (dt - datetime.datetime(2010,1,1),))
    "SELECT '38 days 6027.425337 seconds';"

.. index::
    single: Array; Adaptation
    single: Lists; Adaptation

- Python lists are converted into PostgreSQL arrays::

    >>> cur.mogrify("SELECT %s;", ([10, 20, 30], ))
    'SELECT ARRAY[10, 20, 30];'

.. index::
    single: Tuple; Adaptation
    single: IN operator

- Python tuples are converted in a syntax suitable for the SQL ``IN``
  operator::

    >>> cur.mogrify("SELECT %s IN %s;", (10, (10, 20, 30)))
    'SELECT 10 IN (10, 20, 30);'

  .. note:: 

    SQL doesn't allow an empty list in the IN operator, so your code should
    guard against empty tuples.



.. index::
    pair: Server side; Cursor
    pair: Named; Cursor
    pair: DECLARE; SQL command
    pair: FETCH; SQL command
    pair: MOVE; SQL command

.. _server-side-cursors:

Server side cursors
-------------------

When a database query is executed, the Psycopg :class:`cursor` usually fetches
all the returned records, transferring them to the client process. If the
query returned an huge amount of data, a proportionally large amount of memory
will be allocated by the client.

If the dataset is too large to be practically handled on the client side, it is
possible to create a *server side* cursor. Using this kind of cursor it is
possible to transfer to the client only a controlled amount of data, so that a
large dataset can be examined without keeping it entirely in memory.

Server side cursor are created in PostgreSQL using the |DECLARE|_ command and
subsequently handled using ``MOVE``, ``FETCH`` and ``CLOSE`` commands.

Psycopg wraps the database server side cursor in *named cursors*. A name
cursor is created using the :meth:`connection.cursor` method specifying the
:obj:`name` parameter. Such cursor will behave mostly like a regular cursor,
allowing the user to move in the dataset using the :meth:`cursor.scroll`
methog and to read the data using :meth:`cursor.fetchone` and
:meth:`cursor.fetchmany` methods.

.. |DECLARE| replace:: ``DECLARE``
.. _DECLARE: http://www.postgresql.org/docs/8.4/static/sql-declare.html



.. index:: Thread safety, Multithread

.. _thread-safety:

Thread safety
-------------

The Psycopg module is *thread-safe*: threads can access the same database
using separate session (by creating a :class:`connection` per thread) or using
the same session (accessing to the same connection and creating separate
:class:`cursor`\ s). In |DBAPI|_ parlance, Psycopg is *level 2 thread safe*.



.. index:: 
    pair: COPY; SQL command

.. _copy:

Using COPY TO and COPY FROM
---------------------------

Psycopg :class:`cursor` objects provide an interface to the efficient
PostgreSQL |COPY|__ command to move data from files to tables and back.
The methods exposed are:

:meth:`cursor.copy_to()`
    Writes the content of a table *to* a file-like object (``COPY table TO
    file`` syntax). The target file must have a ``write()`` method.

:meth:`cursor.copy_from()`
    Reads data *from* a file-like object appending them to a database table
    (``COPY table FROM file`` syntax). The source file must have both
    ``read()`` and ``readline()`` method.

:meth:`cursor.copy_expert()`
    Allows to handle more specific cases and to use all the |COPY| features
    available in PostgreSQL.

Please refer to the documentation of the single methods for details and
examples.

.. |COPY| replace:: ``COPY``
.. __: http://www.postgresql.org/docs/8.4/static/sql-copy.html


