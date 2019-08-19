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


.. note::

    Queries that are executed with `cursor.executemany()` are not logged.


.. autoclass:: MinTimeLoggingConnection
    :members: initialize,filter

.. autoclass:: MinTimeLoggingCursor



.. _replication-objects:

Replication support objects
---------------------------

See :ref:`replication-support` for an introduction to the topic.


The following replication types are defined:

.. data:: REPLICATION_LOGICAL
.. data:: REPLICATION_PHYSICAL


.. index::
    pair: Connection; replication

.. autoclass:: LogicalReplicationConnection

    This connection factory class can be used to open a special type of
    connection that is used for logical replication.

    Example::

        from psycopg2.extras import LogicalReplicationConnection
        log_conn = psycopg2.connect(dsn, connection_factory=LogicalReplicationConnection)
        log_cur = log_conn.cursor()


.. autoclass:: PhysicalReplicationConnection

    This connection factory class can be used to open a special type of
    connection that is used for physical replication.

    Example::

        from psycopg2.extras import PhysicalReplicationConnection
        phys_conn = psycopg2.connect(dsn, connection_factory=PhysicalReplicationConnection)
        phys_cur = phys_conn.cursor()

    Both `LogicalReplicationConnection` and `PhysicalReplicationConnection` use
    `ReplicationCursor` for actual communication with the server.


.. index::
    pair: Message; replication

The individual messages in the replication stream are represented by
`ReplicationMessage` objects (both logical and physical type):

.. autoclass:: ReplicationMessage

    .. attribute:: payload

        The actual data received from the server.

        An instance of either `bytes()` or `unicode()`, depending on the value
        of `decode` option passed to `~ReplicationCursor.start_replication()`
        on the connection.  See `~ReplicationCursor.read_message()` for
        details.

    .. attribute:: data_size

        The raw size of the message payload (before possible unicode
        conversion).

    .. attribute:: data_start

        LSN position of the start of the message.

    .. attribute:: wal_end

        LSN position of the current end of WAL on the server.

    .. attribute:: send_time

        A `~datetime` object representing the server timestamp at the moment
        when the message was sent.

    .. attribute:: cursor

        A reference to the corresponding `ReplicationCursor` object.


.. index::
    pair: Cursor; replication

.. autoclass:: ReplicationCursor

    .. method:: create_replication_slot(slot_name, slot_type=None, output_plugin=None)

        Create streaming replication slot.

        :param slot_name: name of the replication slot to be created
        :param slot_type: type of replication: should be either
                          `REPLICATION_LOGICAL` or `REPLICATION_PHYSICAL`
        :param output_plugin: name of the logical decoding output plugin to be
                              used by the slot; required for logical
                              replication connections, disallowed for physical

        Example::

            log_cur.create_replication_slot("logical1", "test_decoding")
            phys_cur.create_replication_slot("physical1")

            # either logical or physical replication connection
            cur.create_replication_slot("slot1", slot_type=REPLICATION_LOGICAL)

        When creating a slot on a logical replication connection, a logical
        replication slot is created by default.  Logical replication requires
        name of the logical decoding output plugin to be specified.

        When creating a slot on a physical replication connection, a physical
        replication slot is created by default.  No output plugin parameter is
        required or allowed when creating a physical replication slot.

        In either case the type of slot being created can be specified
        explicitly using *slot_type* parameter.

        Replication slots are a feature of PostgreSQL server starting with
        version 9.4.

    .. method:: drop_replication_slot(slot_name)

        Drop streaming replication slot.

        :param slot_name: name of the replication slot to drop

        Example::

            # either logical or physical replication connection
            cur.drop_replication_slot("slot1")

        Replication slots are a feature of PostgreSQL server starting with
        version 9.4.

    .. method:: start_replication(slot_name=None, slot_type=None, start_lsn=0, timeline=0, options=None, decode=False, status_interval=10)

        Start replication on the connection.

        :param slot_name: name of the replication slot to use; required for
                          logical replication, physical replication can work
                          with or without a slot
        :param slot_type: type of replication: should be either
                          `REPLICATION_LOGICAL` or `REPLICATION_PHYSICAL`
        :param start_lsn: the optional LSN position to start replicating from,
                          can be an integer or a string of hexadecimal digits
                          in the form ``XXX/XXX``
        :param timeline: WAL history timeline to start streaming from (optional,
                         can only be used with physical replication)
        :param options: a dictionary of options to pass to logical replication
                        slot (not allowed with physical replication)
        :param decode: a flag indicating that unicode conversion should be
                       performed on messages received from the server
        :param status_interval: time between feedback packets sent to the server

        If a *slot_name* is specified, the slot must exist on the server and
        its type must match the replication type used.

        If not specified using *slot_type* parameter, the type of replication
        is defined by the type of replication connection.  Logical replication
        is only allowed on logical replication connection, but physical
        replication can be used with both types of connection.

        On the other hand, physical replication doesn't require a named
        replication slot to be used, only logical replication does.  In any
        case logical replication and replication slots are a feature of
        PostgreSQL server starting with version 9.4.  Physical replication can
        be used starting with 9.0.

        If *start_lsn* is specified, the requested stream will start from that
        LSN.  The default is `!None` which passes the LSN ``0/0`` causing
        replay to begin at the last point for which the server got flush
        confirmation from the client, or the oldest available point for a new
        slot.

        The server might produce an error if a WAL file for the given LSN has
        already been recycled or it may silently start streaming from a later
        position: the client can verify the actual position using information
        provided by the `ReplicationMessage` attributes.  The exact server
        behavior depends on the type of replication and use of slots.

        The *timeline* parameter can only be specified with physical
        replication and only starting with server version 9.3.

        A dictionary of *options* may be passed to the logical decoding plugin
        on a logical replication slot.  The set of supported options depends
        on the output plugin that was used to create the slot.  Must be
        `!None` for physical replication.

        If *decode* is set to `!True` the messages received from the server
        would be converted according to the connection `~connection.encoding`.
        *This parameter should not be set with physical replication or with
        logical replication plugins that produce binary output.*

        Replication stream should periodically send feedback to the database
        to prevent disconnect via timeout. Feedback is automatically sent when
        `read_message()` is called or during run of the `consume_stream()`.
        To specify the feedback interval use *status_interval* parameter.
        The value of this parameter must be set to at least 1 second, but
        it can have a fractional part.


        This function constructs a |START_REPLICATION|_ command and calls
        `start_replication_expert()` internally.

        After starting the replication, to actually consume the incoming
        server messages use `consume_stream()` or implement a loop around
        `read_message()` in case of :ref:`asynchronous connection
        <async-support>`.

        .. versionchanged:: 2.8.3
            added the *status_interval* parameter.

        .. |START_REPLICATION| replace:: :sql:`START_REPLICATION`
        .. _START_REPLICATION: https://www.postgresql.org/docs/current/static/protocol-replication.html

    .. method:: start_replication_expert(command, decode=False, status_interval=10)

        Start replication on the connection using provided
        |START_REPLICATION|_ command.

        :param command: The full replication command. It can be a string or a
            `~psycopg2.sql.Composable` instance for dynamic generation.
        :param decode: a flag indicating that unicode conversion should be
            performed on messages received from the server.
        :param status_interval: time between feedback packets sent to the server

        .. versionchanged:: 2.8.3
            added the *status_interval* parameter.


    .. method:: consume_stream(consume, keepalive_interval=None)

        :param consume: a callable object with signature :samp:`consume({msg})`
        :param keepalive_interval: interval (in seconds) to send keepalive
                                   messages to the server

        This method can only be used with synchronous connection.  For
        asynchronous connections see `read_message()`.

        Before using this method to consume the stream call
        `start_replication()` first.

        This method enters an endless loop reading messages from the server
        and passing them to ``consume()`` one at a time, then waiting for more
        messages from the server.  In order to make this method break out of
        the loop and return, ``consume()`` can throw a `StopReplication`
        exception.  Any unhandled exception will make it break out of the loop
        as well.

        The *msg* object passed to ``consume()`` is an instance of
        `ReplicationMessage` class.  See `read_message()` for details about
        message decoding.

        This method also sends feedback messages to the server every
        *keepalive_interval* (in seconds). The value of this parameter must
        be set to at least 1 second, but it can have a fractional part.
        If the *keepalive_interval* is not specified, the value of
        *status_interval* specified in the `start_replication()` or
        `start_replication_expert()` will be used.

        The client must confirm every processed message by calling
        `send_feedback()` method on the corresponding replication cursor. A
        reference to the cursor is provided in the `ReplicationMessage` as an
        attribute.

        The following example is a sketch implementation of ``consume()``
        callable for logical replication::

            class LogicalStreamConsumer(object):

                # ...

                def __call__(self, msg):
                    self.process_message(msg.payload)
                    msg.cursor.send_feedback(flush_lsn=msg.data_start)

            consumer = LogicalStreamConsumer()
            cur.consume_stream(consumer)

        .. warning::

            When using replication with slots, failure to constantly consume
            *and* report success to the server appropriately can eventually
            lead to "disk full" condition on the server, because the server
            retains all the WAL segments that might be needed to stream the
            changes via all of the currently open replication slots.

        .. versionchanged:: 2.8.3
            changed the default value of the *keepalive_interval* parameter to `!None`.

    .. method:: send_feedback(write_lsn=0, flush_lsn=0, apply_lsn=0, reply=False, force=False)

        :param write_lsn: a LSN position up to which the client has written the data locally
        :param flush_lsn: a LSN position up to which the client has processed the
                          data reliably (the server is allowed to discard all
                          and every data that predates this LSN)
        :param apply_lsn: a LSN position up to which the warm standby server
                          has applied the changes (physical replication
                          master-slave protocol only)
        :param reply: request the server to send back a keepalive message immediately
        :param force: force sending a feedback message regardless of status_interval timeout

        Use this method to report to the server that all messages up to a
        certain LSN position have been processed on the client and may be
        discarded on the server.

        If the *reply* or *force* parameters are not set, this method will
        just update internal structures without sending the feedback message
        to the server. The library sends feedback message automatically
        when *status_interval* timeout is reached.

        .. versionchanged:: 2.8.3
            added the *force* parameter.

    Low-level replication cursor methods for :ref:`asynchronous connection
    <async-support>` operation.

    With the synchronous connection a call to `consume_stream()` handles all
    the complexity of handling the incoming messages and sending keepalive
    replies, but at times it might be beneficial to use low-level interface
    for better control, in particular to `~select` on multiple sockets.  The
    following methods are provided for asynchronous operation:

    .. method:: read_message()

        Try to read the next message from the server without blocking and
        return an instance of `ReplicationMessage` or `!None`, in case there
        are no more data messages from the server at the moment.

        This method should be used in a loop with asynchronous connections
        (after calling `start_replication()` once).  For synchronous
        connections see `consume_stream()`.

        The returned message's `~ReplicationMessage.payload` is an instance of
        `!unicode` decoded according to connection `~connection.encoding`
        *iff* *decode* was set to `!True` in the initial call to
        `start_replication()` on this connection, otherwise it is an instance
        of `!bytes` with no decoding.

        It is expected that the calling code will call this method repeatedly
        in order to consume all of the messages that might have been buffered
        until `!None` is returned.  After receiving `!None` from this method
        the caller should use `~select.select()` or `~select.poll()` on the
        corresponding connection to block the process until there is more data
        from the server.

        Last, but not least, this method sends feedback messages when
        *status_interval* timeout is reached or when keepalive message with
        reply request arrived from the server.

    .. method:: fileno()

        Call the corresponding connection's `~connection.fileno()` method and
        return the result.

        This is a convenience method which allows replication cursor to be
        used directly in `~select.select()` or `~select.poll()` calls.

    .. attribute:: io_timestamp

        A `~datetime` object representing the timestamp at the moment of last
        communication with the server (a data or keepalive message in either
        direction).

    .. attribute:: feedback_timestamp

        A `~datetime` object representing the timestamp at the moment when
        the last feedback message sent to the server.

        .. versionadded:: 2.8.3

    .. attribute:: wal_end

       LSN position of the current end of WAL on the server at the
       moment of last data or keepalive message received from the
       server.

        .. versionadded:: 2.8

    An actual example of asynchronous operation might look like this::

      from select import select
      from datetime import datetime

      def consume(msg):
          # ...
          msg.cursor.send_feedback(flush_lsn=msg.data_start)

      status_interval = 10.0
      while True:
          msg = cur.read_message()
          if msg:
              consume(msg)
          else:
              now = datetime.now()
              timeout = status_interval - (now - cur.feedback_timestamp).total_seconds()
              try:
                  sel = select([cur], [], [], max(0, timeout))
              except InterruptedError:
                  pass  # recalculate timeout and continue

.. index::
    pair: Cursor; Replication

.. autoclass:: StopReplication


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
.. versionchanged:: 2.5.4
    added |jsonb| support. In previous versions |jsonb| values are returned
    as strings. See :ref:`the FAQ <faq-jsonb-adapt>` for a workaround.

Psycopg can adapt Python objects to and from the PostgreSQL |jsons|_
types.  With PostgreSQL 9.2 and following versions adaptation is
available out-of-the-box. To use JSON data with previous database versions
(either with the `9.1 json extension`__, but even if you want to convert text
fields to JSON) you can use the `register_json()` function.

.. __: http://people.planetpostgresql.org/andrew/index.php?/archives/255-JSON-for-PG-9.2-...-and-now-for-9.1!.html

The Python :py:mod:`json` module is used by default to convert Python objects
to JSON and to parse data from the database.

.. _JSON: https://www.json.org/
.. |json| replace:: :sql:`json`
.. |jsonb| replace:: :sql:`jsonb`
.. |jsons| replace:: |json| and |jsonb|
.. _jsons: https://www.postgresql.org/docs/current/static/datatype-json.html

In order to pass a Python object to the database as query argument you can use
the `Json` adapter::

    curs.execute("insert into mytable (jsondata) values (%s)",
        [Json({'a': 100})])

Reading from the database, |json| and |jsonb| values will be automatically
converted to Python objects.

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
custom `!loads()` function to `register_json()`.  For the builtin data types
(|json| from PostgreSQL 9.2, |jsonb| from PostgreSQL 9.4) use
`register_default_json()` and `register_default_jsonb()`.  For example, if you
want to convert the float values from :sql:`json` into
:py:class:`~decimal.Decimal` you can use::

    loads = lambda x: json.loads(x, parse_float=Decimal)
    psycopg2.extras.register_json(conn, loads=loads)

Or, if you want to use an alternative JSON module implementation, such as the
faster UltraJSON_, you can use::

    psycopg2.extras.register_default_json(loads=ujson.loads, globally=True)
    psycopg2.extras.register_default_jsonb(loads=ujson.loads, globally=True)

.. _UltraJSON: https://pypi.org/project/ujson/


.. autoclass:: Json

    .. automethod:: dumps

.. autofunction:: register_json

    .. versionchanged:: 2.5.4
        added the *name* parameter to enable :sql:`jsonb` support.

.. autofunction:: register_default_json

.. autofunction:: register_default_jsonb

    .. versionadded:: 2.5.4



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
.. _hstore: https://www.postgresql.org/docs/current/static/hstore.html



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
.. _CREATE TYPE: https://www.postgresql.org/docs/current/static/sql-createtype.html

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
.. _range: https://www.postgresql.org/docs/current/static/rangetypes.html

.. autoclass:: Range

    This Python type is only used to pass and retrieve range values to and
    from PostgreSQL and doesn't attempt to replicate the PostgreSQL range
    features: it doesn't perform normalization and doesn't implement all the
    operators__ supported by the database.

    .. __: https://www.postgresql.org/docs/current/static/functions-range.html#RANGE-OPERATORS-TABLE

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
    pair: CIDR; Data types
    pair: MACADDR; Data types

.. _adapt-network:

Networking data types
^^^^^^^^^^^^^^^^^^^^^

By default Psycopg casts the PostgreSQL networking data types (:sql:`inet`,
:sql:`cidr`, :sql:`macaddr`) into ordinary strings; array of such types are
converted into lists of strings.

.. versionchanged:: 2.7
    in previous version array of networking types were not treated as arrays.

.. autofunction:: register_ipaddress


.. autofunction:: register_inet

    .. deprecated:: 2.7
        this function will not receive further development and may disappear in
        future versions.

.. doctest::

    >>> psycopg2.extras.register_inet()
    <psycopg2._psycopg.type object at 0x...>

    >>> cur.mogrify("SELECT %s", (Inet('127.0.0.1/32'),))
    "SELECT E'127.0.0.1/32'::inet"

    >>> cur.execute("SELECT '192.168.0.1/24'::inet")
    >>> cur.fetchone()[0].addr
    '192.168.0.1/24'


.. autoclass:: Inet

    .. deprecated:: 2.7
        this object will not receive further development and may disappear in
        future versions.



.. _fast-exec:

Fast execution helpers
----------------------

The current implementation of `~cursor.executemany()` is (using an extremely
charitable understatement) not particularly performing. These functions can
be used to speed up the repeated execution of a statement against a set of
parameters.  By reducing the number of server roundtrips the performance can be
`orders of magnitude better`__ than using `!executemany()`.

.. __: https://github.com/psycopg/psycopg2/issues/491#issuecomment-276551038


.. autofunction:: execute_batch

    .. versionadded:: 2.7

.. note::

    `!execute_batch()` can be also used in conjunction with PostgreSQL
    prepared statements using |PREPARE|_, |EXECUTE|_, |DEALLOCATE|_.
    Instead of executing::

        execute_batch(cur,
            "big and complex SQL with %s %s params",
            params_list)

    it is possible to execute something like::

        cur.execute("PREPARE stmt AS big and complex SQL with $1 $2 params")
        execute_batch(cur, "EXECUTE stmt (%s, %s)", params_list)
        cur.execute("DEALLOCATE stmt")

    which may bring further performance benefits: if the operation to perform
    is complex, every single execution will be faster as the query plan is
    already cached; furthermore the amount of data to send on the server will
    be lesser (one |EXECUTE| per param set instead of the whole, likely
    longer, statement).

    .. |PREPARE| replace:: :sql:`PREPARE`
    .. _PREPARE: https://www.postgresql.org/docs/current/static/sql-prepare.html

    .. |EXECUTE| replace:: :sql:`EXECUTE`
    .. _EXECUTE: https://www.postgresql.org/docs/current/static/sql-execute.html

    .. |DEALLOCATE| replace:: :sql:`DEALLOCATE`
    .. _DEALLOCATE: https://www.postgresql.org/docs/current/static/sql-deallocate.html


.. autofunction:: execute_values

    .. versionadded:: 2.7
    .. versionchanged:: 2.8
        added the *fetch* parameter.


.. index::
   pair: Example; Coroutine;



Coroutine support
-----------------

.. autofunction:: wait_select(conn)

    .. versionchanged:: 2.6.2
        allow to cancel a query using :kbd:`Ctrl-C`, see
        :ref:`the FAQ <faq-interrupt-query>` for an example.
