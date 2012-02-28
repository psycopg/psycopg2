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

    # Open a cursor to perform database operations
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


The main entry points of Psycopg are:

- The function `~psycopg2.connect()` creates a new database session and
  returns a new `connection` instance.

- The class `connection` encapsulates a database session. It allows to:

  - create new `cursor`\s using the `~connection.cursor()` method to
    execute database commands and queries,

  - terminate the session using the methods `~connection.commit()` or
    `~connection.rollback()`.

- The class `cursor` allows interaction with the database:

  - send commands to the database using methods such as `~cursor.execute()`
    and `~cursor.executemany()`,

  - retrieve data from the database :ref:`by iteration <cursor-iterable>` or
    using methods such as `~cursor.fetchone()`, `~cursor.fetchmany()`,
    `~cursor.fetchall()`.



.. index::
    pair: Query; Parameters

.. _query-parameters:

Passing parameters to SQL queries
---------------------------------

Psycopg casts Python variables to SQL literals by type.  Many standard Python types
are already `adapted to the correct SQL representation`__.

.. __: python-types-adaptation_

Example: the Python function call::

    >>> cur.execute(
    ...     """INSERT INTO some_table (an_int, a_date, a_string)
    ...         VALUES (%s, %s, %s);""",
    ...     (10, datetime.date(2005, 11, 18), "O'Reilly"))

is converted into the SQL command::

    INSERT INTO some_table (an_int, a_date, a_string)
     VALUES (10, '2005-11-18', 'O''Reilly');

Named arguments are supported too using :samp:`%({name})s` placeholders.
Using named arguments the values can be passed to the query in any order and
many placeholders can use the same values::

    >>> cur.execute(
    ...     """INSERT INTO some_table (an_int, a_date, another_date, a_string)
    ...         VALUES (%(int)s, %(date)s, %(date)s, %(str)s);""",
    ...     {'int': 10, 'str': "O'Reilly", 'date': datetime.date(2005, 11, 18)})

While the mechanism resembles regular Python strings manipulation, there are a
few subtle differences you should care about when passing parameters to a
query:

- The Python string operator ``%`` is not used: the `~cursor.execute()`
  method accepts a tuple or dictionary of values as second parameter.
  |sql-warn|__.

  .. |sql-warn| replace:: **Never** use ``%`` or ``+`` to merge values
      into queries

  .. __: sql-injection_

- The variables placeholder must *always be a* ``%s``, even if a different
  placeholder (such as a ``%d`` for integers or ``%f`` for floats) may look
  more appropriate::

    >>> cur.execute("INSERT INTO numbers VALUES (%d)", (42,)) # WRONG
    >>> cur.execute("INSERT INTO numbers VALUES (%s)", (42,)) # correct

- For positional variables binding, *the second argument must always be a
  sequence*, even if it contains a single variable.  And remember that Python
  requires a comma to create a single element tuple::

    >>> cur.execute("INSERT INTO foo VALUES (%s)", "bar")    # WRONG
    >>> cur.execute("INSERT INTO foo VALUES (%s)", ("bar"))  # WRONG
    >>> cur.execute("INSERT INTO foo VALUES (%s)", ("bar",)) # correct
    >>> cur.execute("INSERT INTO foo VALUES (%s)", ["bar"])  # correct

- Only variable values should be bound via this method: it shouldn't be used
  to set table or field names. For these elements, ordinary string formatting
  should be used before running `~cursor.execute()`.



.. index:: Security, SQL injection

.. _sql-injection:

The problem with the query parameters
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The SQL representation for many data types is often not the same of the Python
string representation.  The classic example is with single quotes in
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
easily craft a malformed string, either gaining access to unauthorized data or
performing destructive operations on the database. This form of attack is
called `SQL injection`_ and is known to be one of the most widespread forms of
attack to servers. Before continuing, please print `this page`__ as a memo and
hang it onto your desk.

.. _SQL injection: http://en.wikipedia.org/wiki/SQL_injection
.. __: http://xkcd.com/327/

Psycopg can `automatically convert Python objects to and from SQL
literals`__: using this feature your code will be more robust and
reliable. We must stress this point:

.. __: python-types-adaptation_

.. warning::

    Never, **never**, **NEVER** use Python string concatenation (``+``) or
    string parameters interpolation (``%``) to pass variables to a SQL query
    string.  Not even at gunpoint.

The correct way to pass variables in a SQL command is using the second
argument of the `~cursor.execute()` method::

    >>> SQL = "INSERT INTO authors (name) VALUES (%s);" # Note: no quotes
    >>> data = ("O'Reilly", )
    >>> cur.execute(SQL, data) # Note: no % operator



.. index::
    single: Adaptation
    pair: Objects; Adaptation
    single: Data types; Adaptation

.. _python-types-adaptation:

Adaptation of Python values to SQL types
----------------------------------------

Many standards Python types are adapted into SQL and returned as Python
objects when a query is executed.

If you need to convert other Python types to and from PostgreSQL data types,
see :ref:`adapting-new-types` and :ref:`type-casting-from-sql-to-python`.  You
can also find a few other specialized adapters in the `psycopg2.extras`
module.

In the following examples the method `~cursor.mogrify()` is used to show
the SQL string that would be sent to the database.

.. _adapt-consts:

.. index::
    pair: None; Adaptation
    single: NULL; Adaptation
    pair: Boolean; Adaptation

- Python `None` and boolean values `True` and `False` are converted into the
  proper SQL literals::

    >>> cur.mogrify("SELECT %s, %s, %s;", (None, True, False))
    >>> 'SELECT NULL, true, false;'

.. _adapt-numbers:

.. index::
    single: Adaptation; numbers
    single: Integer; Adaptation
    single: Float; Adaptation
    single: Decimal; Adaptation

- Numeric objects: `int`, `long`, `float`, `~decimal.Decimal` are converted in
  the PostgreSQL numerical representation::

    >>> cur.mogrify("SELECT %s, %s, %s, %s;", (10, 10L, 10.0, Decimal("10.00")))
    >>> 'SELECT 10, 10, 10.0, 10.00;'

.. _adapt-string:

.. index::
    pair: Strings; Adaptation
    single: Unicode; Adaptation

- String types: `str`, `unicode` are converted in SQL string syntax.
  `!unicode` objects (`!str` in Python 3) are encoded in the connection
  `~connection.encoding` to be sent to the backend: trying to send a character
  not supported by the encoding will result in an error. Received data can be
  converted either as `!str` or `!unicode`: see :ref:`unicode-handling`.

.. _adapt-binary:

.. index::
    single: Buffer; Adaptation
    single: bytea; Adaptation
    single: bytes; Adaptation
    single: bytearray; Adaptation
    single: memoryview; Adaptation
    single: Binary string

- Binary types: Python types representing binary objects are converted into
  PostgreSQL binary string syntax, suitable for :sql:`bytea` fields.   Such
  types are `buffer` (only available in Python 2), `memoryview` (available
  from Python 2.7), `bytearray` (available from Python 2.6) and `bytes`
  (only from Python 3: the name is available from Python 2.6 but it's only an
  alias for the type `!str`). Any object implementing the `Revised Buffer
  Protocol`__ should be usable as binary type where the protocol is supported
  (i.e. from Python 2.6). Received data is returned as `!buffer` (in Python 2)
  or `!memoryview` (in Python 3).

  .. __: http://www.python.org/dev/peps/pep-3118/

  .. versionchanged:: 2.4
     only strings were supported before.

  .. versionchanged:: 2.4.1
     can parse the 'hex' format from 9.0 servers without relying on the
     version of the client library.

  .. note::

    In Python 2, if you have binary data in a `!str` object, you can pass them
    to a :sql:`bytea` field using the `psycopg2.Binary` wrapper::

        mypic = open('picture.png', 'rb').read()
        curs.execute("insert into blobs (file) values (%s)",
            (psycopg2.Binary(mypic),))

  .. warning::

     Since version 9.0 PostgreSQL uses by default `a new "hex" format`__ to
     emit :sql:`bytea` fields. Starting from Psycopg 2.4.1 the format is
     correctly supported.  If you use a previous version you will need some
     extra care when receiving bytea from PostgreSQL: you must have at least
     libpq 9.0 installed on the client or alternatively you can set the
     `bytea_output`__ configuration parameter to ``escape``, either in the
     server configuration file or in the client session (using a query such as
     ``SET bytea_output TO escape;``) before receiving binary data.

     .. __: http://www.postgresql.org/docs/current/static/datatype-binary.html
     .. __: http://www.postgresql.org/docs/current/static/runtime-config-client.html#GUC-BYTEA-OUTPUT

.. _adapt-date:

.. index::
    single: Adaptation; Date/Time objects
    single: Date objects; Adaptation
    single: Time objects; Adaptation
    single: Interval objects; Adaptation
    single: mx.DateTime; Adaptation

- Date and time objects: builtin `~datetime.datetime`, `~datetime.date`,
  `~datetime.time`,  `~datetime.timedelta` are converted into PostgreSQL's
  :sql:`timestamp`, :sql:`date`, :sql:`time`, :sql:`interval` data types.
  Time zones are supported too.  The Egenix `mx.DateTime`_ objects are adapted
  the same way::

    >>> dt = datetime.datetime.now()
    >>> dt
    datetime.datetime(2010, 2, 8, 1, 40, 27, 425337)

    >>> cur.mogrify("SELECT %s, %s, %s;", (dt, dt.date(), dt.time()))
    "SELECT '2010-02-08T01:40:27.425337', '2010-02-08', '01:40:27.425337';"

    >>> cur.mogrify("SELECT %s;", (dt - datetime.datetime(2010,1,1),))
    "SELECT '38 days 6027.425337 seconds';"

.. _adapt-list:

.. index::
    single: Array; Adaptation
    double: Lists; Adaptation

- Python lists are converted into PostgreSQL :sql:`ARRAY`\ s::

    >>> cur.mogrify("SELECT %s;", ([10, 20, 30], ))
    'SELECT ARRAY[10, 20, 30];'

  .. note::

    Reading back from PostgreSQL, arrays are converted to list of Python
    objects as expected, but only if the types are known one. Arrays of
    unknown types are returned as represented by the database (e.g.
    ``{a,b,c}``). You can easily create a typecaster for :ref:`array of
    unknown types <cast-array-unknown>`.

.. _adapt-tuple:

.. index::
    double: Tuple; Adaptation
    single: IN operator

- Python tuples are converted in a syntax suitable for the SQL :sql:`IN`
  operator and to represent a composite type::

    >>> cur.mogrify("SELECT %s IN %s;", (10, (10, 20, 30)))
    'SELECT 10 IN (10, 20, 30);'

  .. note::

    SQL doesn't allow an empty list in the IN operator, so your code should
    guard against empty tuples.

  If you want PostgreSQL composite types to be converted into a Python
  tuple/namedtuple you can use the `~psycopg2.extras.register_composite()`
  function.

  .. versionadded:: 2.0.6
     the tuple :sql:`IN` adaptation.

  .. versionchanged:: 2.0.14
     the tuple :sql:`IN` adapter is always active.  In previous releases it
     was necessary to import the `~psycopg2.extensions` module to have it
     registered.

  .. versionchanged:: 2.3
     `~collections.namedtuple` instances are adapted like regular tuples and
     can thus be used to represent composite types.

.. _adapt-dict:

.. index::
    single: dict; Adaptation
    single: hstore; Adaptation

- Python dictionaries are converted into the |hstore|_ data type. By default
  the adapter is not enabled: see `~psycopg2.extras.register_hstore()` for
  further details.

  .. |hstore| replace:: :sql:`hstore`
  .. _hstore: http://www.postgresql.org/docs/current/static/hstore.html

  .. versionadded:: 2.3
     the :sql:`hstore` adaptation.


.. index::
    single: Unicode

.. _unicode-handling:

Unicode handling
^^^^^^^^^^^^^^^^

Psycopg can exchange Unicode data with a PostgreSQL database.  Python
`!unicode` objects are automatically *encoded* in the client encoding
defined on the database connection (the `PostgreSQL encoding`__, available in
`connection.encoding`, is translated into a `Python codec`__ using the
`~psycopg2.extensions.encodings` mapping)::

    >>> print u, type(u)
    àèìòù€ <type 'unicode'>

    >>> cur.execute("INSERT INTO test (num, data) VALUES (%s,%s);", (74, u))

.. __: http://www.postgresql.org/docs/current/static/multibyte.html
.. __: http://docs.python.org/library/codecs.html#standard-encodings

When reading data from the database, in Python 2 the strings returned are
usually 8 bit `!str` objects encoded in the database client encoding::

    >>> print conn.encoding
    UTF8

    >>> cur.execute("SELECT data FROM test WHERE num = 74")
    >>> x = cur.fetchone()[0]
    >>> print x, type(x), repr(x)
    àèìòù€ <type 'str'> '\xc3\xa0\xc3\xa8\xc3\xac\xc3\xb2\xc3\xb9\xe2\x82\xac'

    >>> conn.set_client_encoding('LATIN9')

    >>> cur.execute("SELECT data FROM test WHERE num = 74")
    >>> x = cur.fetchone()[0]
    >>> print type(x), repr(x)
    <type 'str'> '\xe0\xe8\xec\xf2\xf9\xa4'

In Python 3 instead the strings are automatically *decoded* in the connection
`~connection.encoding`, as the `!str` object can represent Unicode characters.
In Python 2 you must register a :ref:`typecaster
<type-casting-from-sql-to-python>` in order to receive `!unicode` objects::

    >>> psycopg2.extensions.register_type(psycopg2.extensions.UNICODE, cur)

    >>> cur.execute("SELECT data FROM test WHERE num = 74")
    >>> x = cur.fetchone()[0]
    >>> print x, type(x), repr(x)
    àèìòù€ <type 'unicode'> u'\xe0\xe8\xec\xf2\xf9\u20ac'

In the above example, the `~psycopg2.extensions.UNICODE` typecaster is
registered only on the cursor. It is also possible to register typecasters on
the connection or globally: see the function
`~psycopg2.extensions.register_type()` and
:ref:`type-casting-from-sql-to-python` for details.

.. note::

    In Python 2, if you want to uniformly receive all your database input in
    Unicode, you can register the related typecasters globally as soon as
    Psycopg is imported::

        import psycopg2
        import psycopg2.extensions
        psycopg2.extensions.register_type(psycopg2.extensions.UNICODE)
        psycopg2.extensions.register_type(psycopg2.extensions.UNICODEARRAY)

    and then forget about this story.


.. index::
    single: Time Zones

.. _tz-handling:

Time zones handling
^^^^^^^^^^^^^^^^^^^

The PostgreSQL type :sql:`timestamp with time zone` is converted into Python
`~datetime.datetime` objects with a `~datetime.datetime.tzinfo` attribute set
to a `~psycopg2.tz.FixedOffsetTimezone` instance.

    >>> cur.execute("SET TIME ZONE 'Europe/Rome';")  # UTC + 1 hour
    >>> cur.execute("SELECT '2010-01-01 10:30:45'::timestamptz;")
    >>> cur.fetchone()[0].tzinfo
    psycopg2.tz.FixedOffsetTimezone(offset=60, name=None)

Note that only time zones with an integer number of minutes are supported:
this is a limitation of the Python `datetime` module.  A few historical time
zones had seconds in the UTC offset: these time zones will have the offset
rounded to the nearest minute, with an error of up to 30 seconds.

    >>> cur.execute("SET TIME ZONE 'Asia/Calcutta';")  # offset was +5:53:20
    >>> cur.execute("SELECT '1930-01-01 10:30:45'::timestamptz;")
    >>> cur.fetchone()[0].tzinfo
    psycopg2.tz.FixedOffsetTimezone(offset=353, name=None)

.. versionchanged:: 2.2.2
    timezones with seconds are supported (with rounding). Previously such
    timezones raised an error.  In order to deal with them in previous
    versions use `psycopg2.extras.register_tstz_w_secs()`.


.. index:: Transaction, Begin, Commit, Rollback, Autocommit, Read only

.. _transactions-control:

Transactions control
--------------------

In Psycopg transactions are handled by the `connection` class. By
default, the first time a command is sent to the database (using one of the
`cursor`\ s created by the connection), a new transaction is created.
The following database commands will be executed in the context of the same
transaction -- not only the commands issued by the first cursor, but the ones
issued by all the cursors created by the same connection.  Should any command
fail, the transaction will be aborted and no further command will be executed
until a call to the `~connection.rollback()` method.

The connection is responsible to terminate its transaction, calling either the
`~connection.commit()` or `~connection.rollback()` method.  Committed
changes are immediately made persistent into the database.  Closing the
connection using the `~connection.close()` method or destroying the
connection object (using `!del` or letting it fall out of scope)
will result in an implicit `!rollback()` call.

It is possible to set the connection in *autocommit* mode: this way all the
commands executed will be immediately committed and no rollback is possible. A
few commands (e.g. :sql:`CREATE DATABASE`, :sql:`VACUUM`...) require to be run
outside any transaction: in order to be able to run these commands from
Psycopg, the session must be in autocommit mode: you can use the
`~connection.autocommit` property (`~connection.set_isolation_level()` in
older versions).

.. warning::

    By default even a simple :sql:`SELECT` will start a transaction: in
    long-running programs, if no further action is taken, the session will
    remain "idle in transaction", a condition non desiderable for several
    reasons (locks are held by the session, tables bloat...). For long lived
    scripts, either make sure to terminate a transaction as soon as possible or
    use an autocommit connection.

A few other transaction properties can be set session-wide by the
`!connection`: for instance it is possible to have read-only transactions or
change the isolation level. See the `~connection.set_session()` method for all
the details.


.. index::
    pair: Server side; Cursor
    pair: Named; Cursor
    pair: DECLARE; SQL command
    pair: FETCH; SQL command
    pair: MOVE; SQL command

.. _server-side-cursors:

Server side cursors
-------------------

When a database query is executed, the Psycopg `cursor` usually fetches
all the records returned by the backend, transferring them to the client
process. If the query returned an huge amount of data, a proportionally large
amount of memory will be allocated by the client.

If the dataset is too large to be practically handled on the client side, it is
possible to create a *server side* cursor. Using this kind of cursor it is
possible to transfer to the client only a controlled amount of data, so that a
large dataset can be examined without keeping it entirely in memory.

Server side cursor are created in PostgreSQL using the |DECLARE|_ command and
subsequently handled using :sql:`MOVE`, :sql:`FETCH` and :sql:`CLOSE` commands.

Psycopg wraps the database server side cursor in *named cursors*. A named
cursor is created using the `~connection.cursor()` method specifying the
*name* parameter. Such cursor will behave mostly like a regular cursor,
allowing the user to move in the dataset using the `~cursor.scroll()`
method and to read the data using `~cursor.fetchone()` and
`~cursor.fetchmany()` methods.

Named cursors are also :ref:`iterable <cursor-iterable>` like regular cursors.
Note however that before Psycopg 2.4 iteration was performed fetching one
record at time from the backend, resulting in a large overhead. The attribute
`~cursor.itersize` now controls how many records are fetched at time
during the iteration: the default value of 2000 allows to fetch about 100KB
per roundtrip assuming records of 10-20 columns of mixed number and strings;
you may decrease this value if you are dealing with huge records.

Named cursors are usually created :sql:`WITHOUT HOLD`, meaning they live only
as long as the current transaction. Trying to fetch from a named cursor after
a `~connection.commit()` or to create a named cursor when the `connection`
transaction isolation level is set to `AUTOCOMMIT` will result in an exception.
It is possible to create a :sql:`WITH HOLD` cursor by specifying a `!True`
value for the `withhold` parameter to `~connection.cursor()` or by setting the
`~cursor.withhold` attribute to `!True` before calling `~cursor.execute()` on
the cursor. It is extremely important to always `~cursor.close()` such cursors,
otherwise they will continue to hold server-side resources until the connection
will be eventually closed. Also note that while :sql:`WITH HOLD` cursors
lifetime extends well after `~connection.commit()`, calling
`~connection.rollback()` will automatically close the cursor.

.. note::

    It is also possible to use a named cursor to consume a cursor created
    in some other way than using the |DECLARE| executed by
    `~cursor.execute()`. For example, you may have a PL/pgSQL function
    returning a cursor::

        CREATE FUNCTION reffunc(refcursor) RETURNS refcursor AS $$
        BEGIN
            OPEN $1 FOR SELECT col FROM test;
            RETURN $1;
        END;
        $$ LANGUAGE plpgsql;

    You can read the cursor content by calling the function with a regular,
    non-named, Psycopg cursor:

    .. code-block:: python

        cur1 = conn.cursor()
        cur1.callproc('reffunc', ['curname'])

    and then use a named cursor in the same transaction to "steal the cursor":

    .. code-block:: python

        cur2 = conn.cursor('curname')
        for record in cur2:     # or cur2.fetchone, fetchmany...
            # do something with record
            pass


.. |DECLARE| replace:: :sql:`DECLARE`
.. _DECLARE: http://www.postgresql.org/docs/current/static/sql-declare.html



.. index:: Thread safety, Multithread, Multiprocess

.. _thread-safety:

Thread and process safety
-------------------------

The Psycopg module and the `connection` objects are *thread-safe*: many
threads can access the same database either using separate sessions and
creating a `!connection` per thread or using the same
connection and creating separate `cursor`\ s. In |DBAPI|_ parlance, Psycopg is
*level 2 thread safe*.

The difference between the above two approaches is that, using different
connections, the commands will be executed in different sessions and will be
served by different server processes. On the other hand, using many cursors on
the same connection, all the commands will be executed in the same session
(and in the same transaction if the connection is not in :ref:`autocommit
<transactions-control>` mode), but they will be serialized.

The above observations are only valid for regular threads: they don't apply to
forked processes nor to green threads. `libpq` connections `shouldn't be used by a
forked processes`__, so when using a module such as `multiprocessing` or a
forking web deploy method such as FastCGI make sure to create the connections
*after* the fork.

.. __: http://www.postgresql.org/docs/current/static/libpq-connect.html#LIBPQ-CONNECT

Connections shouldn't be shared either by different green threads: see
:ref:`green-support` for further details.



.. index::
    pair: COPY; SQL command

.. _copy:

Using COPY TO and COPY FROM
---------------------------

Psycopg `cursor` objects provide an interface to the efficient
PostgreSQL |COPY|__ command to move data from files to tables and back.
The methods exposed are:

`~cursor.copy_from()`
    Reads data *from* a file-like object appending them to a database table
    (:sql:`COPY table FROM file` syntax). The source file must have both
    `!read()` and `!readline()` method.

`~cursor.copy_to()`
    Writes the content of a table *to* a file-like object (:sql:`COPY table TO
    file` syntax). The target file must have a `write()` method.

`~cursor.copy_expert()`
    Allows to handle more specific cases and to use all the :sql:`COPY`
    features available in PostgreSQL.

Please refer to the documentation of the single methods for details and
examples.

.. |COPY| replace:: :sql:`COPY`
.. __: http://www.postgresql.org/docs/current/static/sql-copy.html



.. index::
    single: Large objects

.. _large-objects:

Access to PostgreSQL large objects
----------------------------------

PostgreSQL offers support for `large objects`__, which provide stream-style
access to user data that is stored in a special large-object structure. They
are useful with data values too large to be manipulated conveniently as a
whole.

.. __: http://www.postgresql.org/docs/current/static/largeobjects.html

Psycopg allows access to the large object using the
`~psycopg2.extensions.lobject` class. Objects are generated using the
`connection.lobject()` factory method. Data can be retrieved either as bytes
or as Unicode strings.

Psycopg large object support efficient import/export with file system files
using the |lo_import|_ and |lo_export|_ libpq functions.

.. |lo_import| replace:: `!lo_import()`
.. _lo_import: http://www.postgresql.org/docs/current/static/lo-interfaces.html#LO-IMPORT
.. |lo_export| replace:: `!lo_export()`
.. _lo_export: http://www.postgresql.org/docs/current/static/lo-interfaces.html#LO-EXPORT



.. index::
    pair: Two-phase commit; Transaction

.. _tpc:

Two-Phase Commit protocol support
---------------------------------

.. versionadded:: 2.3

Psycopg exposes the two-phase commit features available since PostgreSQL 8.1
implementing the *two-phase commit extensions* proposed by the |DBAPI|.

The |DBAPI| model of two-phase commit is inspired by the `XA specification`__,
according to which transaction IDs are formed from three components:

- a format ID (non-negative 32 bit integer)
- a global transaction ID (string not longer than 64 bytes)
- a branch qualifier (string not longer than 64 bytes)

For a particular global transaction, the first two components will be the same
for all the resources. Every resource will be assigned a different branch
qualifier.

According to the |DBAPI| specification, a transaction ID is created using the
`connection.xid()` method. Once you have a transaction id, a distributed
transaction can be started with `connection.tpc_begin()`, prepared using
`~connection.tpc_prepare()` and completed using `~connection.tpc_commit()` or
`~connection.tpc_rollback()`.  Transaction IDs can also be retrieved from the
database using `~connection.tpc_recover()` and completed using the above
`!tpc_commit()` and `!tpc_rollback()`.

PostgreSQL doesn't follow the XA standard though, and the ID for a PostgreSQL
prepared transaction can be any string up to 200 characters long.
Psycopg's `~psycopg2.extensions.Xid` objects can represent both XA-style
transactions IDs (such as the ones created by the `!xid()` method) and
PostgreSQL transaction IDs identified by an unparsed string.

The format in which the Xids are converted into strings passed to the
database is the same employed by the `PostgreSQL JDBC driver`__: this should
allow interoperation between tools written in Python and in Java. For example
a recovery tool written in Python would be able to recognize the components of
transactions produced by a Java program.

For further details see the documentation for the above methods.

.. __: http://www.opengroup.org/bookstore/catalog/c193.htm
.. __: http://jdbc.postgresql.org/

