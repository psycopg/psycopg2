`psycopg2.errors` -- Exception classes mapping PostgreSQL errors
================================================================

.. sectionauthor:: Daniele Varrazzo <daniele.varrazzo@gmail.com>

.. index::
    single: Error; Class

.. module:: psycopg2.errors

.. versionadded:: 2.8

.. versionchanged:: 2.8.4 added errors introduced in PostgreSQL 12

This module exposes the classes psycopg raises upon receiving an error from
the database with a :sql:`SQLSTATE` value attached (available in the
`~psycopg2.Error.pgcode` attribute). The content of the module is generated
from the PostgreSQL source code and includes classes for every error defined
by PostgreSQL in versions between 9.1 and 12.

Every class in the module is named after what referred as "condition name" `in
the documentation`__, converted to CamelCase: e.g. the error 22012,
``division_by_zero`` is exposed by this module as the class `!DivisionByZero`.

.. __: https://www.postgresql.org/docs/current/static/errcodes-appendix.html#ERRCODES-TABLE

Every exception class is a subclass of one of the :ref:`standard DB-API
exception <dbapi-exceptions>` and expose the `~psycopg2.Error` interface.
Each class' superclass is what used to be raised by psycopg in versions before
the introduction of this module, so everything should be compatible with
previously written code catching one the DB-API class: if your code used to
catch `!IntegrityError` to detect a duplicate entry, it will keep on working
even if a more specialised subclass such as `UniqueViolation` is raised.

The new classes allow a more idiomatic way to check and process a specific
error among the many the database may return. For instance, in order to check
that a table is locked, the following code could have been used previously:

.. code-block:: python

    try:
        cur.execute("LOCK TABLE mytable IN ACCESS EXCLUSIVE MODE NOWAIT")
    except psycopg2.OperationalError as e:
        if e.pgcode == psycopg2.errorcodes.LOCK_NOT_AVAILABLE:
            locked = True
        else:
            raise

While this method is still available, the specialised class allows for a more
idiomatic error handler:

.. code-block:: python

    try:
        cur.execute("LOCK TABLE mytable IN ACCESS EXCLUSIVE MODE NOWAIT")
    except psycopg2.errors.LockNotAvailable:
        locked = True


.. autofunction:: lookup

    .. code-block:: python

        try:
            cur.execute("LOCK TABLE mytable IN ACCESS EXCLUSIVE MODE NOWAIT")
        except psycopg2.errors.lookup("55P03"):
            locked = True


SQLSTATE exception classes
--------------------------

The following table contains the list of all the SQLSTATE classes exposed by
the module.

Note that, for completeness, the module also exposes all the
:ref:`DB-API-defined exceptions <dbapi-exceptions>` and :ref:`a few
psycopg-specific ones <extension-exceptions>` exposed by the `!extensions`
module, which are not listed here.

.. include:: sqlstate_errors.rst
