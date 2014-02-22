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


.. _cursor-subclasses:

Connection and cursor subclasses
--------------------------------

A few objects that change the way the results are returned by the cursor or
modify the object behavior in some other way. Typically `!cursor` subclasses
are passed as *cursor_factory* argument to `~psycopg2.connect()` so that the
connection's `~connection.cursor()` method will generate objects of this
class.  Alternatively a `!cursor` subclass can be used one-off by passing it
as the *cursor_factory* argument to the `!cursor()` method.

If you want to use a `!connection` subclass you can pass it as the
*connection_factory* argument of the `!connect()` function.


.. index::
    pair: Cursor; Dictionary

.. _dict-cursor:


Dictionary-like cursor
^^^^^^^^^^^^^^^^^^^^^^

The dict cursors allow to access to the retrieved records using an interface
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

    .. note::

        Not very useful since Psycopg 2.5: you can use `psycopg2.connect`\
        ``(dsn, cursor_factory=DictCursor)`` instead of `!DictConnection`.

.. autoclass:: DictRow


Real dictionary cursor
^^^^^^^^^^^^^^^^^^^^^^

.. autoclass:: RealDictCursor

.. autoclass:: RealDictConnection

    .. note::

        Not very useful since Psycopg 2.5: you can use `psycopg2.connect`\
        ``(dsn, cursor_factory=RealDictCursor)`` instead of
        `!RealDictConnection`.

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

    .. note::

        Not very useful since Psycopg 2.5: you can use `psycopg2.connect`\
        ``(dsn, cursor_factory=NamedTupleCursor)`` instead of
        `!NamedTupleConnection`.


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


.. index::
    pair: JSON; Data types
    pair: JSON; Adaptation

.. _adapt-json:

JSON_ adaptation
^^^^^^^^^^^^^^^^

.. versionadded:: 2.5

Psycopg can adapt Python objects to and from the PostgreSQL |pgjson|_ type.
With PostgreSQL 9.2 adaptation is available out-of-the-box. To use JSON data
with previous database versions (either with the `9.1 json extension`__, but
even if you want to convert text fields to JSON) you can use
`register_json()`.

.. __: http://people.planetpostgresql.org/andrew/index.php?/archives/255-JSON-for-PG-9.2-...-and-now-for-9.1!.html

The Python library used to convert Python objects to JSON depends on the
language version: with Python 2.6 and following the :py:mod:`json` module from
the standard library is used; with previous versions the `simplejson`_ module
is used if available. Note that the last `!simplejson` version supporting
Python 2.4 is the 2.0.9.

.. _JSON: http://www.json.org/
.. |pgjson| replace:: :sql:`json`
.. _pgjson: http://www.postgresql.org/docs/current/static/datatype-json.html
.. _simplejson: http://pypi.python.org/pypi/simplejson/

In order to pass a Python object to the database as query argument you can use
the `Json` adapter::

    curs.execute("insert into mytable (jsondata) values (%s)",
        [Json({'a': 100})])

Reading from the database, |pgjson| values will be automatically converted to
Python objects.

.. note::

    If you are using the PostgreSQL :sql:`json` data type but you want to read
    it as string in Python instead of having it parsed, your can either cast
    the column to :sql:`text` in the query (it is an efficient operation, that
    doesn't involve a copy)::

        cur.execute("select jsondata::text from mytable")

    or you can register a no-op `!loads()` function with
    `register_default_json()`::

        psycopg2.extras.register_default_json(loads=lambda x: x)

.. note::

    You can use `~psycopg2.extensions.register_adapter()` to adapt any Python
    dictionary to JSON, either registering `Json` or any subclass or factory
    creating a compatible adapter::

        psycopg2.extensions.register_adapter(dict, psycopg2.extras.Json)

    This setting is global though, so it is not compatible with similar
    adapters such as the one registered by `register_hstore()`. Any other
    object supported by JSON can be registered the same way, but this will
    clobber the default adaptation rule, so be careful to unwanted side
    effects.

If you want to customize the adaptation from Python to PostgreSQL you can
either provide a custom `!dumps()` function to `Json`::

    curs.execute("insert into mytable (jsondata) values (%s)",
        [Json({'a': 100}, dumps=simplejson.dumps)])

or you can subclass it overriding the `~Json.dumps()` method::

    class MyJson(Json):
        def dumps(self, obj):
            return simplejson.dumps(obj)

    curs.execute("insert into mytable (jsondata) values (%s)",
        [MyJson({'a': 100})])

Customizing the conversion from PostgreSQL to Python can be done passing a
custom `!loads()` function to `register_json()` (or `register_default_json()`
for PostgreSQL 9.2).  For example, if you want to convert the float values
from :sql:`json` into :py:class:`~decimal.Decimal` you can use::

    loads = lambda x: json.loads(x, parse_float=Decimal)
    psycopg2.extras.register_json(conn, loads=loads)



.. autoclass:: Json

    .. automethod:: dumps

.. autofunction:: register_json

.. autofunction:: register_default_json



.. index::
    pair: hstore; Data types
    pair: dict; Adaptation

.. _adapt-hstore:

Hstore data type
^^^^^^^^^^^^^^^^

.. versionadded:: 2.3

The |hstore|_ data type is a key-value store embedded in PostgreSQL.  It has
been available for several server versions but with the release 9.0 it has
been greatly improved in capacity and usefulness with the addition of many
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



.. index::
    pair: Composite types; Data types
    pair: tuple; Adaptation
    pair: namedtuple; Adaptation

.. _adapt-composite:

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

Nested composite types are handled as expected, provided that the type of the
composite components are registered as well.

.. doctest::

    >>> cur.execute("CREATE TYPE card_back AS (face card, back text);")
    >>> psycopg2.extras.register_composite('card_back', cur)
    <psycopg2.extras.CompositeCaster object at 0x...>

    >>> cur.execute("select ((8, 'hearts'), 'blue')::card_back")
    >>> cur.fetchone()[0]
    card_back(face=card(value=8, suit='hearts'), back='blue')

Adaptation from Python tuples to composite types is automatic instead and
requires no adapter registration.


.. _custom-composite:

.. Note::

    If you want to convert PostgreSQL composite types into something different
    than a `!namedtuple` you can subclass the `CompositeCaster` overriding
    `~CompositeCaster.make()`. For example, if you want to convert your type
    into a Python dictionary you can use::

        >>> class DictComposite(psycopg2.extras.CompositeCaster):
        ...     def make(self, values):
        ...         return dict(zip(self.attnames, values))

        >>> psycopg2.extras.register_composite('card', cur,
        ...     factory=DictComposite)

        >>> cur.execute("select (8, 'hearts')::card")
        >>> cur.fetchone()[0]
        {'suit': 'hearts', 'value': 8}


.. autofunction:: register_composite

    .. versionchanged:: 2.4.3
        added support for array of composite types
    .. versionchanged:: 2.5
        added the *factory* parameter


.. autoclass:: CompositeCaster

    .. automethod:: make

        .. versionadded:: 2.5

    Object attributes:

    .. attribute:: name

        The name of the PostgreSQL type.

    .. attribute:: schema

        The schema where the type is defined.

        .. versionadded:: 2.5

    .. attribute:: oid

        The oid of the PostgreSQL type.

    .. attribute:: array_oid

        The oid of the PostgreSQL array type, if available.

    .. attribute:: type

        The type of the Python objects returned. If :py:func:`collections.namedtuple()`
        is available, it is a named tuple with attributes equal to the type
        components. Otherwise it is just the `!tuple` object.

    .. attribute:: attnames

        List of component names of the type to be casted.

    .. attribute:: atttypes

        List of component type oids of the type to be casted.


.. index::
    pair: range; Data types

.. _adapt-range:

Range data types
^^^^^^^^^^^^^^^^

.. versionadded:: 2.5

Psycopg offers a `Range` Python type and supports adaptation between them and
PostgreSQL |range|_ types. Builtin |range| types are supported out-of-the-box;
user-defined |range| types can be adapted using `register_range()`.

.. |range| replace:: :sql:`range`
.. _range: http://www.postgresql.org/docs/current/static/rangetypes.html

.. autoclass:: Range

    This Python type is only used to pass and retrieve range values to and
    from PostgreSQL and doesn't attempt to replicate the PostgreSQL range
    features: it doesn't perform normalization and doesn't implement all the
    operators__ supported by the database.

    .. __: http://www.postgresql.org/docs/current/static/functions-range.html#RANGE-OPERATORS-TABLE

    `!Range` objects are immutable, hashable, and support the ``in`` operator
    (checking if an element is within the range). They can be tested for
    equivalence. Empty ranges evaluate to `!False` in boolean context,
    nonempty evaluate to `!True`.

    .. versionchanged:: 2.5.3

        `!Range` objects can be sorted although, as on the server-side, this
        ordering is not particularly meangingful. It is only meant to be used
        by programs assuming objects using `!Range` as primary key can be
        sorted on them. In previous versions comparing `!Range`\s raises
        `!TypeError`.

    Although it is possible to instantiate `!Range` objects, the class doesn't
    have an adapter registered, so you cannot normally pass these instances as
    query arguments. To use range objects as query arguments you can either
    use one of the provided subclasses, such as `NumericRange` or create a
    custom subclass using `register_range()`.

    Object attributes:

    .. autoattribute:: isempty
    .. autoattribute:: lower
    .. autoattribute:: upper
    .. autoattribute:: lower_inc
    .. autoattribute:: upper_inc
    .. autoattribute:: lower_inf
    .. autoattribute:: upper_inf


The following `Range` subclasses map builtin PostgreSQL |range| types to
Python objects: they have an adapter registered so their instances can be
passed as query arguments. |range| values read from database queries are
automatically casted into instances of these classes.

.. autoclass:: NumericRange
.. autoclass:: DateRange
.. autoclass:: DateTimeRange
.. autoclass:: DateTimeTZRange

.. note::

    Python lacks a representation for :sql:`infinity` date so Psycopg converts
    the value to `date.max` and such. When written into the database these
    dates will assume their literal value (e.g. :sql:`9999-12-31` instead of
    :sql:`infinity`).  Check :ref:`infinite-dates-handling` for an example of
    an alternative adapter to map `date.max` to :sql:`infinity`. An
    alternative dates adapter will be used automatically by the `DateRange`
    adapter and so on.


Custom |range| types (created with |CREATE TYPE|_ :sql:`... AS RANGE`) can be
adapted to a custom `Range` subclass:

.. autofunction:: register_range

.. autoclass:: RangeCaster

    Object attributes:

    .. attribute:: range

        The `!Range` subclass adapted.

    .. attribute:: adapter

        The `~psycopg2.extensions.ISQLQuote` responsible to adapt `!range`.

    .. attribute:: typecaster

        The object responsible for casting.

    .. attribute:: array_typecaster

        The object responsible to cast arrays, if available, else `!None`.



.. index::
    pair: UUID; Data types

.. _adapt-uuid:

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

