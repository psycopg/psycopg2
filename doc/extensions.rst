=======================================
 psycopg 2 extensions to the DBAPI 2.0
=======================================

This document is a short summary of the extensions built in psycopg 2.0.x over
the standard `Python Database API Specification 2.0`__, usually called simply
DBAPI-2.0 or even PEP-249.  Before reading on this document please make sure
you already know how to program in Python using a DBAPI-2.0 compliant driver:
basic concepts like opening a connection, executing queries and commiting or
rolling back a transaction will not be explained but just used.

.. __: http://www.python.org/peps/pep-0249.html

Many objects and extension functions are defined in the `psycopg2.extensions`
module.


Connection and cursor factories
===============================

psycopg 2 exposes two new-style classes that can be sub-classed and expanded to
adapt them to the needs of the programmer: `cursor` and `connection`.  The
`connection` class is usually sub-classed only to provide an easy way to create
customized cursors but other uses are possible. `cursor` is much more
interesting, because it is the class where query building, execution and result
type-casting into Python variables happens.

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


Row factories
-------------

tzinfo factories
----------------


Setting transaction isolation levels
====================================

psycopg2 connection objects hold informations about the PostgreSQL `transaction
isolation level`_.  The current transaction level can be read from the
`.isolation_level` attribute.  The default isolation level is ``READ
COMMITTED``.  A different isolation level con be set through the
`.set_isolation_level()` method.  The level can be set to one of the following
constants, defined in `psycopg2.extensions`:

`ISOLATION_LEVEL_AUTOCOMMIT`
    No transaction is started when command are issued and no
    `.commit()`/`.rollback()` is required.  Some PostgreSQL command such as
    ``CREATE DATABASE`` can't run into a transaction: to run such command use
    `.set_isolation_level(ISOLATION_LEVEL_AUTOCOMMIT)`.
    
`ISOLATION_LEVEL_READ_COMMITTED`
    This is the default value.  A new transaction is started at the first
    `.execute()` command on a cursor and at each new `.execute()` after a
    `.commit()` or a `.rollback()`.  The transaction runs in the PostgreSQL
    ``READ COMMITTED`` isolation level.
    
`ISOLATION_LEVEL_SERIALIZABLE`
    Transactions are run at a ``SERIALIZABLE`` isolation level.


.. _transaction isolation level: 
   http://www.postgresql.org/docs/8.1/static/transaction-iso.html


Adaptation of Python values to SQL types
========================================

psycopg2 casts Python variables to SQL literals by type.  Standard Python types
are already adapted to the proper SQL literal.

Example: the Python function::

    curs.execute("""INSERT INTO atable (anint, adate, astring)
                     VALUES (%s, %s, %s)""",
                 (10, datetime.date(2005, 11, 18), "O'Reilly"))

is converted into the SQL command::

    INSERT INTO atable (anint, adate, astring)
     VALUES (10, '2005-11-18', 'O''Reilly');

Named arguments are supported too with ``%(name)s`` placeholders. Notice that:

  - The Python string operator ``%`` is not used: the `.execute()` function
    accepts the values tuple or dictionary as second parameter.

  - The variables placeholder must always be a ``%s``, even if a different
    placeholder (such as a ``%d`` for an integer) may look more appropriate.

  - For positional variables binding, the second argument must always be a
    tuple, even if it contains a single variable.

  - Only variable values should be bound via this method: it shouldn't be used
    to set table or field names. For these elements, ordinary string formatting
    should be used before running `.execute()`.


Adapting new types
------------------

Any Python class or type can be adapted to an SQL string.  Adaptation mechanism
is similar to the Object Adaptation proposed in the `PEP-246`_ and is exposed
by the `adapt()` function.

psycopg2 `.execute()` method adapts its ``vars`` arguments to the `ISQLQuote`
protocol.  Objects that conform to this protocol expose a ``getquoted()`` method
returning the SQL representation of the object as a string.

The easiest way to adapt an object to an SQL string is to register an adapter
function via the `register_adapter()` function.  The adapter function must take
the value to be adapted as argument and return a conform object.  A convenient
object is the `AsIs` wrapper, whose ``getquoted()`` result is simply the
``str()``\ ingification of the wrapped object.

Example: mapping of a ``Point`` class into the ``point`` PostgreSQL geometric
type::

    from psycopg2.extensions import adapt, register_adapter, AsIs
    
    class Point(object):
        def __init__(self, x=0.0, y=0.0):
            self.x = x
            self.y = y
    
    def adapt_point(point):
        return AsIs("'(%s,%s)'" % (adapt(point.x), adapt(point.y)))
        
    register_adapter(Point, adapt_point)
    
    curs.execute("INSERT INTO atable (apoint) VALUES (%s)", 
                 (Point(1.23, 4.56),))

The above function call results in the SQL command::

    INSERT INTO atable (apoint) VALUES ((1.23, 4.56));


.. _PEP-246: http://www.python.org/peps/pep-0246.html


Type casting of SQL types into Python values
============================================

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

The `new_type()` function binds the object oids (more than one can be
specified) to the adapter function.  `register_type()` completes the spell.
Conversion is automatically performed when a column whose type is a registered
``oid`` is read::

    curs.execute("SELECT '(10.2,20.3)'::point")
    point = curs.fetchone()[0]
    print type(point), point.x, point.y
    # Prints: "<class '__main__.Point'> 10.2 20.3"


Working with times and dates
============================


Receiving NOTIFYs
=================


Using COPY TO and COPY FROM
===========================

psycopg2 `cursor` object provides an interface to the efficient `PostgreSQL
COPY command`__ to move data from files to tables and back.

The `.copy_to(file, table)` method writes the content of the table
named ``table`` *to* the file-like object ``file``. ``file`` must have a
``write()`` method.

The `.copy_from(file, table)` reads data *from* the file-like object
``file`` appending them to the table named ``table``. ``file`` must have both
``read()`` and ``readline()`` method.

Both methods accept two optional arguments: ``sep`` (defaulting to a tab) is
the columns separator and ``null`` (defaulting to ``\N``) represents ``NULL``
values in the file.

.. __: http://www.postgresql.org/docs/8.1/static/sql-copy.html


PostgreSQL status message and executed query
============================================

`cursor` objects have two special fields related to the last executed query:

  - `.query` is the textual representation (str or unicode, depending on what
    was passed to `.execute()` as first argument) of the query *after* argument
    binding and mogrification has been applied. To put it another way, `.query`
    is the *exact* query that was sent to the PostgreSQL backend.
    
  - `.statusmessage` is the status message that the backend sent upon query
    execution. It usually contains the basic type of the query (SELECT,
    INSERT, UPDATE, ...) and some additional information like the number of
    rows updated and so on. Refer to the PostgreSQL manual for more
    information.
