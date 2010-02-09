The ``cursor`` class
====================

.. sectionauthor:: Daniele Varrazzo <daniele.varrazzo@gmail.com>

.. class:: cursor

    Allows Python code to execute PostgreSQL command in a database session.
    Cursors are created by the :meth:`connection.cursor`: they are bound to
    the connection for the entire lifetime and all the commands are executed 
    in the context of the database session wrapped by the connection.

    Cursors created from the same connection are not isolated, i.e., any
    changes done to the database by a cursor are immediately visible by the
    other cursors. Cursors created from different connections can or can not
    be isolated, depending on the :attr:`connection.isolation_level`. See also
    :meth:`connection.rollback()` and :meth:`connection.commit()` methods.

    Cursors are *not* thread safe: a multithread application can create
    many cursors from the same same connection and should use each cursor from
    a single thread. See :ref:`thread-safety` for details.

 
    .. attribute:: description 

        This read-only attribute is a sequence of 7-item sequences.  

        Each of these sequences contains information describing one result
        column: 

        - ``name``
        - ``type_code``
        - ``display_size``
        - ``internal_size``
        - ``precision``
        - ``scale``
        - ``null_ok``

        The first two items (``name`` and ``type_code``) are mandatory, the
        other five are optional and are set to ``None`` if no meaningful
        values can be provided.

        This attribute will be ``None`` for operations that do not return rows
        or if the cursor has not had an operation invoked via the
        |execute*|_ methods yet.
        
        The type_code can be interpreted by comparing it to the Type Objects
        specified in the section :ref:`type-objects-and-constructors`.

    .. method:: close()
          
        Close the cursor now (rather than whenever ``__del__`` is called).
        The cursor will be unusable from this point forward; an :exc:`Error` (or
        subclass) exception will be raised if any operation is attempted with
        the cursor.
            
    .. attribute:: closed

        Read-only boolean attribute: specifies if the cursor is closed
        (``True``) or not (``False``).

    .. attribute:: connection

        Read-only attribute returning a reference to the :class:`connection`
        object on which the cursor was created.

    .. attribute:: name

        Read-only attribute containing the name of the cursor if it was
        creates as named cursor by :meth:`connection.cursor`, or ``None`` if
        it is a client side cursor.  See :ref:`server-side-cursors`.

    .. |execute*| replace:: :obj:`execute*()`

    .. _execute*:

    .. method:: execute(operation [, parameters] [, async]) 
      
        Prepare and execute a database operation (query or command).

        Parameters may be provided as sequence or mapping and will be bound to
        variables in the operation.  Variables are specified either with
        positional (``%s``) or named (``%(name)s``) placeholders. See
        :ref:`query-parameters`.
        
        The method returns `None`. If a query was executed, the returned
        values can be retrieved using |fetch*|_ methods.

        A reference to the operation will be retained by the cursor.  If the
        same operation object is passed in again, then the cursor can optimize
        its behavior.  This is most effective for algorithms where the same
        operation is used, but different parameters are bound to it (many
        times).

        .. todo:: does Psycopg2 do the above?
        
        If :obj:`async` is ``True``, query execution will be asynchronous: the
        function returns immediately while the query is executed by the
        backend.  Use the :attr:`isready` attribute to see if the data is
        ready for return via |fetch*|_ methods. See
        :ref:`asynchronous-queries`.

    .. method:: mogrify(operation [, parameters)

        Return a query string after arguments binding. The string returned is
        exactly the one that would be sent to the database running the
        :meth:`execute()` method or similar.
        
    .. method:: executemany(operation, seq_of_parameters)
      
        Prepare a database operation (query or command) and then execute it
        against all parameter sequences or mappings found in the sequence
        seq_of_parameters.
        
        The function is mostly useful for commands that update the database:
        any result set returned by the query is discarded.
        
        Parameters are bounded to the query using the same rules described in
        the :meth:`execute()` method.

    .. method:: callproc(procname [, parameters] [, async])
            
        Call a stored database procedure with the given name. The sequence of
        parameters must contain one entry for each argument that the procedure
        expects. The result of the call is returned as modified copy of the
        input sequence. Input parameters are left untouched, output and
        input/output parameters replaced with possibly new values.
        
        The procedure may also provide a result set as output. This must then
        be made available through the standard |fetch*|_ methods.

        If :obj:`async` is ``True``, procedure execution will be asynchronous:
        the function returns immediately while the procedure is executed by
        the backend.  Use the :attr:`isready` attribute to see if the data is
        ready for return via |fetch*|_ methods. See
        :ref:`asynchronous-queries`.

    .. attribute:: query

        Read-only attribute containing the body of the last query sent to the
        backend (including bound arguments). ``None`` if no query has been
        executed yet::

            >>> cur.execute("INSERT INTO test (num, data) VALUES (%s, %s)", (42, 'bar'))
            >>> cur.query 
            "INSERT INTO test (num, data) VALUES (42, E'bar')"

    .. attribute:: statusmessage

        Return the message returned by the last command::

            >>> cur.execute("INSERT INTO test (num, data) VALUES (%s, %s)", (42, 'bar'))
            >>> cur.statusmessage 
            'INSERT 0 1'

    .. method:: isready()

        Return ``False`` if the backend is still processing an asynchronous
        query or ``True`` if data is ready to be fetched by one of the
        |fetch*|_ methods.  See :ref:`asynchronous-queries`.

    .. method:: fileno()

        Return the file descriptor associated with the current connection and
        make possible to use a cursor in a context where a file object would
        be expected (like in a :func:`select()` call).  See
        :ref:`asynchronous-queries`.


    .. |fetch*| replace:: :obj:`fetch*()`

    .. _fetch*:

    The following methods are used to read data from the database after an
    :meth:`execute()` call.

    .. note::

        :class:`cursor` objects are iterable, so, instead of calling
        explicitly :meth:`fetchone()` in a loop, the object itself can be
        used::

            >>> cur.execute("SELECT * FROM test;")
            >>> for record in cur:
            ...     print record
            ...
            (1, 100, "abc'def")
            (2, None, 'dada')
            (4, 42, 'bar')

    .. method:: fetchone()

        Fetch the next row of a query result set, returning a single tuple,
        or ``None`` when no more data is available::

            >>> cur.execute("SELECT * FROM test WHERE id = %s", (4,))
            >>> cur.fetchone()
            (4, 42, 'bar')
        
        An :exc:`Error` (or subclass) exception is raised if the previous call
        to |execute*|_ did not produce any result set or no call was issued
        yet.

    .. method:: fetchmany([size=cursor.arraysize])
      
        Fetch the next set of rows of a query result, returning a list of
        tuples. An empty list is returned when no more rows are available.
        
        The number of rows to fetch per call is specified by the parameter.
        If it is not given, the cursor's :attr:`arraysize` determines the
        number of rows to be fetched. The method should try to fetch as many
        rows as indicated by the size parameter. If this is not possible due
        to the specified number of rows not being available, fewer rows may be
        returned::

            >>> cur.execute("SELECT * FROM test;")
            >>> cur.fetchmany(2)
            [(1, 100, "abc'def"), (2, None, 'dada')]
            >>> cur.fetchmany(2)
            [(4, 42, 'bar')]
            >>> cur.fetchmany(2)
            []

        An :exc:`Error` (or subclass) exception is raised if the previous
        call to |execute*|_ did not produce any result set or no call was
        issued yet.
        
        Note there are performance considerations involved with the size
        parameter.  For optimal performance, it is usually best to use the
        :attr:`arraysize` attribute.  If the size parameter is used, then it
        is best for it to retain the same value from one :meth:`fetchmany()`
        call to the next.

    .. method:: fetchall()

        Fetch all (remaining) rows of a query result, returning them as a list
        of tuples.  Note that the cursor's :attr:`arraysize` attribute can
        affect the performance of this operation::

            >>> cur.execute("SELECT * FROM test;")
            >>> cur.fetchall()
            [(1, 100, "abc'def"), (2, None, 'dada'), (4, 42, 'bar')]

        .. todo:: does arraysize influence fetchall()?
        
        An :exc:`Error` (or subclass) exception is raised if the previous call
        to |execute*|_ did not produce any result set or no call was issued
        yet.

    .. method:: scroll(value[,mode='relative'])

        Scroll the cursor in the result set to a new position according
        to mode.

        If mode is ``relative`` (default), value is taken as offset to
        the current position in the result set, if set to ``absolute``,
        value states an absolute target position.

        If the scroll operation would leave the result set, a
        :exc:`ProgrammingError` is raised and the cursor position is not
        changed.

        .. todo:: dbapi says should have been IndexError...

        The method can be used both for client-side cursors and server-side
        (named) cursors.

    .. attribute:: arraysize
          
        This read/write attribute specifies the number of rows to fetch at a
        time with :meth:`fetchmany()`. It defaults to 1 meaning to fetch a
        single row at a time.
        
        Implementations must observe this value with respect to the
        :meth:`fetchmany()` method, but are free to interact with the database
        a single row at a time. It may also be used in the implementation of
        :meth:`executemany()`.

        .. todo:: copied from dbapi: better specify what psycopg does with
            arraysize

    .. attribute:: rowcount 
          
        This read-only attribute specifies the number of rows that the last
        |execute*|_ produced (for DQL statements like ``SELECT``) or
        affected (for DML statements like ``UPDATE`` or ``INSERT``).
        
        The attribute is -1 in case no |execute*| has been performed on
        the cursor or the row count of the last operation if it can't be
        determined by the interface.

        .. note::
            The |DBAPI 2.0|_ interface reserves to redefine the latter case to
            have the object return ``None`` instead of -1 in future versions
            of the specification.
            
    .. attribute:: rownumber

        This read-only attribute provides the current 0-based index of the
        cursor in the result set or ``None`` if the index cannot be
        determined.

        The index can be seen as index of the cursor in a sequence (the result
        set). The next fetch operation will fetch the row indexed by
        :attr:`rownumber` in that sequence.

    .. index:: oid

    .. attribute:: lastrowid

        This read-only attribute provides the *oid* of the last row inserted
        by the cursor. If the table wasn't created with oid support or the
        last operation is not a single record insert, the attribute is set to
        ``None``.

        PostgreSQL currently advises to not create oid on the tables and the
        default for |CREATE-TABLE|__ is to not support them. The
        |INSERT-RETURNING|__ syntax available from PostgreSQL 8.3 allows more
        flexibility:

        .. |CREATE-TABLE| replace:: ``CREATE TABLE``
        .. __: http://www.postgresql.org/docs/8.4/static/sql-createtable.html

        .. |INSERT-RETURNING| replace:: ``INSERT ... RETURNING``
        .. __: http://www.postgresql.org/docs/8.4/static/sql-insert.html


    .. method:: nextset()
    
        This method is not supported (PostgreSQL does not have multiple data
        sets) and will raise a :exc:`NotSupportedError` exception.

    .. method:: setinputsizes(sizes)
      
        This method currently does nothing but it is safe to call it.

    .. method:: setoutputsize(size [, column])
      
        This method currently does nothing but it is safe to call it.


    .. method:: copy_from(file, table, sep='\\t', null='\\N', columns=None)
 
        Read data *from* the file-like object :obj:`file` appending them to
        the table named :obj:`table`.  :obj:`file` must have both ``read()``
        and ``readline()`` method.  See :ref:`copy` for an overview. ::

            >>> f = StringIO("42\tfoo\n74\tbar\n")
            >>> cur.copy_from(f, 'test', columns=('num', 'data'))

            >>> cur.execute("select * from test where id > 5;")
            >>> cur.fetchall()
            [(7, 42, 'foo'), (8, 74, 'bar')]

        The optional argument :obj:`sep` is the columns separator and
        :obj:`null` represents ``NULL`` values in the file.

        The :obj:`columns` argument is a sequence of field names: if not
        ``None`` only the specified fields will be included in the dump.

        .. versionchanged:: 2.0.6
            added the :obj:`columns` parameter.

    .. method:: copy_to(file, table, sep='\\t', null='\\N', columns=None)

        Write the content of the table named :obj:`table` *to* the file-like object :obj:`file`.  :obj:`file` must have a ``write()`` method. See
        :ref:`copy` for an overview. ::

            >>> cur.copy_to(sys.stdout, 'test', sep="|")
            1|100|abc'def
            2|\N|dada

        The optional argument :obj:`sep` is the columns separator and
        :obj:`null` represents ``NULL`` values in the file.

        The :obj:`columns` argument is a sequence representing the fields
        where the read data will be entered. Its length and column type should
        match the content of the read file.

        .. versionchanged:: 2.0.6
            added the :obj:`columns` parameter.

    .. method:: copy_expert(sql, file [, size])

        Submit a user-composed ``COPY`` statement. The method is useful to
        handle all the parameters that PostgreSQL makes available (see
        |COPY|__ command documentation).

            >>> cur.copy_expert("COPY test TO STDOUT WITH CSV HEADER", sys.stdout)
            id,num,data
            1,100,abc'def
            2,,dada

        :obj:`file` must be an open, readable file for ``COPY FROM`` or an
        open, writeable file for ``COPY TO``. The optional :obj:`size`
        argument, when specified for a ``COPY FROM`` statement, will be passed
        to file's read method to control the read buffer size. 

        .. |COPY| replace:: ``COPY``
        .. __: http://www.postgresql.org/docs/8.4/static/sql-copy.html

        .. versionadded:: 2.0.6

    .. attribute:: row_factory

        .. todo:: cursor.row_factory

    .. attribute:: typecaster

        .. todo:: cursor.typecaster

    .. attribute:: tzinfo_factory

        .. todo:: tzinfo_factory


