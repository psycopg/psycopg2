`psycopg2.sql` -- SQL string composition
========================================

.. sectionauthor:: Daniele Varrazzo <daniele.varrazzo@gmail.com>

.. module:: psycopg2.sql

.. versionadded:: 2.7

The module contains objects and functions useful to generate SQL dynamically,
in a convenient and safe way. SQL identifiers (e.g. names of tables and
fields) cannot be passed to the `~cursor.execute()` method like query
arguments::

    # This will not work
    table_name = 'my_table'
    cur.execute("insert into %s values (%s, %s)", [table_name, 10, 20])

The SQL query should be composed before the arguments are merged, for
instance::

    # This works, but it is not optimal
    table_name = 'my_table'
    cur.execute(
        "insert into %s values (%%s, %%s)" % table_name,
        [10, 20])

This sort of works, but it is an accident waiting to happen: the table name
may be an invalid SQL literal and need quoting; even more serious is the
security problem in case the table name comes from an untrusted source. The
name should be escaped using `~psycopg2.extensions.quote_ident()`::

    # This works, but it is not optimal
    table_name = 'my_table'
    cur.execute(
        "insert into %s values (%%s, %%s)" % ext.quote_ident(table_name),
        [10, 20])

This is now safe, but it somewhat ad-hoc. In case, for some reason, it is
necessary to include a value in the query string (as opposite as in a value)
the merging rule is still different (`~psycopg2.extensions.adapt()` should be
used...). It is also still relatively dangerous: if `!quote_ident()` is
forgotten somewhere, the program will usually work, but will eventually crash
in the presence of a table or field name with containing characters to escape,
or will present a potentially exploitable weakness.

The objects exposed by the `!psycopg2.sql` module allow generating SQL
statements on the fly, separating clearly the variable parts of the statement
from the query parameters::

    from psycopg2 import sql

    cur.execute(
        sql.SQL("insert into {} values (%s, %s)")
            .format(sql.Identifier('my_table')),
        [10, 20])

The objects exposed by the `!sql` module can be used to compose a query as a
Python string (using the `~Composable.as_string()` method) or passed directly
to cursor methods such as `~cursor.execute()`, `~cursor.executemany()`,
`~cursor.copy_expert()`.


.. autoclass:: Composable

    .. automethod:: as_string


.. autoclass:: SQL

    .. autoattribute:: string

    .. automethod:: format

    .. automethod:: join


.. autoclass:: Identifier

    .. autoattribute:: string

.. autoclass:: Literal

    .. autoattribute:: wrapped

.. autoclass:: Placeholder

    .. autoattribute:: name

.. autoclass:: Composed

    .. autoattribute:: seq

    .. automethod:: join
