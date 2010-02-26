The ``connection`` class
========================

.. sectionauthor:: Daniele Varrazzo <daniele.varrazzo@gmail.com>

.. testsetup::

    from pprint import pprint
    import psycopg2.extensions

    drop_test_table('foo')

.. class:: connection

    Handles the connection to a PostgreSQL database instance. It encapsulates
    a database session.

    Connections are created using the factory function
    `~psycopg2.connect()`.

    Connections are thread safe and can be shared among many thread. See
    :ref:`thread-safety` for details.

    .. method:: cursor([name] [, cursor_factory])
          
        Return a new `cursor` object using the connection.

        If `name` is specified, the returned cursor will be a *server
        side* (or *named*) cursor. Otherwise the cursor will be *client side*.
        See :ref:`server-side-cursors` for further details.

        The `cursor_factory` argument can be used to create non-standard
        cursors. The class returned should be a subclass of
        `psycopg2.extensions.cursor`. See :ref:`subclassing-cursor` for
        details.

        .. extension::

            The `name` and `cursor_factory` parameters are Psycopg
            extensions to the |DBAPI|.


    .. index::
        pair: Transaction; Commit

    .. method:: commit()
          
        Commit any pending transaction to the database. Psycopg can be set to
        perform automatic commits at each operation, see
        `~connection.set_isolation_level()`.
        

    .. index::
        pair: Transaction; Rollback

    .. method:: rollback()

        Roll back to the start of any pending transaction.  Closing a
        connection without committing the changes first will cause an implicit
        rollback to be performed.


    .. method:: close()
              
        Close the connection now (rather than whenever `__del__()` is
        called).  The connection will be unusable from this point forward; an
        `~psycopg2.InterfaceError` will be raised if any operation is
        attempted with the connection.  The same applies to all cursor objects
        trying to use the connection.  Note that closing a connection without
        committing the changes first will cause an implicit rollback to be
        performed (unless a different isolation level has been selected: see
        `~connection.set_isolation_level()`).


    .. index::
        single: Exceptions; In the connection class

    .. rubric:: Excetptions as connection class attributes

    The `!connection` also exposes as attributes the same exceptions
    available in the `psycopg2` module.  See :ref:`dbapi-exceptions`.


    .. extension::

        The above methods are the only ones defined by the |DBAPI| protocol.
        The Psycopg connection objects exports the following additional
        methods and attributes.


    .. attribute:: closed

        Read-only attribute reporting whether the database connection is open
        (0) or closed (1).


    .. method:: reset

        Reset the connection to the default.

        The method rolls back an eventual pending transaction and executes the
        PostgreSQL |RESET|_ and |SET SESSION AUTHORIZATION|__ to revert the
        session to the default values.

        .. |RESET| replace:: :sql:`RESET`
        .. _RESET: http://www.postgresql.org/docs/8.4/static/sql-reset.html

        .. |SET SESSION AUTHORIZATION| replace:: :sql:`SET SESSION AUTHORIZATION`
        .. __: http://www.postgresql.org/docs/8.4/static/sql-set-session-authorization.html

        .. versionadded:: 2.0.12


    .. attribute:: dsn

        Read-only string containing the connection string used by the
        connection.


    .. index::
        pair: Transaction; Autocommit
        pair: Transaction; Isolation level

    .. _autocommit:

    .. attribute:: isolation_level
    .. method:: set_isolation_level(level)

        Read or set the `transaction isolation level`_ for the current session.
        The level defines the different phenomena that can happen in the
        database between concurrent transactions.

        The value set or read is an integer: symbolic constants are defined in
        the module `psycopg2.extensions`: see
        :ref:`isolation-level-constants` for the available values.

        The default level is :sql:`READ COMMITTED`: at this level a
        transaction is automatically started the first time a database command
        is executed.  If you want an *autocommit* mode, switch to
        `~psycopg2.extensions.ISOLATION_LEVEL_AUTOCOMMIT` before
        executing any command::

            >>> conn.set_isolation_level(psycopg2.extensions.ISOLATION_LEVEL_AUTOCOMMIT)

        See also :ref:`transactions-control`.

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
        the session.

        .. doctest::
            :options: NORMALIZE_WHITESPACE

            >>> cur.execute("CREATE TABLE foo (id serial PRIMARY KEY);")
            >>> pprint(conn.notices)
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
        :samp:`({pid},{name})`, where :samp:`{pid}` is the PID of the backend
        that sent the notification and :samp:`{name}` is the signal name
        specified in the :sql:`NOTIFY` command.

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

        .. versionadded:: 2.0.8


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

        .. versionadded:: 2.0.12


    .. index::
        pair: Transaction; Status

    .. method:: get_transaction_status()

        Return the current session transaction status as an integer.  Symbolic
        constants for the values are defined in the module
        `psycopg2.extensions`: see :ref:`transaction-status-constants`
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

        .. versionadded:: 2.0.12


    .. index::
        pair: Server; Version

    .. attribute:: server_version

        A read-only integer representing the backend version.

        The number is formed by converting the major, minor, and revision
        numbers into two-decimal-digit numbers and appending them together.
        For example, version 8.1.5 will be returned as ``80105``.
        
        .. seealso:: libpq docs for `PQserverVersion()`__ for details.

            .. __: http://www.postgresql.org/docs/8.4/static/libpq-status.html#AEN33556

        .. versionadded:: 2.0.12


    .. index::
        pair: Connection; Status

    .. attribute:: status

        A read-only integer representing the status of the connection.
        Symbolic constants for the values are defined in the module 
        `psycopg2.extensions`: see :ref:`connection-status-constants`
        for the available values.


    .. method:: lobject([oid [, mode [, new_oid [, new_file [, lobject_factory]]]]])

        Return a new database large object. See :ref:`large-objects` for an
        overview.

        :param oid: The OID of the object to read or write. 0 to create
            a new large object and and have its OID assigned automatically.
        :param mode: Access mode to the object: can be ``r``, ``w``,
            ``rw`` or ``n`` (meaning don't open it).
        :param new_oid: Create a new object using the specified OID. The
            function raises `OperationalError` if the OID is already in
            use. Default is 0, meaning assign a new one automatically.
        :param new_file: The name of a file to be imported in the the database
            (using the |lo_import|_ function)
        :param lobject_factory: Subclass of
            `~psycopg2.extensions.lobject` to be instantiated.
        :rtype: `~psycopg2.extensions.lobject`

        .. |lo_import| replace:: `!lo_import()`
        .. _lo_import: http://www.postgresql.org/docs/8.4/static/lo-interfaces.html#AEN36307

        .. versionadded:: 2.0.8
