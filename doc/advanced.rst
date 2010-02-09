More advanced topics
====================

.. sectionauthor:: Daniele Varrazzo <daniele.varrazzo@gmail.com>

.. index::
    double: Subclassing; Cursor
    double: Subclassing; Connection

.. _subclassing-connection:
.. _subclassing-cursor:

Connection and cursor factories
-------------------------------

Psycopg exposes two new-style classes that can be sub-classed and expanded to
adapt them to the needs of the programmer: :class:`psycopg2.extensions.cursor`
and :class:`psycopg2.extensions.connection`.  The :class:`connection` class is
usually sub-classed only to provide an easy way to create customized cursors
but other uses are possible. :class:`cursor` is much more interesting, because
it is the class where query building, execution and result type-casting into
Python variables happens.

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
    curs = conn.cursor(cursor_factory=LoggingCursor)
    curs.execute("INSERT INTO mytable VALUES (%s, %s, %s);",
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
by the :func:`psycopg2.extensions.adapt()` function.

The :meth:`cursor.execute()` method adapts its arguments to the
:class:`psycopg2.extensions.ISQLQuote` protocol.  Objects that conform to this
protocol expose a ``getquoted()`` method returning the SQL representation of
the object as a string.

The easiest way to adapt an object to an SQL string is to register an adapter
function via the :func:`psycopg2.extensions.register_adapter()` function.  The
adapter function must take the value to be adapted as argument and return a
conform object.  A convenient object is the :func:`psycopg2.extensions.AsIs`
wrapper, whose ``getquoted()`` result is simply the ``str()``\ ingification of
the wrapped object.

Example: mapping of a ``Point`` class into the ``point`` PostgreSQL geometric
type::

    from psycopg2.extensions import adapt, register_adapter, AsIs
    
    class Point(object):
        def __init__(self, x, y):
            self.x = x
            self.y = y
    
    def adapt_point(point):
        return AsIs("'(%s, %s)'" % (adapt(point.x), adapt(point.y)))
        
    register_adapter(Point, adapt_point)
    
    curs.execute("INSERT INTO atable (apoint) VALUES (%s)", 
                 (Point(1.23, 4.56),))

The above function call results in the SQL command::

    INSERT INTO atable (apoint) VALUES ((1.23, 4.56));



.. index:: Type casting

.. _type-casting-from-sql-to-python:

Type casting of SQL types into Python values
--------------------------------------------

PostgreSQL objects read from the database can be adapted to Python objects
through an user-defined adapting function.  An adapter function takes two
argments: the object string representation as returned by PostgreSQL and the
cursor currently being read, and should return a new Python object.  For
example, the following function parses a PostgreSQL ``point`` into the
previously defined ``Point`` class::

    def cast_point(value, curs):
        if value is not None:
        	# Convert from (f1, f2) syntax using a regular expression.
            m = re.match("\((.*),(.*)\)", value) 
            if m:
                return Point(float(m.group(1)), float(m.group(2)))
                
To create a mapping from the PostgreSQL type (either standard or user-defined),
its ``oid`` must be known. It can be retrieved either by the second column of
the cursor description::

    curs.execute("SELECT NULL::point")
    point_oid = curs.description[0][1]   # usually returns 600

or by querying the system catalogs for the type name and namespace (the
namespace for system objects is ``pg_catalog``)::

    curs.execute("""
        SELECT pg_type.oid
          FROM pg_type JOIN pg_namespace
                 ON typnamespace = pg_namespace.oid
         WHERE typname = %(typename)s
           AND nspname = %(namespace)s""",
                {'typename': 'point', 'namespace': 'pg_catalog'})
        
    point_oid = curs.fetchone()[0]

After you know the object ``oid``, you must can and register the new type::

    POINT = psycopg2.extensions.new_type((point_oid,), "POINT", cast_point)
    psycopg2.extensions.register_type(POINT)

The :func:`psycopg2.extensions.new_type()` function binds the object oids
(more than one can be specified) to the adapter function.
:func:`psycopg2.extensions.register_type()` completes the spell.  Conversion
is automatically performed when a column whose type is a registered ``oid`` is
read::

    >>> curs.execute("SELECT '(10.2,20.3)'::point")
    >>> point = curs.fetchone()[0]
    >>> print type(point), point.x, point.y
    <class '__main__.Point'> 10.2 20.3



.. index::
    double: Asynchronous; Query
    
.. _asynchronous-queries:

Asynchronous queries
--------------------

.. warning::

    Async quaeries are not enabled for 2.0

    .. todo::

        I think this is false now: async queries seem working right now...

Program code can initiate an asynchronous query by passing an ``async=1`` flag
to the :meth:`cursor.execute` or :meth:`cursor.callproc` methods. A very
simple example, from the connection to the query::

    conn = psycopg.connect(database='test')
    curs = conn.cursor()
    curs.execute("SELECT * from test WHERE fielda > %s", (1971,), async=1)
    
From then on any query on other cursors derived from the same connection is
doomed to fail (and raise an exception) until the original cursor (the one
executing the query) complete the asynchronous operation. This can happen in
a number of different ways:

1) one of the :obj:`.fetch*()` methods is called, effectively blocking untill
   data has been sent from the backend to the client, terminating the query.
   
2) :meth:`connection.cancel` is called. This method tries to abort the
   current query and will block until the query is aborted or fully executed.
   The return value is ``True`` if the query was successfully aborted or
   ``False`` if it was executed. Query result are discarded in both cases.
   
   .. todo::

        Can't see any ``connection.cancel`` method.
   
3) :meth:`cursor.execute` is called again on the same cursor
   (:obj:`.execute()` on a different cursor will simply raise an exception).
   This waits for the complete execution of the current query, discard any
   data and execute the new one.

Note that calling :obj:`.execute()` two times in a row will not abort the
former query and will temporarily go to synchronous mode until the first of
the two queries is executed.

Cursors now have some extra methods that make them usefull during
asynchronous queries:

:meth:`cursor.fileno`
    Returns the file descriptor associated with the current connection and
    make possible to use a cursor in a context where a file object would be
    expected (like in a :func:`select()` call).

:meth:`cursor.isready`
    Returns ``False`` if the backend is still processing the query or ``True``
    if data is ready to be fetched (by one of the :obj:`.fetch*()` methods).
      
A code snippet that shows how to use the cursor object in a :func:`select()`
call::

    import psycopg
    import select
        
    conn = psycopg.connect(database='test')
    curs = conn.cursor()
    curs.execute("SELECT * from test WHERE fielda > %s", (1971,), async=1)

    # wait for input with a maximum timeout of 5 seconds
    query_ended = False
    while not query_ended:
        rread, rwrite, rspec = select([curs, another_file], [], [], 5)

    if curs.isready():
       query_ended = True

    # manage input from other sources like other_file, etc.

    print "Query Results:"
    for row in curs:
        print row


