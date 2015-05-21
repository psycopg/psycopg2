More advanced topics
====================

.. sectionauthor:: Daniele Varrazzo <daniele.varrazzo@gmail.com>

.. testsetup:: *

    import re
    import select

    cur.execute("CREATE TABLE atable (apoint point)")
    conn.commit()

    def wait(conn):
        while 1:
            state = conn.poll()
            if state == psycopg2.extensions.POLL_OK:
                break
            elif state == psycopg2.extensions.POLL_WRITE:
                select.select([], [conn.fileno()], [])
            elif state == psycopg2.extensions.POLL_READ:
                select.select([conn.fileno()], [], [])
            else:
                raise psycopg2.OperationalError("poll() returned %s" % state)

    aconn = psycopg2.connect(database='test', async=1)
    wait(aconn)
    acurs = aconn.cursor()


.. index::
    double: Subclassing; Cursor
    double: Subclassing; Connection

.. _subclassing-connection:
.. _subclassing-cursor:

Connection and cursor factories
-------------------------------

Psycopg exposes two new-style classes that can be sub-classed and expanded to
adapt them to the needs of the programmer: `psycopg2.extensions.cursor`
and `psycopg2.extensions.connection`.  The `connection` class is
usually sub-classed only to provide an easy way to create customized cursors
but other uses are possible. `cursor` is much more interesting, because
it is the class where query building, execution and result type-casting into
Python variables happens.

The `~psycopg2.extras` module contains several examples of :ref:`connection
and cursor sublcasses <cursor-subclasses>`.

.. note::

    If you only need a customized cursor class, since Psycopg 2.5 you can use
    the `~connection.cursor_factory` parameter of a regular connection instead
    of creating a new `!connection` subclass.


.. index::
    single: Example; Cursor subclass

An example of cursor subclass performing logging is::

    import psycopg2
    import psycopg2.extensions
    import logging

    class LoggingCursor(psycopg2.extensions.cursor):
        def execute(self, sql, args=None):
            logger = logging.getLogger('sql_debug')
            logger.info(self.mogrify(sql, args))

            try:
                psycopg2.extensions.cursor.execute(self, sql, args)
            except Exception, exc:
                logger.error("%s: %s" % (exc.__class__.__name__, exc))
                raise

    conn = psycopg2.connect(DSN)
    cur = conn.cursor(cursor_factory=LoggingCursor)
    cur.execute("INSERT INTO mytable VALUES (%s, %s, %s);",
                 (10, 20, 30))



.. index::
    single: Objects; Creating new adapters
    single: Adaptation; Creating new adapters
    single: Data types; Creating new adapters

.. _adapting-new-types:

Adapting new Python types to SQL syntax
---------------------------------------

Any Python class or type can be adapted to an SQL string.  Adaptation mechanism
is similar to the Object Adaptation proposed in the :pep:`246` and is exposed
by the `psycopg2.extensions.adapt()` function.

The `~cursor.execute()` method adapts its arguments to the
`~psycopg2.extensions.ISQLQuote` protocol.  Objects that conform to this
protocol expose a `!getquoted()` method returning the SQL representation
of the object as a string (the method must return `!bytes` in Python 3).
Optionally the conform object may expose a
`~psycopg2.extensions.ISQLQuote.prepare()` method.

There are two basic ways to have a Python object adapted to SQL:

- the object itself is conform, or knows how to make itself conform. Such
  object must expose a `__conform__()` method that will be called with the
  protocol object as argument. The object can check that the protocol is
  `!ISQLQuote`, in which case it can return `!self` (if the object also
  implements `!getquoted()`) or a suitable wrapper object. This option is
  viable if you are the author of the object and if the object is specifically
  designed for the database (i.e. having Psycopg as a dependency and polluting
  its interface with the required methods doesn't bother you). For a simple
  example you can take a look at the source code for the
  `psycopg2.extras.Inet` object.

- If implementing the `!ISQLQuote` interface directly in the object is not an
  option (maybe because the object to adapt comes from a third party library),
  you can use an *adaptation function*, taking the object to be adapted as
  argument and returning a conforming object.  The adapter must be
  registered via the `~psycopg2.extensions.register_adapter()` function.  A
  simple example wrapper is `!psycopg2.extras.UUID_adapter` used by the
  `~psycopg2.extras.register_uuid()` function.

A convenient object to write adapters is the `~psycopg2.extensions.AsIs`
wrapper, whose `!getquoted()` result is simply the `!str()`\ ing conversion of
the wrapped object.

.. index::
    single: Example; Types adaptation

Example: mapping of a `!Point` class into the |point|_ PostgreSQL
geometric type:

.. doctest::

    >>> from psycopg2.extensions import adapt, register_adapter, AsIs

    >>> class Point(object):
    ...    def __init__(self, x, y):
    ...        self.x = x
    ...        self.y = y

    >>> def adapt_point(point):
    ...     x = adapt(point.x).getquoted()
    ...     y = adapt(point.y).getquoted()
    ...     return AsIs("'(%s, %s)'" % (x, y))

    >>> register_adapter(Point, adapt_point)

    >>> cur.execute("INSERT INTO atable (apoint) VALUES (%s)",
    ...             (Point(1.23, 4.56),))


.. |point| replace:: :sql:`point`
.. _point: http://www.postgresql.org/docs/current/static/datatype-geometric.html#DATATYPE-GEOMETRIC

The above function call results in the SQL command::

    INSERT INTO atable (apoint) VALUES ('(1.23, 4.56)');



.. index:: Type casting

.. _type-casting-from-sql-to-python:

Type casting of SQL types into Python objects
---------------------------------------------

PostgreSQL objects read from the database can be adapted to Python objects
through an user-defined adapting function.  An adapter function takes two
arguments: the object string representation as returned by PostgreSQL and the
cursor currently being read, and should return a new Python object.  For
example, the following function parses the PostgreSQL :sql:`point`
representation into the previously defined `!Point` class:

    >>> def cast_point(value, cur):
    ...    if value is None:
    ...        return None
    ...
    ...    # Convert from (f1, f2) syntax using a regular expression.
    ...    m = re.match(r"\(([^)]+),([^)]+)\)", value)
    ...    if m:
    ...        return Point(float(m.group(1)), float(m.group(2)))
    ...    else:
    ...        raise InterfaceError("bad point representation: %r" % value)
                

In order to create a mapping from a PostgreSQL type (either standard or
user-defined), its OID must be known. It can be retrieved either by the second
column of the `cursor.description`:

    >>> cur.execute("SELECT NULL::point")
    >>> point_oid = cur.description[0][1]
    >>> point_oid
    600

or by querying the system catalog for the type name and namespace (the
namespace for system objects is :sql:`pg_catalog`):

    >>> cur.execute("""
    ...    SELECT pg_type.oid
    ...      FROM pg_type JOIN pg_namespace
    ...             ON typnamespace = pg_namespace.oid
    ...     WHERE typname = %(typename)s
    ...       AND nspname = %(namespace)s""",
    ...    {'typename': 'point', 'namespace': 'pg_catalog'})
    >>> point_oid = cur.fetchone()[0]
    >>> point_oid
    600

After you know the object OID, you can create and register the new type:

    >>> POINT = psycopg2.extensions.new_type((point_oid,), "POINT", cast_point)
    >>> psycopg2.extensions.register_type(POINT)

The `~psycopg2.extensions.new_type()` function binds the object OIDs
(more than one can be specified) to the adapter function.
`~psycopg2.extensions.register_type()` completes the spell.  Conversion
is automatically performed when a column whose type is a registered OID is
read:

    >>> cur.execute("SELECT '(10.2,20.3)'::point")
    >>> point = cur.fetchone()[0]
    >>> print type(point), point.x, point.y
    <class 'Point'> 10.2 20.3

A typecaster created by `!new_type()` can be also used with
`~psycopg2.extensions.new_array_type()` to create a typecaster converting a
PostgreSQL array into a Python list.


.. index::
    pair: Asynchronous; Notifications
    pair: LISTEN; SQL command
    pair: NOTIFY; SQL command

.. _async-notify:

Asynchronous notifications
--------------------------

Psycopg allows asynchronous interaction with other database sessions using the
facilities offered by PostgreSQL commands |LISTEN|_ and |NOTIFY|_. Please
refer to the PostgreSQL documentation for examples about how to use this form of
communication.

Notifications are instances of the `~psycopg2.extensions.Notify` object made
available upon reception in the `connection.notifies` list. Notifications can
be sent from Python code simply executing a :sql:`NOTIFY` command in an
`~cursor.execute()` call.

Because of the way sessions interact with notifications (see |NOTIFY|_
documentation), you should keep the connection in `~connection.autocommit`
mode if you wish to receive or send notifications in a timely manner.

.. |LISTEN| replace:: :sql:`LISTEN`
.. _LISTEN: http://www.postgresql.org/docs/current/static/sql-listen.html
.. |NOTIFY| replace:: :sql:`NOTIFY`
.. _NOTIFY: http://www.postgresql.org/docs/current/static/sql-notify.html

Notifications are received after every query execution. If the user is
interested in receiving notifications but not in performing any query, the
`~connection.poll()` method can be used to check for new messages without
wasting resources.

A simple application could poll the connection from time to time to check if
something new has arrived. A better strategy is to use some I/O completion
function such as :py:func:`~select.select` to sleep until awaken from the kernel when there is
some data to read on the connection, thereby using no CPU unless there is
something to read::

    import select
    import psycopg2
    import psycopg2.extensions

    conn = psycopg2.connect(DSN)
    conn.set_isolation_level(psycopg2.extensions.ISOLATION_LEVEL_AUTOCOMMIT)

    curs = conn.cursor()
    curs.execute("LISTEN test;")

    print "Waiting for notifications on channel 'test'"
    while 1:
        if select.select([conn],[],[],5) == ([],[],[]):
            print "Timeout"
        else:
            conn.poll()
            while conn.notifies:
                notify = conn.notifies.pop(0)
                print "Got NOTIFY:", notify.pid, notify.channel, notify.payload

Running the script and executing a command such as :sql:`NOTIFY test, 'hello'`
in a separate :program:`psql` shell, the output may look similar to::

    Waiting for notifications on channel 'test'
    Timeout
    Timeout
    Got NOTIFY: 6535 test hello
    Timeout
    ...

Note that the payload is only available from PostgreSQL 9.0: notifications
received from a previous version server will have the
`~psycopg2.extensions.Notify.payload` attribute set to the empty string.

.. versionchanged:: 2.3
    Added `~psycopg2.extensions.Notify` object and handling notification
    payload.



.. index::
    double: Asynchronous; Connection

.. _async-support:

Asynchronous support
--------------------

.. versionadded:: 2.2.0

Psycopg can issue asynchronous queries to a PostgreSQL database. An asynchronous
communication style is established passing the parameter *async*\=1 to the
`~psycopg2.connect()` function: the returned connection will work in
*asynchronous mode*.

In asynchronous mode, a Psycopg connection will rely on the caller to poll the
socket file descriptor, checking if it is ready to accept data or if a query
result has been transferred and is ready to be read on the client. The caller
can use the method `~connection.fileno()` to get the connection file
descriptor and `~connection.poll()` to make communication proceed according to
the current connection state.

The following is an example loop using methods `!fileno()` and `!poll()`
together with the Python :py:func:`~select.select` function in order to carry on
asynchronous operations with Psycopg::

    def wait(conn):
        while 1:
            state = conn.poll()
            if state == psycopg2.extensions.POLL_OK:
                break
            elif state == psycopg2.extensions.POLL_WRITE:
                select.select([], [conn.fileno()], [])
            elif state == psycopg2.extensions.POLL_READ:
                select.select([conn.fileno()], [], [])
            else:
                raise psycopg2.OperationalError("poll() returned %s" % state)

The above loop of course would block an entire application: in a real
asynchronous framework, `!select()` would be called on many file descriptors
waiting for any of them to be ready.  Nonetheless the function can be used to
connect to a PostgreSQL server only using nonblocking commands and the
connection obtained can be used to perform further nonblocking queries.  After
`!poll()` has returned `~psycopg2.extensions.POLL_OK`, and thus `!wait()` has
returned, the connection can be safely used:

    >>> aconn = psycopg2.connect(database='test', async=1)
    >>> wait(aconn)
    >>> acurs = aconn.cursor()

Note that there are a few other requirements to be met in order to have a
completely non-blocking connection attempt: see the libpq documentation for
|PQconnectStart|_.

.. |PQconnectStart| replace:: `!PQconnectStart()`
.. _PQconnectStart: http://www.postgresql.org/docs/current/static/libpq-connect.html#LIBPQ-PQCONNECTSTARTPARAMS

The same loop should be also used to perform nonblocking queries: after
sending a query via `~cursor.execute()` or `~cursor.callproc()`, call
`!poll()` on the connection available from `cursor.connection` until it
returns `!POLL_OK`, at which point the query has been completely sent to the
server and, if it produced data, the results have been transferred to the
client and available using the regular cursor methods:

    >>> acurs.execute("SELECT pg_sleep(5); SELECT 42;")
    >>> wait(acurs.connection)
    >>> acurs.fetchone()[0]
    42

When an asynchronous query is being executed, `connection.isexecuting()` returns
`!True`. Two cursors can't execute concurrent queries on the same asynchronous
connection.

There are several limitations in using asynchronous connections: the
connection is always in `~connection.autocommit` mode and it is not
possible to change it. So a
transaction is not implicitly started at the first query and is not possible
to use methods `~connection.commit()` and `~connection.rollback()`: you can
manually control transactions using `~cursor.execute()` to send database
commands such as :sql:`BEGIN`, :sql:`COMMIT` and :sql:`ROLLBACK`. Similarly
`~connection.set_session()` can't be used but it is still possible to invoke the
:sql:`SET` command with the proper :sql:`default_transaction_...` parameter.

With asynchronous connections it is also not possible to use
`~connection.set_client_encoding()`, `~cursor.executemany()`, :ref:`large
objects <large-objects>`, :ref:`named cursors <server-side-cursors>`.

:ref:`COPY commands <copy>` are not supported either in asynchronous mode, but
this will be probably implemented in a future release.




.. index::
    single: Greenlet
    single: Coroutine
    single: Eventlet
    single: gevent
    single: Wait callback

.. _green-support:

Support for coroutine libraries
-------------------------------

.. versionadded:: 2.2.0

Psycopg can be used together with coroutine_\-based libraries and participate
in cooperative multithreading.

Coroutine-based libraries (such as Eventlet_ or gevent_) can usually patch the
Python standard library in order to enable a coroutine switch in the presence of
blocking I/O: the process is usually referred as making the system *green*, in
reference to the `green threads`_.

Because Psycopg is a C extension module, it is not possible for coroutine
libraries to patch it: Psycopg instead enables cooperative multithreading by
allowing the registration of a *wait callback* using the
`psycopg2.extensions.set_wait_callback()` function. When a wait callback is
registered, Psycopg will use `libpq non-blocking calls`__ instead of the regular
blocking ones, and will delegate to the callback the responsibility to wait
for the socket to become readable or writable.

Working this way, the caller does not have the complete freedom to schedule the
socket check whenever they want as with an :ref:`asynchronous connection
<async-support>`, but has the advantage of maintaining a complete |DBAPI|
semantics: from the point of view of the end user, all Psycopg functions and
objects will work transparently in the coroutine environment (blocking the
calling green thread and giving other green threads the possibility to be
scheduled), allowing non modified code and third party libraries (such as
SQLAlchemy_) to be used in coroutine-based programs.

.. warning::
    Psycopg connections are not *green thread safe* and can't be used
    concurrently by different green threads. Trying to execute more than one
    command at time using one cursor per thread will result in an error (or a
    deadlock on versions before 2.4.2).

    Therefore, programmers are advised to either avoid sharing connections
    between coroutines or to use a library-friendly lock to synchronize shared
    connections, e.g. for pooling.

Coroutine libraries authors should provide a callback implementation (and
possibly a method to register it) to make Psycopg as green as they want. An
example callback (using `!select()` to block) is provided as
`psycopg2.extras.wait_select()`: it boils down to something similar to::

    def wait_select(conn):
        while 1:
            state = conn.poll()
            if state == extensions.POLL_OK:
                break
            elif state == extensions.POLL_READ:
                select.select([conn.fileno()], [], [])
            elif state == extensions.POLL_WRITE:
                select.select([], [conn.fileno()], [])
            else:
                raise OperationalError("bad state from poll: %s" % state)

Providing callback functions for the single coroutine libraries is out of
psycopg2 scope, as the callback can be tied to the libraries' implementation
details. You can check the `psycogreen`_ project for further informations and
resources about the topic.

.. _coroutine: http://en.wikipedia.org/wiki/Coroutine
.. _greenlet: http://pypi.python.org/pypi/greenlet
.. _green threads: http://en.wikipedia.org/wiki/Green_threads
.. _Eventlet: http://eventlet.net/
.. _gevent: http://www.gevent.org/
.. _SQLAlchemy: http://www.sqlalchemy.org/
.. _psycogreen: http://bitbucket.org/dvarrazzo/psycogreen/
.. __: http://www.postgresql.org/docs/current/static/libpq-async.html

.. warning::

    :ref:`COPY commands <copy>` are currently not supported when a wait callback
    is registered, but they will be probably implemented in a future release.

    :ref:`Large objects <large-objects>` are not supported either: they are
    not compatible with asynchronous connections.


.. testcode::
    :hide:

    aconn.close()
    conn.rollback()
    cur.execute("DROP TABLE atable")
    conn.commit()
    cur.close()
    conn.close()
