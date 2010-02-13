:mod:`psycopg2.extras` -- Miscellaneous goodies for Psycopg 2
=============================================================

.. sectionauthor:: Daniele Varrazzo <daniele.varrazzo@gmail.com>

.. module:: psycopg2.extras

.. testsetup::

    import psycopg2.extras

    create_test_table()

This module is a generic place used to hold little helper functions and
classes until a better place in the distribution is found.


.. index::
    pair: Cursor; Dictionary

.. _dict-cursor:

Dictionary-like cursor
----------------------

The dict cursors allow to access to the retrieved records using an iterface
similar to the Python dictionaries instead of the tuples. You can use it
either passing :class:`DictConnection` as :obj:`!connection_factory` argument
to the :func:`~psycopg2.connect` function or passing :class:`DictCursor` as
the :class:`!cursor_factory` argument to the :meth:`~connection.cursor` method
of a regular :class:`connection`.

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

    # The records still support indexing as the original tuple
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
    pair: Cursor; Logging

Logging cursor
--------------

.. autoclass:: LoggingConnection
    :members: initialize,filter

.. autoclass:: LoggingCursor


.. autoclass:: MinTimeLoggingConnection
    :members: initialize,filter

.. autoclass:: MinTimeLoggingCursor



.. index::
    pair: UUID; Data types

UUID data type
--------------

.. versionadded:: 2.0.9
.. versionchanged:: 2.0.13 added UUID array support.

    >>> psycopg2.extras.register_uuid()
    <psycopg2._psycopg.type object at 0x...>
    >>>
    >>> # Python UUID can be used in SQL queries
    >>> import uuid
    >>> psycopg2.extensions.adapt(uuid.uuid4()).getquoted()
    "'...-...-...-...-...'::uuid"
    >>>
    >>> # PostgreSQL UUID are transformed into Python UUID objects.
    >>> cur.execute("SELECT 'a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11'::uuid")
    >>> cur.fetchone()[0]
    UUID('a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11')


.. autofunction:: register_uuid

.. autoclass:: UUID_adapter



.. index::
    pair: INET; Data types

INET data type
--------------

    >>> psycopg2.extras.register_inet()
    <psycopg2._psycopg.type object at 0x...>
    >>> cur.execute("SELECT '192.168.0.1'::inet")
    >>> cur.fetchone()[0].addr
    '192.168.0.1'

.. autofunction:: register_inet()

.. autoclass:: Inet

.. todo:: Adapter non registered: how to register it?



.. index::
    single: Time zones; Fractional

Fractional time zones
---------------------

.. autofunction:: register_tstz_w_secs

    .. versionadded:: 2.0.9

