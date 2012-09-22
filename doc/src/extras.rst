`psycopg2.extras` -- Miscellaneous goodies for Psycopg 2
=============================================================

.. sectionauthor:: Daniele Varrazzo <daniele.varrazzo@gmail.com>

.. module:: psycopg2.extras

.. testsetup::

    import psycopg2.extras
    from psycopg2.extras import Inet

    create_test_table()

This module is a generic place used to hold little helper functions and
classes until a better place in the distribution is found.


.. index::
    pair: Cursor; Dictionary

.. _dict-cursor:


Connection and cursor subclasses
--------------------------------

A few objects that change the way the results are returned by the cursor or
modify the object behavior in some other way. Typically `!connection`
subclasses are passed as *connection_factory* argument to
`~psycopg2.connect()` so that the connection will generate the matching
`!cursor` subclass. Alternatively a `!cursor` subclass can be used one-off by
passing it as the *cursor_factory* argument to the `~connection.cursor()`
method of a regular `!connection`.

Dictionary-like cursor
^^^^^^^^^^^^^^^^^^^^^^

The dict cursors allow to access to the retrieved records using an iterface
similar to the Python dictionaries instead of the tuples.

    >>> dict_cur = conn.cursor(cursor_factory=psycopg2.extras.DictCursor)
    >>> dict_cur.execute("INSERT INTO test (num, data) VALUES(%s, %s)",
    ...                  (100, "abc'def"))
    >>> dict_cur.execute("SELECT * FROM test")
    >>> rec = dict_cur.fetchone()
    >>> rec['id']
    1
    >>> rec['num']
    100
    >>> rec['data']
    "abc'def"

The records still support indexing as the original tuple:

    >>> rec[2]
    "abc'def"


.. autoclass:: DictCursor

.. autoclass:: DictConnection

.. autoclass:: DictRow


Real dictionary cursor
^^^^^^^^^^^^^^^^^^^^^^

.. autoclass:: RealDictCursor

.. autoclass:: RealDictConnection

.. autoclass:: RealDictRow



.. index::
    pair: Cursor; namedtuple

`namedtuple` cursor
^^^^^^^^^^^^^^^^^^^^

.. versionadded:: 2.3

These objects require :py:func:`collections.namedtuple` to be found, so it is
available out-of-the-box only from Python 2.6. Anyway, the namedtuple
implementation is compatible with previous Python versions, so all you
have to do is to `download it`__ and make it available where we
expect it to be... ::

    from somewhere import namedtuple
    import collections
    collections.namedtuple = namedtuple
    from psycopg.extras import NamedTupleConnection
    # ...

.. __: http://code.activestate.com/recipes/500261-named-tuples/

.. autoclass:: NamedTupleCursor

.. autoclass:: NamedTupleConnection


.. index::
    pair: Cursor; Logging

Logging cursor
^^^^^^^^^^^^^^

.. autoclass:: LoggingConnection
    :members: initialize,filter

.. autoclass:: LoggingCursor


.. autoclass:: MinTimeLoggingConnection
    :members: initialize,filter

.. autoclass:: MinTimeLoggingCursor



.. index::
    single: Data types; Additional

Additional data types
---------------------


.. _adapt-hstore:

.. index::
    pair: hstore; Data types
    pair: dict; Adaptation

Hstore data type
^^^^^^^^^^^^^^^^

.. versionadded:: 2.3

The |hstore|_ data type is a key-value store embedded in PostgreSQL.  It has
been available for several server versions but with the release 9.0 it has
been greatly improved in capacity and usefulness with the addiction of many
functions.  It supports GiST or GIN indexes allowing search by keys or
key/value pairs as well as regular BTree indexes for equality, uniqueness etc.

Psycopg can convert Python `!dict` objects to and from |hstore| structures.
Only dictionaries with string/unicode keys and values are supported.  `!None`
is also allowed as value but not as a key. Psycopg uses a more efficient |hstore|
representation when dealing with PostgreSQL 9.0 but previous server versions
are supported as well.  By default the adapter/typecaster are disabled: they
can be enabled using the `register_hstore()` function.

.. autofunction:: register_hstore

    .. versionchanged:: 2.4
        added the *oid* parameter. If not specified, the typecaster is
        installed also if |hstore| is not installed in the :sql:`public`
        schema.

    .. versionchanged:: 2.4.3
        added support for |hstore| array.


.. |hstore| replace:: :sql:`hstore`
.. _hstore: http://www.postgresql.org/docs/current/static/hstore.html



.. _adapt-composite:

.. index::
    pair: Composite types; Data types
    pair: tuple; Adaptation
    pair: namedtuple; Adaptation

Composite types casting
^^^^^^^^^^^^^^^^^^^^^^^

.. versionadded:: 2.4

Using `register_composite()` it is possible to cast a PostgreSQL composite
type (either created with the |CREATE TYPE|_ command or implicitly defined
after a table row type) into a Python named tuple, or into a regular tuple if
:py:func:`collections.namedtuple` is not found.

.. |CREATE TYPE| replace:: :sql:`CREATE TYPE`
.. _CREATE TYPE: http://www.postgresql.org/docs/current/static/sql-createtype.html

.. doctest::

    >>> cur.execute("CREATE TYPE card AS (value int, suit text);")
    >>> psycopg2.extras.register_composite('card', cur)
    <psycopg2.extras.CompositeCaster object at 0x...>

    >>> cur.execute("select (8, 'hearts')::card")
    >>> cur.fetchone()[0]
    card(value=8, suit='hearts')

Nested composite types are handled as expected, but the type of the composite
components must be registered as well.

.. doctest::

    >>> cur.execute("CREATE TYPE card_back AS (face card, back text);")
    >>> psycopg2.extras.register_composite('card_back', cur)
    <psycopg2.extras.CompositeCaster object at 0x...>

    >>> cur.execute("select ((8, 'hearts'), 'blue')::card_back")
    >>> cur.fetchone()[0]
    card_back(face=card(value=8, suit='hearts'), back='blue')

Adaptation from Python tuples to composite types is automatic instead and
requires no adapter registration.

.. autofunction:: register_composite

.. autoclass:: CompositeCaster



.. index::
    pair: UUID; Data types

UUID data type
^^^^^^^^^^^^^^

.. versionadded:: 2.0.9
.. versionchanged:: 2.0.13 added UUID array support.

.. doctest::

    >>> psycopg2.extras.register_uuid()
    <psycopg2._psycopg.type object at 0x...>

    >>> # Python UUID can be used in SQL queries
    >>> import uuid
    >>> my_uuid = uuid.UUID('{12345678-1234-5678-1234-567812345678}')
    >>> psycopg2.extensions.adapt(my_uuid).getquoted()
    "'12345678-1234-5678-1234-567812345678'::uuid"

    >>> # PostgreSQL UUID are transformed into Python UUID objects.
    >>> cur.execute("SELECT 'a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11'::uuid")
    >>> cur.fetchone()[0]
    UUID('a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11')


.. autofunction:: register_uuid

.. autoclass:: UUID_adapter



.. index::
    pair: INET; Data types

:sql:`inet` data type
^^^^^^^^^^^^^^^^^^^^^^

.. versionadded:: 2.0.9
.. versionchanged:: 2.4.5 added inet array support.

.. doctest::

    >>> psycopg2.extras.register_inet()
    <psycopg2._psycopg.type object at 0x...>

    >>> cur.mogrify("SELECT %s", (Inet('127.0.0.1/32'),))
    "SELECT E'127.0.0.1/32'::inet"

    >>> cur.execute("SELECT '192.168.0.1/24'::inet")
    >>> cur.fetchone()[0].addr
    '192.168.0.1/24'


.. autofunction:: register_inet

.. autoclass:: Inet



.. index::
    single: Time zones; Fractional

Fractional time zones
---------------------

.. autofunction:: register_tstz_w_secs

    .. versionadded:: 2.0.9

    .. versionchanged:: 2.2.2
        function is no-op: see :ref:`tz-handling`.

.. index::
   pair: Example; Coroutine;



Coroutine support
-----------------

.. autofunction:: wait_select(conn)

