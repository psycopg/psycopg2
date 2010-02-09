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

    .. method:: commit()
          
        Commit any pending transaction to the database. Psycopg can be set to
        perform automatic commits at each operation, see
        :meth:`connection.set_isolation_level()`.
        
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

    The above methods are the only ones defined by the |DBAPI 2.0|_ protocol.
    The Psycopg connection objects exports the following additional methods
    and attributes.

    .. attribute:: closed

        Read-only attribute reporting whether the database connection is open
        (0) or closed (1).

    .. attribute:: dsn

        Read-only string containing the connection string used by the
        connection.

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

    .. attribute:: encoding
    .. method:: set_client_encoding(enc)

        Read or set the client encoding for the current session. The default
        is the encoding defined by the database. It should be one of the
        `characters set supported by PostgreSQL`__

        .. __: http://www.postgresql.org/docs/8.4/static/multibyte.html


    .. method:: get_backend_pid()

        Returns the process ID (PID) of the backend server process handling
        this connection.

        Note that the PID belongs to a process executing on the database
        server host, not the local host!

        .. seealso:: libpq docs for `PQbackendPID()`__ for details.

            .. __: http://www.postgresql.org/docs/8.4/static/libpq-status.html#AEN33590

    .. method:: get_parameter_status(parameter)
    
        Look up a current parameter setting of the server.

        Potential values for ``parameter`` are: ``server_version``,
        ``server_encoding``, ``client_encoding``, ``is_superuser``,
        ``session_authorization``, ``DateStyle``, ``TimeZone``,
        ``integer_datetimes``, and ``standard_conforming_strings``.

        If server did not report requested parameter, return ``None``.

        .. seealso:: libpq docs for `PQparameterStatus()`__ for details.

            .. __: http://www.postgresql.org/docs/8.4/static/libpq-status.html#AEN33499

    .. method:: get_transaction_status()

        Return the current session transaction status as an integer.  Symbolic
        constants for the vaules are defined in the module
        :mod:`psycopg2.extensions`: see :ref:`transaction-status-constants`
        for the available values.

        .. seealso:: libpq docs for `PQtransactionStatus()`__ for details.

            .. __: http://www.postgresql.org/docs/8.4/static/libpq-status.html#AEN33480

    .. attribute:: protocol_version

        A read-ony integer representing frontend/backend protocol being used.
        It can be 2 or 3.

        .. seealso:: libpq docs for `PQprotocolVersion()`__ for details.

            .. __: http://www.postgresql.org/docs/8.4/static/libpq-status.html#AEN33546

    .. attribute:: server_version

        A read-only integer representing the backend version.

        The number is formed by converting the major, minor, and revision
        numbers into two-decimal-digit numbers and appending them together.
        For example, version 8.1.5 will be returned as 80105,
        
        .. seealso:: libpq docs for `PQserverVersion()`__ for details.

            .. __: http://www.postgresql.org/docs/8.4/static/libpq-status.html#AEN33556


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

    .. attribute:: notifies

        .. todo:: describe conn.notifies

    .. attribute:: notices

        .. todo:: describe conn.notices

    .. attribute:: binary_types

        .. todo:: describe binary_types

    .. attribute:: string_types

        .. todo:: describe string_types


    The :class:`connection` also exposes the same `Error` classes available in
    the :mod:`psycopg2` module as attributes.

