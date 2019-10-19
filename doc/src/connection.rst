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

    Connections are thread safe and can be shared among many threads. See
    :ref:`thread-safety` for details.

    .. method:: cursor(name=None, cursor_factory=None, scrollable=None, withhold=False)

        Return a new `cursor` object using the connection.

        If *name* is specified, the returned cursor will be a :ref:`server
        side cursor <server-side-cursors>` (also known as *named cursor*).
        Otherwise it will be a regular *client side* cursor.  By default a
        named cursor is declared without :sql:`SCROLL` option and
        :sql:`WITHOUT HOLD`: set the argument or property `~cursor.scrollable`
        to `!True`/`!False` and or `~cursor.withhold` to `!True` to change the
        declaration.

        The name can be a string not valid as a PostgreSQL identifier: for
        example it may start with a digit and contain non-alphanumeric
        characters and quotes.

        .. versionchanged:: 2.4
            previously only valid PostgreSQL identifiers were accepted as
            cursor name.

        The *cursor_factory* argument can be used to create non-standard
        cursors. The class returned must be a subclass of
        `psycopg2.extensions.cursor`. See :ref:`subclassing-cursor` for
        details. A default factory for the connection can also be specified
        using the `~connection.cursor_factory` attribute.

        .. versionchanged:: 2.4.3 added the *withhold* argument.
        .. versionchanged:: 2.5 added the *scrollable* argument.

        .. extension::

            All the function arguments are Psycopg extensions to the |DBAPI|.


    .. index::
        pair: Transaction; Commit

    .. method:: commit()

        Commit any pending transaction to the database.

        By default, Psycopg opens a transaction before executing the first
        command: if `!commit()` is not called, the effect of any data
        manipulation will be lost.

        The connection can be also set in "autocommit" mode: no transaction is
        automatically open, commands have immediate effect. See
        :ref:`transactions-control` for details.

        .. versionchanged:: 2.5 if the connection is used in a ``with``
            statement, the method is automatically called if no exception is
            raised in the ``with`` block.


    .. index::
        pair: Transaction; Rollback

    .. method:: rollback()

        Roll back to the start of any pending transaction.  Closing a
        connection without committing the changes first will cause an implicit
        rollback to be performed.

        .. versionchanged:: 2.5 if the connection is used in a ``with``
            statement, the method is automatically called if an exception is
            raised in the ``with`` block.


    .. method:: close()

        Close the connection now (rather than whenever `del` is executed).
        The connection will be unusable from this point forward; an
        `~psycopg2.InterfaceError` will be raised if any operation is
        attempted with the connection.  The same applies to all cursor objects
        trying to use the connection.  Note that closing a connection without
        committing the changes first will cause any pending change to be
        discarded as if a :sql:`ROLLBACK` was performed (unless a different
        isolation level has been selected: see
        `~connection.set_isolation_level()`).

        .. index::
            single: PgBouncer; unclean server

        .. versionchanged:: 2.2
            previously an explicit :sql:`ROLLBACK` was issued by Psycopg on
            `!close()`. The command could have been sent to the backend at an
            inappropriate time, so Psycopg currently relies on the backend to
            implicitly discard uncommitted changes. Some middleware are known
            to behave incorrectly though when the connection is closed during
            a transaction (when `~connection.status` is
            `~psycopg2.extensions.STATUS_IN_TRANSACTION`), e.g. PgBouncer_
            reports an ``unclean server`` and discards the connection. To
            avoid this problem you can ensure to terminate the transaction
            with a `~connection.commit()`/`~connection.rollback()` before
            closing.

            .. _PgBouncer: http://www.pgbouncer.org/


    .. index::
        single: Exceptions; In the connection class

    .. rubric:: Exceptions as connection class attributes

    The `!connection` also exposes as attributes the same exceptions
    available in the `psycopg2` module.  See :ref:`dbapi-exceptions`.



    .. index::
        single: Two-phase commit; methods

    .. rubric:: Two-phase commit support methods

    .. versionadded:: 2.3

    .. seealso:: :ref:`tpc` for an introductory explanation of these methods.

    Note that PostgreSQL supports two-phase commit since release 8.1: these
    methods raise `~psycopg2.NotSupportedError` if used with an older version
    server.


    .. _tpc_methods:

    .. method:: xid(format_id, gtrid, bqual)

        Returns a `~psycopg2.extensions.Xid` instance to be passed to the
        `!tpc_*()` methods of this connection. The argument types and
        constraints are explained in :ref:`tpc`.

        The values passed to the method will be available on the returned
        object as the members `~psycopg2.extensions.Xid.format_id`,
        `~psycopg2.extensions.Xid.gtrid`, `~psycopg2.extensions.Xid.bqual`.
        The object also allows accessing to these members and unpacking as a
        3-items tuple.


    .. method:: tpc_begin(xid)

        Begins a TPC transaction with the given transaction ID *xid*.

        This method should be called outside of a transaction (i.e. nothing
        may have executed since the last `~connection.commit()` or
        `~connection.rollback()` and `connection.status` is
        `~psycopg2.extensions.STATUS_READY`).

        Furthermore, it is an error to call `!commit()` or `!rollback()`
        within the TPC transaction: in this case a `~psycopg2.ProgrammingError`
        is raised.

        The *xid* may be either an object returned by the `~connection.xid()`
        method or a plain string: the latter allows to create a transaction
        using the provided string as PostgreSQL transaction id. See also
        `~connection.tpc_recover()`.


    .. index::
        pair: Transaction; Prepare

    .. method:: tpc_prepare()

        Performs the first phase of a transaction started with
        `~connection.tpc_begin()`.  A `~psycopg2.ProgrammingError` is raised if
        this method is used outside of a TPC transaction.

        After calling `!tpc_prepare()`, no statements can be executed until
        `~connection.tpc_commit()` or `~connection.tpc_rollback()` will be
        called.  The `~connection.reset()` method can be used to restore the
        status of the connection to `~psycopg2.extensions.STATUS_READY`: the
        transaction will remain prepared in the database and will be
        possible to finish it with `!tpc_commit(xid)` and
        `!tpc_rollback(xid)`.

        .. seealso:: the |PREPARE TRANSACTION|_ PostgreSQL command.

        .. |PREPARE TRANSACTION| replace:: :sql:`PREPARE TRANSACTION`
        .. _PREPARE TRANSACTION: https://www.postgresql.org/docs/current/static/sql-prepare-transaction.html


    .. index::
        pair: Commit; Prepared

    .. method:: tpc_commit([xid])

        When called with no arguments, `!tpc_commit()` commits a TPC
        transaction previously prepared with `~connection.tpc_prepare()`.

        If `!tpc_commit()` is called prior to `!tpc_prepare()`, a single phase
        commit is performed.  A transaction manager may choose to do this if
        only a single resource is participating in the global transaction.

        When called with a transaction ID *xid*, the database commits
        the given transaction.  If an invalid transaction ID is
        provided, a `~psycopg2.ProgrammingError` will be raised.  This form
        should be called outside of a transaction, and is intended for use in
        recovery.

        On return, the TPC transaction is ended.

        .. seealso:: the |COMMIT PREPARED|_ PostgreSQL command.

        .. |COMMIT PREPARED| replace:: :sql:`COMMIT PREPARED`
        .. _COMMIT PREPARED: https://www.postgresql.org/docs/current/static/sql-commit-prepared.html


    .. index::
        pair: Rollback; Prepared

    .. method:: tpc_rollback([xid])

        When called with no arguments, `!tpc_rollback()` rolls back a TPC
        transaction.  It may be called before or after
        `~connection.tpc_prepare()`.

        When called with a transaction ID *xid*, it rolls back the given
        transaction.  If an invalid transaction ID is provided, a
        `~psycopg2.ProgrammingError` is raised.  This form should be called
        outside of a transaction, and is intended for use in recovery.

        On return, the TPC transaction is ended.

        .. seealso:: the |ROLLBACK PREPARED|_ PostgreSQL command.

        .. |ROLLBACK PREPARED| replace:: :sql:`ROLLBACK PREPARED`
        .. _ROLLBACK PREPARED: https://www.postgresql.org/docs/current/static/sql-rollback-prepared.html


    .. index::
        pair: Transaction; Recover

    .. method:: tpc_recover()

        Returns a list of `~psycopg2.extensions.Xid` representing pending
        transactions, suitable for use with `tpc_commit()` or
        `tpc_rollback()`.

        If a transaction was not initiated by Psycopg, the returned Xids will
        have attributes `~psycopg2.extensions.Xid.format_id` and
        `~psycopg2.extensions.Xid.bqual` set to `!None` and the
        `~psycopg2.extensions.Xid.gtrid` set to the PostgreSQL transaction ID: such Xids are still
        usable for recovery.  Psycopg uses the same algorithm of the
        `PostgreSQL JDBC driver`__ to encode a XA triple in a string, so
        transactions initiated by a program using such driver should be
        unpacked correctly.

        .. __: https://jdbc.postgresql.org/

        Xids returned by `!tpc_recover()` also have extra attributes
        `~psycopg2.extensions.Xid.prepared`, `~psycopg2.extensions.Xid.owner`,
        `~psycopg2.extensions.Xid.database` populated with the values read
        from the server.

        .. seealso:: the |pg_prepared_xacts|_ system view.

        .. |pg_prepared_xacts| replace:: `pg_prepared_xacts`
        .. _pg_prepared_xacts: https://www.postgresql.org/docs/current/static/view-pg-prepared-xacts.html



    .. extension::

        The above methods are the only ones defined by the |DBAPI| protocol.
        The Psycopg connection objects exports the following additional
        methods and attributes.


    .. attribute:: closed

        Read-only integer attribute: 0 if the connection is open, nonzero if
        it is closed or broken.


    .. method:: cancel

        Cancel the current database operation.

        The method interrupts the processing of the current operation. If no
        query is being executed, it does nothing. You can call this function
        from a different thread than the one currently executing a database
        operation, for instance if you want to cancel a long running query if a
        button is pushed in the UI. Interrupting query execution will cause the
        cancelled method to raise a
        `~psycopg2.extensions.QueryCanceledError`. Note that the termination
        of the query is not guaranteed to succeed: see the documentation for
        |PQcancel|_.

        .. |PQcancel| replace:: `!PQcancel()`
        .. _PQcancel: https://www.postgresql.org/docs/current/static/libpq-cancel.html#LIBPQ-PQCANCEL

        .. versionadded:: 2.3


    .. method:: reset

        Reset the connection to the default.

        The method rolls back an eventual pending transaction and executes the
        PostgreSQL |RESET|_ and |SET SESSION AUTHORIZATION|__ to revert the
        session to the default values. A two-phase commit transaction prepared
        using `~connection.tpc_prepare()` will remain in the database
        available for recover.

        .. |RESET| replace:: :sql:`RESET`
        .. _RESET: https://www.postgresql.org/docs/current/static/sql-reset.html

        .. |SET SESSION AUTHORIZATION| replace:: :sql:`SET SESSION AUTHORIZATION`
        .. __: https://www.postgresql.org/docs/current/static/sql-set-session-authorization.html

        .. versionadded:: 2.0.12


    .. attribute:: dsn

        Read-only string containing the connection string used by the
        connection.

        If a password was specified in the connection string it will be
        obscured.



    .. rubric:: Transaction control methods and attributes.

    .. index::
        pair: Transaction; Autocommit
        pair: Transaction; Isolation level

    .. method:: set_session(isolation_level=None, readonly=None, deferrable=None, autocommit=None)

        Set one or more parameters for the next transactions or statements in
        the current session.

        :param isolation_level: set the `isolation level`_ for the next
            transactions/statements.  The value can be one of the literal
            values ``READ UNCOMMITTED``, ``READ COMMITTED``, ``REPEATABLE
            READ``, ``SERIALIZABLE`` or the equivalent :ref:`constant
            <isolation-level-constants>` defined in the `~psycopg2.extensions`
            module.
        :param readonly: if `!True`, set the connection to read only;
            read/write if `!False`.
        :param deferrable: if `!True`, set the connection to deferrable;
            non deferrable if `!False`. Only available from PostgreSQL 9.1.
        :param autocommit: switch the connection to autocommit mode: not a
            PostgreSQL session setting but an alias for setting the
            `autocommit` attribute.

        .. _isolation level:
            https://www.postgresql.org/docs/current/static/transaction-iso.html

        Arguments set to `!None` (the default for all) will not be changed.
        The parameters *isolation_level*, *readonly* and *deferrable* also
        accept the string ``DEFAULT`` as a value: the effect is to reset the
        parameter to the server default.  Defaults are defined by the server
        configuration: see values for |default_transaction_isolation|__,
        |default_transaction_read_only|__, |default_transaction_deferrable|__.

        .. |default_transaction_isolation| replace:: :sql:`default_transaction_isolation`
        .. __: https://www.postgresql.org/docs/current/static/runtime-config-client.html#GUC-DEFAULT-TRANSACTION-ISOLATION
        .. |default_transaction_read_only| replace:: :sql:`default_transaction_read_only`
        .. __: https://www.postgresql.org/docs/current/static/runtime-config-client.html#GUC-DEFAULT-TRANSACTION-READ-ONLY
        .. |default_transaction_deferrable| replace:: :sql:`default_transaction_deferrable`
        .. __: https://www.postgresql.org/docs/current/static/runtime-config-client.html#GUC-DEFAULT-TRANSACTION-DEFERRABLE

        The function must be invoked with no transaction in progress.

        .. seealso:: |SET TRANSACTION|_ for further details about the behaviour
            of the transaction parameters in the server.

            .. |SET TRANSACTION| replace:: :sql:`SET TRANSACTION`
            .. _SET TRANSACTION: https://www.postgresql.org/docs/current/static/sql-set-transaction.html

        .. versionadded:: 2.4.2

        .. versionchanged:: 2.7
            Before this version, the function would have set
            :sql:`default_transaction_*` attribute in the current session;
            this implementation has the problem of not playing well with
            external connection pooling working at transaction level and not
            resetting the state of the session: changing the default
            transaction would pollute the connections in the pool and create
            problems to other applications using the same pool.

            Starting from 2.7, if the connection is not autocommit, the
            transaction characteristics are issued together with :sql:`BEGIN`
            and will leave the :sql:`default_transaction_*` settings untouched.
            For example::

                conn.set_session(readonly=True)

            will not change :sql:`default_transaction_read_only`, but
            following transaction will start with a :sql:`BEGIN READ ONLY`.
            Conversely, using::

                conn.set_session(readonly=True, autocommit=True)

            will set :sql:`default_transaction_read_only` to :sql:`on` and
            rely on the server to apply the read only state to whatever
            transaction, implicit or explicit, is executed in the connection.


    .. attribute:: autocommit

        Read/write attribute: if `!True`, no transaction is handled by the
        driver and every statement sent to the backend has immediate effect;
        if `!False` a new transaction is started at the first command
        execution: the methods `commit()` or `rollback()` must be manually
        invoked to terminate the transaction.

        The autocommit mode is useful to execute commands requiring to be run
        outside a transaction, such as :sql:`CREATE DATABASE` or
        :sql:`VACUUM`.

        The default is `!False` (manual commit) as per DBAPI specification.

        .. warning::

            By default, any query execution, including a simple :sql:`SELECT`
            will start a transaction: for long-running programs, if no further
            action is taken, the session will remain "idle in transaction", an
            undesirable condition for several reasons (locks are held by
            the session, tables bloat...). For long lived scripts, either
            ensure to terminate a transaction as soon as possible or use an
            autocommit connection.

        .. versionadded:: 2.4.2


    .. attribute:: isolation_level

        Return or set the `transaction isolation level`_ for the current
        session.  The value is one of the :ref:`isolation-level-constants`
        defined in the `psycopg2.extensions` module.  On set it is also
        possible to use one of the literal values ``READ UNCOMMITTED``, ``READ
        COMMITTED``, ``REPEATABLE READ``, ``SERIALIZABLE``, ``DEFAULT``.

        .. versionchanged:: 2.7

            the property is writable.

        .. versionchanged:: 2.7

            the default value for `!isolation_level` is
            `~psycopg2.extensions.ISOLATION_LEVEL_DEFAULT`; previously the
            property would have queried the server and returned the real value
            applied. To know this value you can run a query such as :sql:`show
            transaction_isolation`. Usually the default value is `READ
            COMMITTED`, but this may be changed in the server configuration.

            This value is now entirely separate from the `autocommit`
            property: in previous version, if `!autocommit` was set to `!True`
            this property would have returned
            `~psycopg2.extensions.ISOLATION_LEVEL_AUTOCOMMIT`; it will now
            return the server isolation level.


    .. attribute:: readonly

        Return or set the read-only status for the current session. Available
        values are `!True` (new transactions will be in read-only mode),
        `!False` (new transactions will be writable), `!None` (use the default
        configured for the server by :sql:`default_transaction_read_only`).

        .. versionadded:: 2.7


    .. attribute:: deferrable

        Return or set the `deferrable status`__ for the current session.
        Available values are `!True` (new transactions will be in deferrable
        mode), `!False` (new transactions will be in non deferrable mode),
        `!None` (use the default configured for the server by
        :sql:`default_transaction_deferrable`).

        .. __: `SET TRANSACTION`_

        .. versionadded:: 2.7


    .. method:: set_isolation_level(level)

        .. note::

            This is a legacy method mixing `~conn.isolation_level` and
            `~conn.autocommit`. Using the respective properties is a better
            option.

        Set the `transaction isolation level`_ for the current session.
        The level defines the different phenomena that can happen in the
        database between concurrent transactions.

        The value set is an integer: symbolic constants are defined in
        the module `psycopg2.extensions`: see
        :ref:`isolation-level-constants` for the available values.

        The default level is `~psycopg2.extensions.ISOLATION_LEVEL_DEFAULT`:
        at this level a transaction is automatically started the first time a
        database command is executed.  If you want an *autocommit* mode,
        switch to `~psycopg2.extensions.ISOLATION_LEVEL_AUTOCOMMIT` before
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

        .. __: https://www.postgresql.org/docs/current/static/multibyte.html


    .. index::
        pair: Client; Logging

    .. attribute:: notices

        A list containing all the database messages sent to the client during
        the session.

        .. doctest::
            :options: +NORMALIZE_WHITESPACE

            >>> cur.execute("CREATE TABLE foo (id serial PRIMARY KEY);")
            >>> pprint(conn.notices)
            ['NOTICE:  CREATE TABLE / PRIMARY KEY will create implicit index "foo_pkey" for table "foo"\n',
             'NOTICE:  CREATE TABLE will create implicit sequence "foo_id_seq" for serial column "foo.id"\n']

        .. versionchanged:: 2.7
            The `!notices` attribute is writable: the user may replace it
            with any Python object exposing an `!append()` method. If
            appending raises an exception the notice is silently
            dropped.

        To avoid a leak in case excessive notices are generated, only the last
        50 messages are kept. This check is only in place if the `!notices`
        attribute is a list: if any other object is used it will be up to the
        user to guard from leakage.

        You can configure what messages to receive using `PostgreSQL logging
        configuration parameters`__ such as ``log_statement``,
        ``client_min_messages``, ``log_min_duration_statement`` etc.

        .. __: https://www.postgresql.org/docs/current/static/runtime-config-logging.html


    .. attribute:: notifies

        List of `~psycopg2.extensions.Notify` objects containing asynchronous
        notifications received by the session.

        For other details see :ref:`async-notify`.

        .. versionchanged:: 2.3
            Notifications are instances of the `!Notify` object. Previously the
            list was composed by 2 items tuples :samp:`({pid},{channel})` and
            the payload was not accessible. To keep backward compatibility,
            `!Notify` objects can still be accessed as 2 items tuples.

        .. versionchanged:: 2.7
            The `!notifies` attribute is writable: the user may replace it
            with any Python object exposing an `!append()` method. If
            appending raises an exception the notification is silently
            dropped.


    .. attribute:: cursor_factory

        The default cursor factory used by `~connection.cursor()` if the
        parameter is not specified.

        .. versionadded:: 2.5


    .. index::
        pair: Connection; Info

    .. attribute:: info

        A `~psycopg2.extensions.ConnectionInfo` object exposing information
        about the native libpq connection.

        .. versionadded:: 2.8


    .. index::
        pair: Connection; Status

    .. attribute:: status

        A read-only integer representing the status of the connection.
        Symbolic constants for the values are defined in the module
        `psycopg2.extensions`: see :ref:`connection-status-constants`
        for the available values.

        The status is undefined for `closed` connections.


    .. method:: lobject([oid [, mode [, new_oid [, new_file [, lobject_factory]]]]])

        Return a new database large object as a `~psycopg2.extensions.lobject`
        instance.

        See :ref:`large-objects` for an overview.

        :param oid: The OID of the object to read or write. 0 to create
            a new large object and and have its OID assigned automatically.
        :param mode: Access mode to the object, see below.
        :param new_oid: Create a new object using the specified OID. The
            function raises `~psycopg2.OperationalError` if the OID is already
            in use. Default is 0, meaning assign a new one automatically.
        :param new_file: The name of a file to be imported in the database
            (using the |lo_import|_ function)
        :param lobject_factory: Subclass of
            `~psycopg2.extensions.lobject` to be instantiated.

        .. |lo_import| replace:: `!lo_import()`
        .. _lo_import: https://www.postgresql.org/docs/current/static/lo-interfaces.html#LO-IMPORT

        Available values for *mode* are:

        ======= =========
        *mode*  meaning
        ======= =========
        ``r``   Open for read only
        ``w``   Open for write only
        ``rw``  Open for read/write
        ``n``   Don't open the file
        ``b``   Don't decode read data (return data as `!str` in Python 2 or `!bytes` in Python 3)
        ``t``   Decode read data according to `connection.encoding` (return data as `!unicode` in Python 2 or `!str` in Python 3)
        ======= =========

        ``b`` and ``t`` can be specified together with a read/write mode. If
        neither ``b`` nor ``t`` is specified, the default is ``b`` in Python 2
        and ``t`` in Python 3.

        .. versionadded:: 2.0.8

        .. versionchanged:: 2.4 added ``b`` and ``t`` mode and unicode
            support.


    .. rubric:: Methods related to asynchronous support

    .. versionadded:: 2.2

    .. seealso:: :ref:`async-support` and :ref:`green-support`.


    .. attribute:: async
                   async_

        Read only attribute: 1 if the connection is asynchronous, 0 otherwise.

        .. versionchanged:: 2.7 added the `!async_` alias for Python versions
            where `!async` is a keyword.


    .. method:: poll()

        Used during an asynchronous connection attempt, or when a cursor is
        executing a query on an asynchronous connection, make communication
        proceed if it wouldn't block.

        Return one of the constants defined in :ref:`poll-constants`. If it
        returns `~psycopg2.extensions.POLL_OK` then the connection has been
        established or the query results are available on the client.
        Otherwise wait until the file descriptor returned by `fileno()` is
        ready to read or to write, as explained in :ref:`async-support`.
        `poll()` should be also used by the function installed by
        `~psycopg2.extensions.set_wait_callback()` as explained in
        :ref:`green-support`.

        `poll()` is also used to receive asynchronous notifications from the
        database: see :ref:`async-notify` from further details.


    .. method:: fileno()

        Return the file descriptor underlying the connection: useful to read
        its status during asynchronous communication.


    .. method:: isexecuting()

        Return `!True` if the connection is executing an asynchronous operation.


    .. rubric:: Interoperation with other C API modules

    .. attribute:: pgconn_ptr

        Return the internal `!PGconn*` as integer. Useful to pass the libpq
        raw connection structure to C functions, e.g. via `ctypes`::

            >>> import ctypes
            >>> libpq = ctypes.pydll.LoadLibrary(ctypes.util.find_library('pq'))
            >>> libpq.PQserverVersion.argtypes = [ctypes.c_void_p]
            >>> libpq.PQserverVersion.restype = ctypes.c_int
            >>> libpq.PQserverVersion(conn.pgconn_ptr)
            90611

        .. versionadded:: 2.8


    .. method:: get_native_connection()

        Return the internal `!PGconn*` wrapped in a PyCapsule object. This is
        only useful for passing the `libpq` raw connection associated to this
        connection object to other C-level modules that may have a use for it.

        .. seealso:: Python C API `Capsules`__  docs.

            .. __: https://docs.python.org/3.1/c-api/capsule.html

        .. versionadded:: 2.8



    .. rubric:: informative methods of the native connection

    .. note::

        These methods are better accessed using the `~connection.info`
        attributes and may be dropped in future versions.


    .. index::
        pair: Transaction; Status

    .. method:: get_transaction_status()

        Also available as `~connection.info`\ `!.`\
        `~psycopg2.extensions.ConnectionInfo.transaction_status`.

        Return the current session transaction status as an integer.  Symbolic
        constants for the values are defined in the module
        `psycopg2.extensions`: see :ref:`transaction-status-constants`
        for the available values.

        .. seealso:: libpq docs for `PQtransactionStatus()`__ for details.

            .. __: https://www.postgresql.org/docs/current/static/libpq-status.html#LIBPQ-PQTRANSACTIONSTATUS


    .. index::
        pair: Protocol; Version

    .. attribute:: protocol_version

        Also available as `~connection.info`\ `!.`\
        `~psycopg2.extensions.ConnectionInfo.protocol_version`.

        A read-only integer representing frontend/backend protocol being used.
        Currently Psycopg supports only protocol 3, which allows connection
        to PostgreSQL server from version 7.4. Psycopg versions previous than
        2.3 support both protocols 2 and 3.

        .. seealso:: libpq docs for `PQprotocolVersion()`__ for details.

            .. __: https://www.postgresql.org/docs/current/static/libpq-status.html#LIBPQ-PQPROTOCOLVERSION

        .. versionadded:: 2.0.12


    .. index::
        pair: Server; Version

    .. attribute:: server_version

        Also available as `~connection.info`\ `!.`\
        `~psycopg2.extensions.ConnectionInfo.server_version`.

        A read-only integer representing the backend version.

        The number is formed by converting the major, minor, and revision
        numbers into two-decimal-digit numbers and appending them together.
        For example, version 8.1.5 will be returned as ``80105``.

        .. seealso:: libpq docs for `PQserverVersion()`__ for details.

            .. __: https://www.postgresql.org/docs/current/static/libpq-status.html#LIBPQ-PQSERVERVERSION

        .. versionadded:: 2.0.12


    .. index::
        pair: Backend; PID

    .. method:: get_backend_pid()

        Also available as `~connection.info`\ `!.`\
        `~psycopg2.extensions.ConnectionInfo.backend_pid`.

        Returns the process ID (PID) of the backend server process *you
        connected to*. Note that if you use a connection pool service such as
        PgBouncer_ this value will not be updated if your connection is
        switched to a different backend.

        Note that the PID belongs to a process executing on the database
        server host, not the local host!

        .. seealso:: libpq docs for `PQbackendPID()`__ for details.

            .. __: https://www.postgresql.org/docs/current/static/libpq-status.html#LIBPQ-PQBACKENDPID

        .. versionadded:: 2.0.8


    .. index::
        pair: Server; Parameters

    .. method:: get_parameter_status(parameter)

        Also available as `~connection.info`\ `!.`\
        `~psycopg2.extensions.ConnectionInfo.parameter_status()`.

        Look up a current parameter setting of the server.

        Potential values for ``parameter`` are: ``server_version``,
        ``server_encoding``, ``client_encoding``, ``is_superuser``,
        ``session_authorization``, ``DateStyle``, ``TimeZone``,
        ``integer_datetimes``, and ``standard_conforming_strings``.

        If server did not report requested parameter, return `!None`.

        .. seealso:: libpq docs for `PQparameterStatus()`__ for details.

            .. __: https://www.postgresql.org/docs/current/static/libpq-status.html#LIBPQ-PQPARAMETERSTATUS

        .. versionadded:: 2.0.12


    .. index::
        pair: Connection; Parameters

    .. method:: get_dsn_parameters()

        Also available as `~connection.info`\ `!.`\
        `~psycopg2.extensions.ConnectionInfo.dsn_parameters`.

        Get the effective dsn parameters for the connection as a dictionary.

        The *password* parameter is removed from the result.

        Example::

            >>> conn.get_dsn_parameters()
            {'dbname': 'test', 'user': 'postgres', 'port': '5432', 'sslmode': 'prefer'}

        Requires libpq >= 9.3.

        .. seealso:: libpq docs for `PQconninfo()`__ for details.

            .. __: https://www.postgresql.org/docs/current/static/libpq-connect.html#LIBPQ-PQCONNINFO

        .. versionadded:: 2.7


.. testcode::
    :hide:

    conn.rollback()
