:mod:`psycopg2.errorcodes` -- Error codes defined by PostgreSQL
===============================================================

.. sectionauthor:: Daniele Varrazzo <daniele.varrazzo@gmail.com>

.. module:: psycopg2.errorcodes

.. versionadded:: 2.0.6

This module contains symbolic names for all PostgreSQL error codes.
Subclasses of :exc:`~psycopg2.Error` make the PostgreSQL error code available
in the :attr:`~psycopg2.Error.pgcode` attribute.

From PostgreSQL documentation:

    All messages emitted by the PostgreSQL server are assigned five-character
    error codes that follow the SQL standard's conventions for :sql:`SQLSTATE`
    codes.  Applications that need to know which error condition has occurred
    should usually test the error code, rather than looking at the textual
    error message.  The error codes are less likely to change across
    PostgreSQL releases, and also are not subject to change due to
    localization of error messages. Note that some, but not all, of the error
    codes produced by PostgreSQL are defined by the SQL standard; some
    additional error codes for conditions not defined by the standard have
    been invented or borrowed from other databases.

    According to the standard, the first two characters of an error code
    denote a class of errors, while the last three characters indicate a
    specific condition within that class. Thus, an application that does not
    recognize the specific error code can still be able to infer what to do
    from the error class.


.. seealso:: `PostgreSQL Error Codes table`__

    .. __: http://www.postgresql.org/docs/8.4/static/errcodes-appendix.html#ERRCODES-TABLE


.. todo:: errors table


