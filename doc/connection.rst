The ``connection`` class
========================

.. sectionauthor:: Daniele Varrazzo <daniele.varrazzo@gmail.com>

.. class:: connection

    Handles the connection to a PostgreSQL database instance. It encapsulates
    a database session.

    Connections are created using the factory function
    :func:`psycopg2.connect()`.

    Connections are thread safe and can be shared among many thread. See
    :ref:`thread-safety` for details.

    .. method:: cursor([name] [, cursor_factory])
          
        Return a new :class:`cursor` object using the connection.

        If :obj:`name` is specified, the returned cursor will be a *server
        side* (or *named*) cursor. Otherwise the cursor will be *client side*.
        See :ref:`server-side-cursors` for further details.

        The ``cursor_factory`` argument can be used to create non-standard
        cursors. The class returned should be a subclass of
        :class:`extensions.cursor`. See :ref:`subclassing-cursor` for details.


    .. index::
        pair: Transaction; Commit

    .. method:: commit()
          
        Commit any pending transaction to the database. Psycopg can be set to
        perform automatic commits at each operation, see
        :meth:`connection.set_isolation_level()`.
        

    .. index::
        pair: Transaction; Rollback

    .. method:: rollback()

        Roll back to the start of any pending transaction.  Closing a
        connection without committing the changes first will cause an implicit
        rollback to be performed.


    .. method:: close()
              
        Close the connection now (rather than whenever ``__del__`` is called).
        The connection will be unusable from this point forward; a
        :exc:`psycopg2.Error` (or subclass) exception will be raised if any
        operation is attempted with the connection.  The same applies to all
        cursor objects trying to use the connection.  Note that closing a
        connection without committing the changes first will cause an implicit
        rollback to be performed (unless a different isolation level has been
        selected: see :meth:`connection.set_isolation_level()`).


    The above methods are the only ones defined by the |DBAPI|_ protocol.
    The Psycopg connection objects exports the following additional methods
    and attributes.

    .. attribute:: closed

        Read-only attribute reporting whether the database connection is open
        (0) or closed (1).

    .. attribute:: dsn

        Read-only string containing the connection string used by the
        connection.


    .. index::
        pair: Transaction; Autocommit
        pair: Transaction; Isolation level

    .. attribute:: isolation_level
    .. method:: set_isolation_level(level)

        Read or set the `transaction isolation level`_ for the current session.
        The level defines the different phenomena that can happen in the
        database between concurrent transactions.

        The value set or read is an integer: symbolic constants are defined in
        the module :mod:`psycopg2.extensions`: see
        :ref:`isolation-level-constants` for the available values.

        The default level is ``READ COMMITTED``: in this level a transaction
        is automatically started every time a database command is executed. If
        you want an *autocommit* mode, set the connection in ``AUTOCOMMIT``
        mode before executing any command::

            >>> conn.set_isolation_level(psycopg2.extensions.ISOLATION_LEVEL_AUTOCOMMIT)


    .. index::
        pair: Client; Encoding

    .. attribute:: encoding
    .. method:: set_client_encoding(enc)

        Read or set the client encoding for the current session. The default
        is the encoding defined by the database. It should be one of the
        `characters set supported by PostgreSQL`__

        .. __: http://www.postgresql.org/docs/8.4/static/multibyte.html


    .. index::
        pair: Client; Logging

    .. attribute:: notices

        A list containing all the database messages sent to the client during
        the session.::

            >>> cur.execute("CREATE TABLE foo (id serial PRIMARY KEY);")
            >>> conn.notices 
            ['NOTICE:  CREATE TABLE / PRIMARY KEY will create implicit index "foo_pkey" for table "foo"\n',
             'NOTICE:  CREATE TABLE will create implicit sequence "foo_id_seq" for serial column "foo.id"\n']

        To avoid a leak in case excessive notices are generated, only the last
        50 messages are kept.

        You can configure what messages to receive using `PostgreSQL logging
        configuration parameters`__ such as ``log_statement``,
        ``client_min_messages``, ``log_min_duration_statement`` etc.
        
        .. __: http://www.postgresql.org/docs/8.4/static/runtime-config-logging.html


    .. attribute:: notifies

        List containing asynchronous notifications received by the session.

        Received notifications have the form of a 2 items tuple
        ``(pid,name)``, where ``pid`` is the PID of the backend that sent the
        notification and ``name`` is the signal name specified in the
        ``NOTIFY`` command.

        For other details see :ref:`async-notify`.

    .. index::
        pair: Backend; PID

    .. method:: get_backend_pid()

        Returns the process ID (PID) of the backend server process handling
        this connection.

        Note that the PID belongs to a process executing on the database
        server host, not the local host!

        .. seealso:: libpq docs for `PQbackendPID()`__ for details.

            .. __: http://www.postgresql.org/docs/8.4/static/libpq-status.html#AEN33590


    .. index::
        pair: Server; Parameters

    .. method:: get_parameter_status(parameter)
    
        Look up a current parameter setting of the server.

        Potential values for ``parameter`` are: ``server_version``,
        ``server_encoding``, ``client_encoding``, ``is_superuser``,
        ``session_authorization``, ``DateStyle``, ``TimeZone``,
        ``integer_datetimes``, and ``standard_conforming_strings``.

        If server did not report requested parameter, return ``None``.

        .. seealso:: libpq docs for `PQparameterStatus()`__ for details.

            .. __: http://www.postgresql.org/docs/8.4/static/libpq-status.html#AEN33499


    .. index::
        pair: Transaction; Status

    .. method:: get_transaction_status()

        Return the current session transaction status as an integer.  Symbolic
        constants for the values are defined in the module
        :mod:`psycopg2.extensions`: see :ref:`transaction-status-constants`
        for the available values.

        .. seealso:: libpq docs for `PQtransactionStatus()`__ for details.

            .. __: http://www.postgresql.org/docs/8.4/static/libpq-status.html#AEN33480


    .. index::
        pair: Protocol; Version

    .. attribute:: protocol_version

        A read-only integer representing frontend/backend protocol being used.
        It can be 2 or 3.

        .. seealso:: libpq docs for `PQprotocolVersion()`__ for details.

            .. __: http://www.postgresql.org/docs/8.4/static/libpq-status.html#AEN33546


    .. index::
        pair: Server; Version

    .. attribute:: server_version

        A read-only integer representing the backend version.

        The number is formed by converting the major, minor, and revision
        numbers into two-decimal-digit numbers and appending them together.
        For example, version 8.1.5 will be returned as 80105,
        
        .. seealso:: libpq docs for `PQserverVersion()`__ for details.

            .. __: http://www.postgresql.org/docs/8.4/static/libpq-status.html#AEN33556


    .. index::
        pair: Connection; Status

    .. attribute:: status

        A read-only integer representing the status of the connection.
        Symbolic constants for the values are defined in the module 
        :mod:`psycopg2.extensions`: see :ref:`connection-status-constants`
        for the available values.


    .. method:: lobject([oid [, mode [, new_oid [, new_file [, lobject_factory]]]]])

        Return a new database large object.

        The ``lobject_factory`` argument can be used to create non-standard
        lobjects by passing a class different from the default. Note that the
        new class *should* be a sub-class of
        :class:`psycopg2.extensions.lobject`.

        .. todo:: conn.lobject details

    .. attribute:: binary_types

        .. todo:: describe binary_types

    .. attribute:: string_types

        .. todo:: describe string_types


    .. index::
        single: Exceptions; In the connection class

    The :class:`connection` also exposes the same `Error` classes available in
    the :mod:`psycopg2` module as attributes.

